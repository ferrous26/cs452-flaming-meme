
#ifndef __NAME_SERVER_H__
#define __NAME_SERVER_H__

#include <std.h>

#define NAME_MAX_SIZE 8
#define NAME_OVERLAY_SIZE (NAME_MAX_SIZE/WORD_SIZE)
#define NAME_MAX      32

void __attribute__ ((noreturn)) name_server(void);

int WhoIs(char* name);
int WhoTid(int tid, char* name);
int RegisterAs(char* name);


#ifdef ASSERT
char* kWhoTid(int tid);
#endif

#endif
