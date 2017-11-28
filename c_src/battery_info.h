#ifndef _BATTERY_INFO_H_
#define _BATTERY_INFO_H_

/**
 * Copyright (c) 2014 Peter Leese
 *
 * Licensed under the GPL License. See LICENSE file in the project root for full license information.  
 */

#include <stdbool.h>

class BatteryInfo
{
private:
    int m_present;
    bool m_charging;
    bool m_discharging;
    float m_max_capacity;
    float m_last_full_capacity;
    float m_current_capacity;
    float m_min_capacity;
    float m_volts;
    float m_rate;
    const char * m_name;

    void read_type();
    void read_charging_state();
    void read_voltage();
    void read_capacity();
    void read_rate();
public:
    BatteryInfo(const char * name);
    bool is_present() const {return m_present;};
    bool is_discharging() const {return m_discharging;};
    bool is_charging() const {return m_charging;};
    void check_battery();
    int calc_left(float min) const;
    int calc_fullness(float min) const;
    int calc_next_period(float min) const;
    void print_self() const;
    void open_database() const;
};

#endif
