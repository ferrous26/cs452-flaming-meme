
#include <vt100.h>
#include <syscall.h>

#include <tasks/clock_server.h>

#include <tasks/train_control.h>
#include <tasks/mission_control.h>
#include <tasks/sensor_farm.h>

#include <tasks/calibrate.h>

void velocitate() {

    int tid = 0;
    int train_num = 0;
    Receive(&tid, (char*)&train_num, sizeof(train_num));
    Reply(tid, NULL, 0);

    update_turnout(1, 'S');
    update_turnout(2, 'S');
    update_turnout(3, 'C');
    update_turnout(4, 'S');
    update_turnout(5, 'C');
    update_turnout(6, 'S');
    update_turnout(7, 'S');
    update_turnout(8, 'C');
    update_turnout(9, 'S');
    update_turnout(10, 'S');
    update_turnout(11, 'C');
    update_turnout(12, 'C');
    update_turnout(13, 'S');
    update_turnout(14, 'S');
    update_turnout(15, 'S');
    update_turnout(16, 'S');
    update_turnout(17, 'S');
    update_turnout(18, 'C');
    update_turnout(153, 'C');
    update_turnout(154, 'S');
    update_turnout(155, 'C');
    update_turnout(156, 'S');

    log("Please get train %d to sensor C9", train_num);

    // wait for train to get to right spot
    train_block_until(train_num, 'C', 9);
    // get it into the proper starting position
    train_reverse(train_num);
    train_set_speed(train_num, 14);
    Delay(800);
    train_set_speed(train_num, 10);
    train_reverse(train_num);
    train_set_speed(train_num, 8);

    int start;
    int curved;
    int straight;

    // GO GO GO!
    for (int speed = 14; speed; speed--) {
        train_set_speed(train_num, speed);

        for (int count = 0; count < 2; count++) {
            train_block_until(train_num, 'C', 9);
            start = Time();

            train_block_until(train_num, 'B', 15);
            curved = Time() - start;

            train_block_until(train_num, 'C', 13);
            start = Time();

            train_block_until(train_num, 'E', 7);
            straight = Time() - start;

            log("%d,%d,%d,%d", train_num, speed, curved, straight);
            if (speed == 1) break;
        }
    }

    for (int speed = 2; speed < 15; speed++) {
        train_set_speed(train_num, speed);

        for (int count = 0; count < 2; count++) {
            train_block_until(train_num, 'C', 9);
            start = Time();

            train_block_until(train_num, 'B', 15);
            curved = Time() - start;

            train_block_until(train_num, 'C', 13);
            start = Time();

            train_block_until(train_num, 'E', 7);
            straight = Time() - start;

            log("%d,%d,%d,%d", train_num, speed, curved, straight);
        }
    }
}

void accelerate() {

    int tid = 0;
    int train_num = 0;
    Receive(&tid, (char*)&train_num, sizeof(train_num));
    Reply(tid, NULL, 0);

    const int adjust_speed = 2;

    reset_train_state();
    update_turnout(8,  'S');
    update_turnout(9,  'S');
    update_turnout(11, 'S');
    update_turnout(12, 'S');
    update_turnout(14, 'S');
    update_turnout(16, 'S');
    update_turnout(17, 'S');

    log("Please get train %d to sensor E9", train_num);
    train_block_until(train_num, 'E', 9);
    train_set_speed(train_num, 2);

    int speed = 2;
    for (; speed < 5; speed += 2) {
        // in position, so stop!
        train_block_until(train_num, 'E', 8);
        train_set_speed(train_num, 0);
        Delay(400); // wait for full stop (estimate)

        // start the test
        train_set_speed(train_num, speed);
        Delay(600);

        // done, so reset
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        train_block_until(train_num, 'E', 7);
        train_set_speed(train_num, 4);
        train_reverse(train_num);
        train_set_speed(train_num, adjust_speed);
    }

    for (; speed < 13; speed += 2) {
        // in position, so stop!
        train_block_until(train_num, 'E', 8);
        train_set_speed(train_num, 0);
        Delay(400); // wait for full stop (estimate)

        // start the test
        train_set_speed(train_num, speed);
        train_block_until(train_num, 'C', 14);

        // done, so reset
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        train_block_until(train_num, 'E', 7);
        train_set_speed(train_num, 4);
        train_reverse(train_num);
        train_set_speed(train_num, adjust_speed);
    }

    train_reverse(train_num);
    train_set_speed(train_num, 8);
    train_block_until(train_num, 'D', 9);
    train_reverse(train_num);

    for (speed = 12; speed > 1; speed -= 2) {
        train_set_speed(train_num, speed);
        train_block_until(train_num, 'E', 8);

        train_set_speed(train_num, 0);
        Delay(600);

        train_set_speed(train_num, 1);
        train_reverse(train_num);
        train_set_speed(train_num, 10);
        train_block_until(train_num, 'D', 9);

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

        train_block_until(train_num, 'E', 8);
        int time1 = Time();

        train_block_until(train_num, 'C', 14);
        time1 = Time() - time1;

        train_reverse(train_num);

        train_block_until(train_num, 'C', 13);
        int time2 = Time();

        train_block_until(train_num, 'E', 7);
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
