
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#define SYS_CREATE 1
#define SYS_TID    2
#define SYS_PTID   3
#define SYS_PASS   4
#define SYS_EXIT   5

#define SWI_HANDLER ((void**)0x28)

void* syscall_enter(void);                  /* found in context.asm */
int   syscall_handle(uint32 code, uint32 *sp);

int Create(int priority, void (*code) (void));

int myTid(void);
int myParentTid(void);

void Pass(void);
void Exit(void);

#endif

