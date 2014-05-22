#include <vt100.h>
#include <syscall.h>

#include <tasks/a1_task.h>
#include <tasks/pass_test.h>
#include <tasks/memcpy_bench.h>
#include <tasks/task_launcher.h>

inline static void print_help() {
    vt_log( "\t1 - Kernel 1 Test Task" );
    vt_log( "\tp - Pass Test Benchmark Task" );
    vt_log( "\tm - memcpy Benchmark Task" );
    vt_log( "\th - Print this Help Message" );
    vt_log( "\tq - Quit" );
    vt_flush();
}

void task_launcher() {
    for(;;) {
        vt_log( "Welcome to Task Launcher (h for help)" );
        vt_flush();

        while( !vt_can_get() ) {
	    vt_read();
	    Pass();
	}
        switch(vt_getc()) {
        case 'q':
            Exit();
            break;
        case '1':
            Create(4, a1_task);
            break;
	case 'p':
	    Create(4, pass_test);
	    break;
	case 'm':
	    Create(4, memcpy_bench);
	case 'h':
        default:
	    print_help();
	    break;
        }
    }
}
