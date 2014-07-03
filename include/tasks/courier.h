
#ifndef __COURIER_H__
#define __COURIER_H__

typedef struct {
    int   receiver;
    char* message;
    int   size;
} courier_package;

void sensor_notifier(void);
void courier(void);

#endif

