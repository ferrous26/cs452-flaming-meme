
#ifndef __COURIER_H__
#define __COURIER_H__

typedef struct {
    int   receiver;
    char* message;
    int   size;
} courier_package;

void courier(void);

#endif

