
#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__

#define TRACK_RESERVATION_NAME "RESERVE"

#define DIR_FORWARD 0
#define DIR_REVERSE 1

void __attribute__((noreturn)) track_reservation(void);

int reserve_who_owns(const track_node* const node, const int dir);
int reserve_section(const track_node* const node, const int dir, const int train);
int reserve_release(const track_node* const node, const int dir, const int train);

int reserve_who_owns_term(const int node_index, const int dir);
int reserve_section_term(const int node_index, const int dir, const int train);
int reserve_release_term(const int node_index, const int dir, const int train);

#endif

