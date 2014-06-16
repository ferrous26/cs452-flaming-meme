
#define TRAIN_ALL_START      0x60
#define TRAIN_ALL_STOP       0x61

#define TRAIN_REVERSE        0x0f
#define TRAIN_LIGHTS         0x10

#define TRAIN_HORN           68
#define TRAIN_FUNCTION_OFF   64

// train speed is from 0 to 14
// train number follows the command
// similar pattern for turnouts

#define TURNOUT_CLEAR        32 // do not forgot to reset the solenoid
#define TURNOUT_STRAIGHT     33
#define TURNOUT_CURVED       34
#define TURNOUT_TOTAL        22

#define SENSOR_RESET         192
#define SENSOR_POLL          133
