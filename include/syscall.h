#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <std.h>

/**
 * The notes very clearly state that we want address 0x08. If that value
 * is wrong then something is euchered or we should tell/ask the professor
 * about the problem.
 *
 * http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/s14/notes/l05.html
 */
#define SWI_HANDLER ((void**)0x28)

#define SYS_CREATE 1
#define SYS_TID    2
#define SYS_PTID   3
#define SYS_PASS   4
#define SYS_EXIT   5

void kernel_enter(unsigned int code);  /* found in context.asm */
int  kernel_exit(unsigned int *sp);    /* found in context.asm */
int  syscall_handle(uint32 code, uint32 *sp);

int Create(int priority, void (*code) (void));

int myTid(void);
int myParentTid(void);

void Pass(void);
void Exit(void);

#endif
