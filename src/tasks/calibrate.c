
#include <vt100.h>
#include <syscall.h>

#include <tasks/calibrate.h>
#include <tasks/clock_server.h>
#include <tasks/train_station.h>
#include <tasks/mission_control.h>


void accelerate() {

    int tid = 0;
    int train_num = 0;
    Receive(&tid, (char*)&train_num, sizeof(train_num));
    Reply(tid, NULL, 0);

    int adjust_speed = train_num == 51 ? 5 : 2;

    reset_train_state();
    update_turnout(8,  'S');
    update_turnout(9,  'S');
    update_turnout(11, 'S');
    update_turnout(12, 'S');
    update_turnout(14, 'S');
    update_turnout(16, 'S');
    update_turnout(17, 'S');

    log("Please get train %d to sensor E9", train_num);
    delay_sensor('E', 9);

    for (int speed = 10; speed; speed--) {
        train_set_speed(train_num, speed);
        Delay(30);
    }

    train_set_speed(train_num, adjust_speed);

    int speed = 1;
    for (; speed < 5; speed++) {
        // in position, so stop!
        delay_sensor('E', 8);
        train_set_speed(train_num, 0);
        Delay(200); // wait for full stop (estimate)

        // start the test
        train_set_speed(train_num, speed);
        Delay(400);

        // done, so reset
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        delay_sensor('E', 7);
        train_set_speed(train_num, 4);
        train_reverse(train_num);
        train_set_speed(train_num, adjust_speed);
    }

    for (; speed < 15; speed++) {
        // in position, so stop!
        delay_sensor('E', 8);
        train_set_speed(train_num, 0);
        Delay(200); // wait for full stop (estimate)

        // start the test
        train_set_speed(train_num, speed);
        delay_sensor('C', 14);

        // done, so reset
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        delay_sensor('E', 7);
        train_set_speed(train_num, 4);
        train_reverse(train_num);
        train_set_speed(train_num, adjust_speed);
    }

    train_reverse(train_num);
    train_set_speed(train_num, 8);
    delay_sensor('D', 9);
    train_reverse(train_num);

    for (speed = 14; speed > 1; speed--) {
        train_set_speed(train_num, speed);
        delay_sensor('E', 8);

        train_set_speed(train_num, 0);
        Delay(600);

        train_set_speed(train_num, 1);
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        delay_sensor('D', 9);

        train_reverse(train_num);
    }

    train_set_speed(train_num, 0);

    log("All done!");
}

void calibrate() {

    int tid = 0;
    int train_num = 49;
    Receive(&tid, (char*)&train_num, sizeof(train_num));
    Reply(tid, NULL, 0);

    reset_train_state();
    update_turnout(9,  'S');
    update_turnout(14, 'S');
    update_turnout(16, 'S');
    update_turnout(17, 'S');
    update_turnout(11, 'S');
    update_turnout(12, 'S');

    int times[14*2];
    int t_insert  = 0;

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
