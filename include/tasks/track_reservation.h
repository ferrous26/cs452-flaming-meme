
#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__

#define TRACK_RESERVATION_NAME "RESERVE"

void __attribute__((noreturn)) track_reservation(void);

int reserve_who_owns(const track_node* const node, const int dir);
int reserve_section(const track_node* const node, const int dir, const int train);

#endif

