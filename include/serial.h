#ifndef __SERIAL_H__
#define __SERIAL_H__

#define Putc(channel, ch) Putc_##channel(ch)
#define Getc(channel) Getc_##channel

#endif
