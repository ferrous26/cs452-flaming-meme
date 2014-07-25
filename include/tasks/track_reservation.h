
#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__

#define TRACK_RESERVATION_NAME "RESERVE"

#define DIR_FORWARD 0
#define DIR_REVERSE 1

void __attribute__((noreturn)) track_reservation(void);
int  __attribute__((pure)) get_reserve_length(const track_node* const node);

int reserve_who_owns(const track_node* const node);
int reserve_can_double(const track_node* const node, const int train);
int reserve_section(const int train,
                    const track_node** const node,
                    const int cnt);

int reserve_who_owns_term(const int node_index);
int reserve_section_term(const int node_index, const int train);

#endif

