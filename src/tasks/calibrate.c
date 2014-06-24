
#include <vt100.h>
#include <syscall.h>

#include <tasks/calibrate.h>
#include <tasks/clock_server.h>
#include <tasks/train_station.h>
#include <tasks/mission_control.h>

void calibrate() {
    int times[14*2];
    int t_insert  = 0;
    
    
    int train_num = 49;

    reset_train_state();
    update_turnout(9,  'S');
    update_turnout(14, 'S');
    update_turnout(16, 'S');
    update_turnout(17, 'S');
    update_turnout(11, 'S');
    update_turnout(12, 'S');

    for (int speed = 14; speed > 0; speed--) {
        train_set_speed(train_num, speed);

        delay_sensor('E', 8);
        int time1 = Time();

        delay_sensor('C', 14);
        time1 = Time() - time1;

        train_reverse(train_num);

        delay_sensor('C', 13);
        int time2 = Time();

        delay_sensor('E', 7);
        time2 = Time() - time2;

        train_reverse(train_num);
        log("Speed %d T1:%d T2:%d", speed, time1, time2);
    
        times[t_insert++] = time1;
        times[t_insert++] = time2;
    }

    for (int i = 13*2; i >= 0; i -= 2) {
        log("Speed %d T1:%d T2:%d", (i>>1)+1, times[i], times[i+1]);
    }
}



