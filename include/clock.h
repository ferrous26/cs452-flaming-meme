
#ifndef __CLOCK_H__
#define __CLOCK_H__

int clock_init( void );
int clock_get( unsigned int *mins, unsigned int *secs, unsigned int *tens );
int clock_poll( void );

void clock_t4enable( void )        { return; }
unsigned int clock_t4tick( void )  { return 1; }


#endif

