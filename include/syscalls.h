
#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

int Create( int priority, void (*code) (void) );

int myTid(void);
int myParentTid(void);

void Pass(void);
void Exit(void);

#endif

