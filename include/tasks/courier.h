
#ifndef __COURIER_H__
#define __COURIER_H__

typedef struct {
    int   receiver;
    char* message;
    int   size;
} courier_package;

typedef enum {
    DELAY_RELATIVE,
    DELAY_ABSOLUTE,
} delay_type;

typedef struct {
    delay_type type;
    int        ticks;
} tnotify_header;

typedef struct {
    int        receiver;
    delay_type type;
    int        ticks;
} tdelay_header;

void sensor_notifier(void);
void time_notifier(void);

void courier(void);

#endif

