
#ifndef __NAME_SERVER_H__
#define __NAME_SERVER_H__

#include <std.h>

#define NAME_MAX_SIZE 8
#define NAME_OVERLAY_SIZE (NAME_MAX_SIZE/WORD_SIZE)
#define NAME_MAX      32

extern int name_server_tid;

typedef enum {
    REGISTER,
    LOOKUP
} ns_req_type;

typedef union {
    char text[NAME_MAX_SIZE];
    uint32 overlay[NAME_OVERLAY_SIZE];
} ns_payload;

typedef struct {
    ns_req_type type;
    ns_payload payload;
} ns_req;

void name_server(void);

#endif
