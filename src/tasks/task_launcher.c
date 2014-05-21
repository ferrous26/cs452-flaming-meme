#include <io.h>
#include <debug.h>
#include <syscall.h>

#include <tasks/a1_task.h>
#include <tasks/pass_test.h>
#include <tasks/task_launcher.h>

inline static void print_help() {
    debug_log( "\t1 - Kernel 1 Test Task" );
    debug_log( "\tp - Pass Test Benchmark Task" );
    debug_log( "\th - Print this Help Message" );
    debug_log( "\tq - Quit" );
    vt_flush();
}

void task_launcher() {
    for(;;) {
        debug_log( "Welcome to Task Launcher (h for help)" );
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
	case 'h':
        default:
	    print_help();
	    break;
        }
    }
}

