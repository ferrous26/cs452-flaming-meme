
#ifndef __TASK_PRIORITY_H__
#define __TASK_PRIORITY_H__

#include <syscall.h>

#define TASK_LAUNCHER_PRIORITY      (TASK_PRIORITY_IDLE + 1)

#define CLOCK_SERVER_PRIORITY        TASK_PRIORITY_MEDIUM_HIGH
#define NAME_SERVER_PRIORITY         TASK_PRIORITY_MEDIUM_HIGH + 1

#define TERM_SERVER_PRIORITY         TASK_PRIORITY_MEDIUM
#define TRAIN_SERVER_PRIORITY        TASK_PRIORITY_MEDIUM
#define MISSION_CONTROL_PRIORITY    (TASK_PRIORITY_MEDIUM - 1)
#define TRAIN_BLASTER_PRIORITY      (TASK_PRIORITY_MEDIUM - 2)
#define TRAIN_MASTER_PRIORITY       (TASK_PRIORITY_MEDIUM - 3)
#define TRAIN_TRACK_ADMIN_PRIORITY  (TASK_PRIORITY_MEDIUM - 4)
#define TRAIN_CONSOLE_PRIORITY       4 // WTF
#define TRAIN_COURIER_PRIORITY       4 // WTF

#endif
