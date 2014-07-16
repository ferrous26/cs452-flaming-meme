
#ifndef __TASK_PRIORITY_H__
#define __TASK_PRIORITY_H__

#include <syscall.h>

/* A list of all static task priorities */
#define NAME_SERVER_PRIORITY        (TASK_PRIORITY_MEDIUM_HIGH + 1)
#define TASK_LAUNCHER_PRIORITY      (TASK_PRIORITY_IDLE + 1)

/* All of the IO Priorities */
#define IO_RECEIVE_NOTE_PRIORITY    (TASK_PRIORITY_HIGH + 1)
#define IO_SEND_CARRIER_PRIORITY     TASK_PRIORITY_HIGH

#define TERM_SERVER_PRIORITY         TASK_PRIORITY_MEDIUM_HIGH
#define TRAIN_SERVER_PRIORITY        TASK_PRIORITY_MEDIUM_HIGH

/* Clock Server Priority */
#define CLOCK_SERVER_PRIORITY       (TASK_PRIORITY_MEDIUM_HIGH+1)
#define CLOCK_NOTE_PRIORITY         (TASK_PRIORITY_HIGH + 2)


#define MISSION_CONTROL_PRIORITY    (TASK_PRIORITY_MEDIUM - 1)
#define SENSOR_FARM_PRIORITY         TASK_PRIORITY_MEDIUM

#define TRAIN_BLASTER_PRIORITY      (TASK_PRIORITY_MEDIUM - 2)
#define TRAIN_TRACK_ADMIN_PRIORITY  (TASK_PRIORITY_MEDIUM - 4)
#define TRAIN_MASTER_PRIORITY        TASK_PRIORITY_MEDIUM_LOW

#define TRAIN_COURIER_PRIORITY       4 // WTF
#define TRAIN_CONSOLE_PRIORITY      TASK_PRIORITY_MEDIUM
#define TC_CHILDREN_PRIORITY        (TASK_PRIORITY_MEDIUM_LOW + 1)


#define TIMER_COURIER_PRIORITY       TASK_PRIORITY_HIGH
#define SENSOR_POLL_PRIORITY        (TASK_PRIORITY_MEDIUM_HIGH - 1)

// PATH Calculation Priorities
#define PATH_ADMIN_PRIORITY          TASK_PRIORITY_MEDIUM
#define PATH_WORKER_HI_PRIORITY      TASK_PRIORITY_MEDIUM_LOW
#define PATH_WORKER_MED_PRIORITY     TASK_PRIORITY_LOW
#define PATH_WORKER_LOW_PRIORITY    (TASK_PRIORITY_LOW - 2)

#endif
