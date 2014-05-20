#include <io.h>
#include <debug.h>

#include <syscall.h>
#include <bootstrap.h>

void task_launcher() {
    while(1) {
	debug_log( "Welcome to Task Launcher!!" );
	vt_flush();
	
	while( !vt_can_get() ) {
		vt_read();
		Pass();
	}
        switch( vt_getc() ) {
	case 'q':
	    Exit();
	    break;
        case '1':
            Create( 4, bootstrap ); 
            break;
        default:
	    break;
	}
    }
}

