#ifndef _BATTERY_INFO_H_
#define _BATTERY_INFO_H_

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
public:
    BatteryInfo();
    bool is_present() const {return m_present;};
    bool is_discharging() const {return m_discharging;};
    void check_battery(const char * name);
    int calc_left(float min) const;
    int calc_fullness(float min) const;
    int calc_next_period(float min) const;
    void print_self() const;
    void open_database() const;
};

#endif
