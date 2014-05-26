#include <io.h>
#include <debug.h>

#include <std.h>
#include <syscall.h>

#include <tasks/name_server.h>

static int __attribute__((const)) find_loc(char names[][NAME_MAX_SIZE],
                                           const char value[NAME_MAX_SIZE],
                                           const int stored ) {
    for(int i = 0; i < stored; i++) {
        for(int j = 0;;j++) {
	    if(j == NAME_MAX_SIZE) return i;
            if(names[i][j] != value[j]) break;
	    if(value[j] == '\0') return i;
	}
    }
    return -1;
}

void name_server() {
    int32 tasks[NAME_MAX];
    union {
        char   name[NAME_MAX][NAME_MAX_SIZE];
    	uint32 overlay[NAME_MAX][NAME_OVERLAY_SIZE];
    } lookup;

    int reply;
    int32 tid;
    ns_req buffer;
    uint insert = 0;

    for(;;) {
        Receive(&tid, (char*)&buffer, sizeof(ns_req));
        int loc = find_loc(lookup.name, buffer.payload.text, insert);

        switch(buffer.type) {
        case REGISTER:
            reply = 0;
	    if(loc < 0) {
		if(insert < NAME_MAX) {
		    lookup.overlay[insert][0] = buffer.payload.overlay[0];
		    lookup.overlay[insert][1] = buffer.payload.overlay[1];
		    tasks[insert++] = tid;
	        } else {
                    reply = -3;
	        }
	    } else {
                tasks[loc] = tid;
	    }
            break;
        case LOOKUP:
            reply = loc < 0 ? -3 : loc; 
	    break;
	default:
	    debug_log("Invalid Request from %d, Ignoring....", tid);
	    vt_flush();
	    reply = -42;
	}

        Reply(tid, (char*)&reply, sizeof(reply));
    }
}

