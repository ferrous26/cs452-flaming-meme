
#ifndef __NAME_SERVER_H__
#define __NAME_SERVER_H__

#define NAME_MAX_SIZE 8
#define NAME_OVERLAY_SIZE (NAME_MAX_SIZE/4)
#define NAME_MAX      32

typedef enum {
    REGISTER,
    LOOKUP
} ns_req_type;

typedef struct {
    ns_req_type type;
    union {
        char text[NAME_MAX_SIZE];
	uint32 overlay[NAME_OVERLAY_SIZE];
    } payload;
} ns_req;


void name_server(void);

#endif

