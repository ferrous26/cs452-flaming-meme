/* THIS FILE IS GENERATED CODE -- DO NOT EDIT */
#ifndef __TRACK_DATA_H__
#define __TRACK_DATA_H__

#include <track_node.h>
#include <std.h>

// The track initialization functions expect an array of this size.
#define TRACK_MAX 144

#define NUM_TURNOUTS           22
#define NUM_SENSOR_BANKS       5
#define NUM_SENSORS_PER_BANK   16
#define NUM_SENSORS           (NUM_SENSOR_BANKS * NUM_SENSORS_PER_BANK)

void init_tracka(track_node* const track) TEXT_COLD;
void init_trackb(track_node* const track) TEXT_COLD;

#endif
