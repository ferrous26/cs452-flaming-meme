
#include <syscall.h>

int Create( int priority, void (*code) () ) {
	(void) priority;
	(void) code;

	return 11;
}

int myTid( ) {

	return 12;
}

int myParentTid( ) {

	return 13;
}

void Pass( ) {

}

void Exit( ) {

}

