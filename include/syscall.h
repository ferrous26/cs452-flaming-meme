
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

void* int_entry();

int Create( int priority, void (*code) (void) );

int myTid(void);
int myParentTid(void);

void Pass(void);
void Exit(void);

#endif

