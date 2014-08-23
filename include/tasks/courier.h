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

void time_notifier(void);
void one_time_courier(void);
void delayed_one_way_courier(void);
void async_courier(void);
void courier(void);

#endif
