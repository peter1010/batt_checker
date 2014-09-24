#ifndef _BATTERY_INFO_H_
#define _BATTERY_INFO_H_

#include <stdbool.h>

struct BatteryInfo_s
{
    int present;
    bool charging;
    bool discharging;
    float max_capacity;
    float last_full_capacity;
    float current_capacity;
    float min_capacity;
    float volts;
    float rate;
};

typedef struct BatteryInfo_s BatteryInfo_t;

#endif
