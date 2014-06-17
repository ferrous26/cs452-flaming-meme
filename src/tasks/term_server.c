#include <tasks/term_server.h>
#include <std.h>
#include <vt100.h>
#include <debug.h>
#include <syscall.h>
#include <scheduler.h>
#include <circular_buffer.h>

// forward declarations
struct term_state;
struct term_puts;
static void _term_try_send(struct term_state* const state);

#define UART_FIFO_SIZE     16

#define INPUT_BUFFER_SIZE  512
#define OUTPUT_BUFFER_SIZE 128
#define OUTPUT_BUFFER_MAX  (OUTPUT_BUFFER_SIZE - 2) // save space for XOFF/XON
#define OUTPUT_Q_SIZE      TASK_MAX // at least 8 tasks will never Puts

// ASCII byte that tells VT100 to stop transmitting
#define XOFF               17
#define XOFF_THRESHOLD     OUTPUT_BUFFER_SIZE

// ASCII byte that tells VT100 to start transmitting (only send after XOFF)
#define XON                19
#define XON_THRESHOLD     (OUTPUT_BUFFER_SIZE >> 1)


// Information stored when a Puts request must block
typedef struct {
    int   tid;
    int   length;
    char* string;
} term_puts;

// Circular buffer for blocked Puts requests
typedef struct {
    term_puts* head;
    term_puts* tail;
    term_puts* end;
    uint       count;
    term_puts  buffer[OUTPUT_Q_SIZE];
} puts_buffer;

// State for the terminal server
struct term_state {
    char* out_head;   // pointer where to insert data for output
    int   obuffer_size;
    char* obuffer;
    char  obuffers[2][OUTPUT_BUFFER_SIZE];

    puts_buffer output_q;

    char_buffer input_q;
    char ibuffer[INPUT_BUFFER_SIZE];
    char* recv_buffer;

    int   in_tid;     // tid for the Getc caller that was blocked
    int   carrier;    // tid for the carrier when it gets blocked
    int   notifier;   // tid for the notifier when it makes a request
    bool  xoff;       // whether we sent the XOFF bit in the last run
    bool  xon;        // whether we sent the XON bit in the last run
};



/* Global data */

int term_server_tid;



/* Circular buffer implementations */

static inline void pbuf_init(puts_buffer* const cb) {
    cb->head  = cb->tail = cb->buffer;
    cb->end   = cb->buffer + OUTPUT_Q_SIZE; // first address past the end
    cb->count = 0;
    memset(cb->buffer, 0, sizeof(cb->buffer));
}

static inline uint pbuf_count(const puts_buffer* const cb) {
    return cb->count;
}

static inline void pbuf_produce(puts_buffer* const cb, term_puts* const puts) {
    memcpy(cb->head, puts, sizeof(term_puts));

    cb->count = mod2(cb->count + 1, OUTPUT_Q_SIZE);
    if (++cb->head == cb->end)
	cb->head = cb->buffer;
}

static inline term_puts* pbuf_consume(puts_buffer* const cb) {

    assert(cb->count, "Trying to consume from empty puts queue");

    term_puts* const ptr = cb->tail++;

    cb->count--;
    if (cb->tail == cb->end)
	cb->tail = cb->buffer;

    return ptr;
}



/* Static methods for the terminal server */


// There are some checks that we want to make that should always be used
// so we have a special assertion function that doesn't disappear during
// a release build
#define TERM_ASSERT(expr, var) if (!(expr))  term_failure(var, __LINE__)
static void __attribute__ ((noreturn)) term_failure(int result, uint line) {
    klog("Action failed in terminal server at line %u (%d)", line, result);
    Shutdown();
}

// data coming from the UART
static void __attribute__ ((noreturn)) recv_notifier() {
    int ptid = myParentTid(); // the term server

    // buffer of input
    char input[UART_FIFO_SIZE];

    term_req msg = {
        .type   = IBUFFER,
        .length = UART_FIFO_SIZE,
	.string = input
    };

    // first, let the server know about the input buffer so that we don't need
    // to keep sending the pointer each time we notify the server of input
    int result = Send(ptid,
		      (char*)&msg,  sizeof(msg),
		      (char*)input, sizeof(input));
    TERM_ASSERT(result == 0, result);

    // now, we can keep filling the buffer and sending over only the size
    // with the notification
    msg.type = NOTIFIER;

    FOREVER {
        msg.length = AwaitEvent(UART2_RECV, (char*)input, sizeof(input));
        result     = Send(ptid,
			  (char*)&msg, sizeof(term_req_type) + sizeof(int),
			  NULL, 0);
	TERM_ASSERT(result == 0, result); // we should never get anything back
    }
}

// handle the initial message from the receive notifier
static inline void _term_ibuffer(struct term_state* const state,
				 term_req* const req) {

    state->recv_buffer = req->string;

    int result = Reply(state->notifier, NULL, 0);
    TERM_ASSERT(result == 0, result);
}

// data going to the UART
static void __attribute__ ((noreturn)) send_carrier() {
    int ptid = myParentTid(); // the term server

    struct {
	char* one;
	char* two;
    } buffers;

    term_req msg = {
        .type = OBUFFER
    };

    // first, ask the server for the buffer locations so that we can avoid
    // having to send the buffer pointers over and over again
    int result = Send(ptid,
		      (char*)&msg,     sizeof(term_req_type),
		      (char*)&buffers, sizeof(buffers));
    TERM_ASSERT(result == sizeof(buffers), result);

    // now, we can just keep swapping between buffers every time that we
    // send data, thus avoiding one copy of the buffer and the need to
    // send the buffer pointers, all we need is the size
    msg.type = CARRIER;
    char* buffer = buffers.two;

    FOREVER {
	// blocked until a buffer is ready to go
        result = Send(ptid,
		      (char*)&msg,        sizeof(term_req_type),
		      (char*)&msg.length, sizeof(int));
	TERM_ASSERT(result == sizeof(int), result);

	assert(msg.length > 0 && msg.length <= 128,
	       "BAD CARRIER SIZE MOTHER FUCKER %d", msg.length);

	// switch to next buffer
	if (buffer == buffers.two)
	    buffer = buffers.one;
	else
	    buffer = buffers.two;

	// now, pump the buffer through to the kernel
	int chunk = 0;
	while (msg.length) {
	    result = AwaitEvent(UART2_SEND, buffer + chunk, msg.length);
	    TERM_ASSERT(result > 0, result);

	    chunk      += result;
	    msg.length -= result;
	}
    }
}

// handle the init message from the server
static void _term_obuffer(struct term_state* const state) {

    // need to send back the pointers to the obuffers
    struct {
	char* one;
	char* two;
    } buffers = {
	.one = state->obuffers[0],
	.two = state->obuffers[1]
    };

    int result = Reply(state->carrier, (char*)&buffers, sizeof(buffers));
    TERM_ASSERT(result == 0, result);

    state->carrier = -1;
}

static void _term_try_puts(struct term_state* const state,
			   term_puts* const puts) {

    // if we have space in the buffer (leave space for XOFF/XON)
    if ((state->obuffer_size + puts->length) <= OUTPUT_BUFFER_MAX) {
	// then just add it
	memcpy(state->out_head, puts->string, puts->length);

	// and let the caller go back on their merry way
	int result = Reply(puts->tid, NULL, 0);
	TERM_ASSERT(result == 0, result);

	// move these up so we don't overwrite things...
	state->out_head     += puts->length;
	state->obuffer_size += puts->length;

	// if the carrier happens to be waiting
	if (state->carrier >= 0)
	    // then we can wake him up and send some stuff
	    _term_try_send(state);
    }
    // else, we need to block the task until we have space
    else {
	pbuf_produce(&state->output_q, puts);
    }
}

static void _term_send_xoff(struct term_state* const state) {
    assert(state->obuffer_size < OUTPUT_BUFFER_SIZE,
	   "No space in buffer for the stop byte!");
    *state->out_head++ = XOFF; // J-J-Jam it in
    state->xoff = false;
    state->obuffer_size++;
}

static void _term_send_xon(struct term_state* const state) {
    assert(state->obuffer_size < OUTPUT_BUFFER_SIZE,
	   "No space in buffer for the stop byte!");
    assert(state->xoff, "Haven't sent the xoff byte yet");
    *state->out_head++ = XON; // J-J-Jam it in
    state->xon = true;
    state->obuffer_size++;
}

static void _term_try_send(struct term_state* const state) {

    // if the carrier is waiting
    if (state->carrier >= 0 && state->obuffer_size) {

    	// then do carrier-send dance
    	int result = Reply(state->carrier,
    			   (char*)&state->obuffer_size,
    			   sizeof(uint));
    	TERM_ASSERT(result == 0, result);

    	// then do the buffer-swap dance
    	if (state->obuffer == state->obuffers[0])
    	    state->obuffer  = state->obuffers[1];
    	else
    	    state->obuffer  = state->obuffers[0];
    	state->out_head     = state->obuffer;
    	state->obuffer_size = 0;

    	// reset this now (before calling _term_try_puts)
    	state->carrier = -1;

    	// this is technically correct, but might actually be
    	// too premature...we may want a different heuristic
    	if (state->xon)
    	    state->xoff = state->xon = false;

    	// wake up tasks that were blocked trying to output;
    	// I am choosing to wake them all up because in all likelyhood
    	// all the waiting output will fit into the buffer, so we can
    	// skip checking if it will fit in this function; it will be
    	// checked in _term_try_puts, and in the worst case, the requests
    	// that do not fit will waste CPU time and get requeued
    	for (; pbuf_count(&state->output_q);)
    	    _term_try_puts(state, pbuf_consume(&state->output_q));
    }
}

static void _term_try_getc(struct term_state* const state) {

    // if we have a buffered byte
    if (cbuf_can_consume(&state->input_q)) {
	// then send it over the wire immediately
	char  byte = cbuf_consume(&state->input_q);
	int result = Reply(state->in_tid, &byte, 1);
	TERM_ASSERT(result == 0, result);

	// don't forget to reset this
	state->in_tid = -1;

	// NOTE: if we blocked the receiver for filling the buffer,
	// this is where we would want to unblock the receiver if
	// the buffer has been sufficiently emptied

	// if we have previously written the XOFF byte and not
	// written the XON byte, but we've consumed enough bytes
	// from the input buffer to allow more input
	if (state->xoff && !state->xon &&
	    cbuf_count(&state->input_q) < XON_THRESHOLD)
	    // then we should re-enable output
	    _term_send_xon(state);
    }
}

static void _term_recv(struct term_state* const state,
			      const int length) {

    // TODO: implement bulk produce for circular buffers
    for (int count = 0; count < length; count++)
	cbuf_produce(&state->input_q, *(state->recv_buffer + count));

    // reply right away in case there is already more crap waiting
    int result = Reply(state->notifier, NULL, 0);
    TERM_ASSERT(result == 0, result);

    // if we are past the fill threshold, then we need to send
    // the XOFF byte
    assert(cbuf_count(&state->input_q) < INPUT_BUFFER_SIZE,
	   "Filled the input buffer");

    if (!state->xoff &&
	cbuf_count(&state->input_q) > XOFF_THRESHOLD)
	_term_send_xoff(state);

    // if someone was waiting for a byte
    if (state->in_tid >= 0)
	_term_try_getc(state);
}

static void _startup() {

    term_server_tid = myTid();

    klog("Terminal Server started at %d", term_server_tid);

    int tid = Create(TASK_PRIORITY_HIGH, recv_notifier);
    TERM_ASSERT(tid > 0, tid);

    tid = Create(TASK_PRIORITY_HIGH, send_carrier);
    TERM_ASSERT(tid > 0, tid);
}


void term_server() {

    _startup();

    struct term_state state;

    // initialize this mofo
    memset(&state, 0, sizeof(state));
    state.out_head = state.obuffer = state.obuffers[0];
    state.obuffer_size = 0;
    pbuf_init(&state.output_q);
    cbuf_init(&state.input_q, INPUT_BUFFER_SIZE, state.ibuffer);
    state.in_tid = state.carrier = state.notifier = -1;
    state.xoff   = state.xon     = false;


    int tid = 0;
    term_req req;

    FOREVER {
#ifdef DEBUG
	memset(&req, 0, sizeof(req));
#endif
	int result = Receive(&tid, (char*)&req, sizeof(req));

	UNUSED(result);
	assert(req.type >= GETC && req.type <= IBUFFER,
	       "Invalid message type sent to terminal server (%d)", req.type);

        switch(req.type) {
	case GETC:
	    assert(state.in_tid == -1,
		   "Multiple tasks waiting for input: %d and %d",
		   state.in_tid, tid);
	    state.in_tid = tid;
	    _term_try_getc(&state);
	    break;

	case PUTS: {
	    term_puts puts = {
		.tid    = tid,
		.length = req.length,
		.string = req.string
	    };
	    _term_try_puts(&state, &puts);
	    break;
	}

	case CARRIER:
	    state.carrier = tid;
	    _term_try_send(&state);
	    break;

	case NOTIFIER:
	    _term_recv(&state, req.length);
	    break;

	case OBUFFER:
	    state.carrier = tid;
	    _term_obuffer(&state);
	    break;

	case IBUFFER:
	    state.notifier = tid;
	    _term_ibuffer(&state, &req);
	    break;
	}
    }
}
