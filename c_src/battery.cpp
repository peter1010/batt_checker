/**
 * Copyright (c) 2014 Peter Leese
 *
 * Licensed under the GPL License. See LICENSE file in the project root for full license information.  
 */

#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "battery_info.h"

#define SYS_PREFIX "/sys/class/power_supply"
#define CACHE_LOG  "/var/cache/batt_checker/data.log"
#define WORST_RATE "/var/cache/batt_checker/worst_discharge_rate"
#define MEAN_RATE  "/var/cache/batt_checker/mean_discharge_rate"

/**
 * Close all file descriptors accept stdin, stdout and stderr
 * Used after a fork() so that child only has access to the
 * standard IO
 */
static void close_all_fds(void)
{
    const int maxFd = sysconf(_SC_OPEN_MAX);
    int i;
    for(i = 3; i <= maxFd; i++) {
        close(i);
    }
}

/**
 * Redirect stdout & stderr to /dev/null, this is used to
 * detach from the controlling terminal
 */
static void redirect_out(void)
{
    const int fd_null_out = open("/dev/null", O_RDONLY);
    if(fd_null_out > 0) {
	dup2(fd_null_out, 1);
	dup2(fd_null_out, 2);

        close(fd_null_out);
    }
}

/**
 * Redirect stdin & from /dev/null, this is used to detach
 * from the controlling terminal
 */
static void redirect_in(void)
{
    const int fd_null_in = open("/dev/null", O_WRONLY);
    if(fd_null_in > 0) {
        dup2(fd_null_in, 0);
        close(fd_null_in);
    }
}
/**
 * Generate an alert dialog to indicate battery is getting low.
 * This is done by forking another process to do the alert.
 *
 * @param[in] left Estimated time left until battery is flat (in mins)
 * @param[in] app_argv The App plus args that does the alert dialog
 */
static void alert(int left, const char * app_argv[])
{
    printf("Alert Left =%i\n", left);

    const pid_t pid1 = fork();
    if(pid1 == 0) {     /* Child */
        /* Group child and any grand children under the same process group */
        if(setsid() < 0) {
            fprintf(stderr, "Failed to setsid\n");
        }

        redirect_out();
        redirect_in();
        close_all_fds();

        const pid_t pid2 = fork();
        if(pid2 == 0) {
            execvp(app_argv[0], const_cast<char **>(app_argv));
            exit(EXIT_FAILURE);
        }
        else {
            exit(EXIT_SUCCESS);
        }
    }
    else if(pid1 > 0) {
        int status;
        wait(&status);
    }
}


/**
 * Convert uW to W
 *
 * @param[in] value Value in micro-watts
 *
 * @return Conversion to Watts
 */
static float uwatts2watts(int value)
{
    return value / 1000000.0;
}

/**
 * Convert uWh to Joules
 *
 * @param[in] value Value in micro-watt-hours
 *
 * @return Conversion to Joules
 */
static float uwatthr2joules(int value)
{
    return (value * 60.0 * 60.0)/1000000.0;
}

/**
 * Convert uAh to Joules
 *
 * @param[in] value Value in micro-amp-hours
 * @param[in] volts The voltage
 *
 * @return Conversion to Joules
 */
static float uamphr2joules(int value, float volts)
{
    return uwatthr2joules(value * volts);
}

/**
 * Convert uV to V
 *
 * @param[in] value Value in micro-volts
 *
 * @return Conversion to Volts
 */
static float uvolts2volts(int value)
{
    return value / 1000000.0;
}

/**
 * read proc file system
 *
 * @param[in] base The battery info directory in the proc filesystem
 * @param[in] file The file in the base directory
 * @param[out] result The buffer to put contents of the file after reading it
 * @param[out] maxlen The maximum size of buffer
 *
 * @return The actual number of bytes place in result buffer
 */
static size_t read_sys(const char * base, const char * file, char * result, size_t maxlen) {
    size_t length = 0;
    char pathname[1024];
    snprintf(pathname, sizeof(pathname), "%s/%s/%s", SYS_PREFIX, base, file);
//    printf("pathname='%s'\n", pathname);
    const int f = open(pathname, O_RDONLY);
    if(f) {
        length = read(f, result, maxlen-1);
        while((length > 0) && (result[length-1] == '\n')) {
            length--;
        }
//        printf("reading..%u %u\n", length, maxlen);
//        perror("");
        close(f);
    }
    result[length] ='\0';
    return length;
}

/**
 * Convert value (a string) to a integer
 *
 * @param[in] value as a string
 *
 * @return as an integer
 */
static int to_int(const char * value)
{
    return strtol(value, NULL, 10);
}

/**
 * The BatteryInfo constructor
 */
BatteryInfo::BatteryInfo(const char * name)
{
    memset(this, 0, sizeof(*this));
    m_name = name;
}

/**
 * Read and refresh the is battery present flag
 */
void BatteryInfo::read_type()
{
    char result[256];
    int status = read_sys(m_name, "type", result, sizeof(result));
    if(status > 0) {
        if(strcasecmp(result, "mains") == 0) {
            m_present = false;
        }
        else if(strcasecmp(result, "battery") == 0) {
            status = read_sys(m_name, "present", result, sizeof(result));
            if(status > 0) {
                m_present = to_int(result) ? true : false;
            }
            else {
                m_present = false;
            }
        }
        else {
            fprintf(stderr, "Invalid type '%s'\n", result);
            m_present = false;
        }
    }
    else {
        m_present = false;
    }
}

/**
 * Convert text charging state value into two bool values
 */
void BatteryInfo::read_charging_state()
{
    char result[256];

    const int status = read_sys(m_name, "status", result, sizeof(result));
    if(status > 0) {
        if(strcmp(result, "Charging") == 0) {
            m_charging = true;
            m_discharging = false;
        }
        else if(strcmp(result, "Discharging") == 0) {
            m_discharging = true;
            m_charging = false;
        }
        else if( (strcmp(result, "Unknown") != 0)
                && (strcmp(result, "Full") != 0)) {
            fprintf(stderr, "State = '%s'", result);
            exit(EXIT_FAILURE);
        }
        else {
            m_charging= false;
            m_discharging = false;
        }
    }
}



void BatteryInfo::read_voltage()
{
    char result[256];
    int status = read_sys(m_name, "voltage_now", result, sizeof(result));
    if(status > 0) {
        m_volts = uvolts2volts(to_int(result));

        status = read_sys(m_name, "voltage_min_design", result,
                sizeof(result));
        if(status > 0) {
            float min_voltage = uvolts2volts(to_int(result));
            if(min_voltage > 100) {
                /* Some times volts are in pico-volts, err.. */
                min_voltage /= 1000;
            }
            if(m_volts < 0.5 * min_voltage) {
                printf("voltage reading is probably broken\n");
                m_volts = min_voltage;
            }
        }
    }
}

void BatteryInfo::read_capacity()
{
    char result[256];
    int status = read_sys(m_name, "energy_full", result, sizeof(result));
    if(status > 0) {
        m_last_full_capacity = uwatthr2joules(to_int(result));
    }
    else {
        status = read_sys(m_name, "charge_full", result, sizeof(result));
        if(status > 0) {
            m_last_full_capacity = uamphr2joules(to_int(result),
                    m_volts);
        }
    }

    status = read_sys(m_name, "energy_full_design", result, sizeof(result));
    if(status > 0) {
        m_max_capacity = uwatthr2joules(to_int(result));
    }
    else {
        status = read_sys(m_name, "charge_full_design", result,
                sizeof(result));
        if(status > 0) {
            m_max_capacity = uamphr2joules(to_int(result), m_volts);
        }
    }

    status = read_sys(m_name, "energy_now", result, sizeof(result));
    if(status > 0) {
        m_current_capacity = uwatthr2joules(to_int(result));
    }
    else {
        status = read_sys(m_name, "charge_now", result, sizeof(result));
        if(status > 0) {
           m_current_capacity = uamphr2joules(to_int(result),
                    m_volts);
        }
    }

    read_sys(m_name, "alarm", result, sizeof(result));
    if(status > 0) {
        m_min_capacity = uwatthr2joules(to_int(result));
    }
    else {
        m_min_capacity = uamphr2joules(to_int(result),
                m_volts);
    }
}

void BatteryInfo::read_rate()
{
    char result[256];
    int status = read_sys(m_name, "power_now", result, sizeof(result));
    if(status > 0) {
        m_rate = uwatts2watts(to_int(result));
    }
    else {
        read_sys(m_name, "current_now", result, sizeof(result));
        m_rate = uwatts2watts(to_int(result) * m_volts);
    }
}

/**
 * Fill in the BatterInfo_t structure with information read from the proc
 * filesystem about battery name
 *
 * @param[out] info The Battery status
 */
void BatteryInfo::check_battery()
{
    read_type();
    if(!is_present()) {
        return;
    }

    read_charging_state();
    read_voltage();
    read_capacity();
    read_rate();
}

/**
 * Estimate in mins until battery reaches minimum charge
 *
 * @param[in] info The battery status
 * @param[in] min The minimum charge (in Joules)
 *
 * @return estimated time in minutes
 */
int BatteryInfo::calc_left(float min) const
{
    if(is_discharging()) {
        float left = m_current_capacity - min;
        float mean_rate = m_rate;

        if(mean_rate <= 0.0001) {
            FILE * fp = fopen(MEAN_RATE,"r");
            if(fp) {
                char buf[512];
                int got = fread(buf, sizeof(buf), 1, fp);
                buf[got] = '\0';
                fclose(fp);
                mean_rate = strtof(buf, 0);
            }
        }
        return static_cast<int>(left / mean_rate / 60.0 + 0.5);
    }
    return 999;
}

/**
 * Calculate part/total as a percentage
 *
 * @param[in] part (the numerator)
 * @param[in] total (the denomator)
 *
 * @return percentage or -1 if out of range
 */
static int calc_percent(float part, float total)
{
    if(total > part) {
        return static_cast<int>(part*100/total + 0.5);
    }
    return -1;
}



int BatteryInfo::calc_fullness(float min) const
{
    return calc_percent(m_current_capacity - min,
                        m_last_full_capacity - min);
}

/**
 * Open the database that contains history of previous measurements
 * and calculate the worst case (minimum time) for when battery will
 * go below the min threshold.
 *
 * @param[in] info The battery status
 * @param[in] min The minimum charge (in Joules)
 *
 * @return estimated time in minutes
 */
int BatteryInfo::calc_next_period(float min) const
{
    float worst_rate = 15;
    float left = m_current_capacity - min;

    FILE * fp = fopen(WORST_RATE,"r");
    if(fp) {
        char buf[512];
        int got = fread(buf, sizeof(buf), 1, fp);
        buf[got] = '\0';
        fclose(fp);
        worst_rate = strtof(buf, 0);
    }
    if(worst_rate < m_rate) {
        worst_rate = m_rate;
    }
    return static_cast<int>(left / (60.0 * worst_rate) + 0.5);
}

/**
 * print info
 *
 * @param[in] info The battery status
 */
void BatteryInfo::print_self() const
{
    if(!is_present()) {
        printf("No battery\n");
        return;
    }

    printf("Max      =%10.1f J (%3i%%)\n", m_max_capacity, 100);
    printf("Last full=%10.1f J (%3i%%)\n", m_last_full_capacity,
            calc_percent(m_last_full_capacity, m_max_capacity));
    printf("Current  =%10.1f J (%3i%%)\n", m_current_capacity,
            calc_percent(m_current_capacity, m_max_capacity));
    printf("Min      =%10.1f J (%3i%%)\n", m_min_capacity,
            calc_percent(m_min_capacity, m_max_capacity));

    printf("volts=%.2f v\n", m_volts);

//    info->m_min_capacity = 0
    if(is_charging()) {
        float left = m_last_full_capacity - m_current_capacity;
        int minutes = static_cast<int>(left / m_rate / 60.0 + 0.5);
        printf("Charging rate=%f J/s\n", m_rate);
        printf( "%i mins left to reach last full\n", minutes);
    }
    if(is_discharging()) {
        printf("Discharging rate=%f J/s\n", m_rate);
        printf("%i mins left before flat\n", calc_left(0));
    }
}

/**
 * Write to the history database
 *
 * @param[in] info The battery status
 */
void BatteryInfo::open_database() const
{
    time_t real_time;
    struct timespec up_time;
    real_time = time(NULL);
    clock_gettime(CLOCK_MONOTONIC, &up_time);

    const char status = is_charging() ? '/' : is_discharging() ? '\\' : '-';

    FILE * fp = fopen(CACHE_LOG,"a");
    if(fp) {
        fprintf(fp,"%lu\t%lu\t%c\t%9.1f\t%.2f\n",
                  real_time, up_time.tv_sec, status, m_current_capacity, m_volts);
        fclose(fp);
    }
}

void signal_sock_listener(const char * sock, int fullness, bool alert)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path));
    addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
    addr.sun_family = AF_UNIX;

    const int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(fd >= 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%i %s\n", fullness, alert ? "ALERT" : "");

        const int length = sendto(fd, msg, strlen(msg), 0,
                reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
        if(length < 0) {
            perror("send");
        }
        else {
            printf("Signalling '%s' %i\n", sock, length);
        }
        close(fd);
    }
    else {
        printf("Failed to create unix socket\n");
    }
}

/**
 * Check all the batteries, if one is about to expire before low_threshold
 * Alert the user
 *
 * @param[in] argc Number of args for the alert program
 * @param[in] argv List of arguments for the alert program
 * @param[in] low_threshold In mins the threshold
 *
 * @return The time in mins whn we should check again
 */
static int check_batteries(int argc, const char * argv[], const char * sig_sock,
        int low_threshold)
{
    int left = 9999;
    int fullness = 100;
    int next_period = 9999;
    DIR * dir = opendir(SYS_PREFIX);
    if(dir) {
        struct dirent * entry;
        while((entry = readdir(dir))) {
            if(entry->d_name[0] != '.') {
                BatteryInfo info(entry->d_name);
                info.check_battery();
                if(info.is_present()) {
                    info.print_self();
                    info.open_database();
                    left = info.calc_left(0);
                    fullness = info.calc_fullness(0);
                    next_period = info.calc_next_period(0);
                }
            }
        }
        closedir(dir);
    }

    const bool need_to_alert = left < low_threshold ? true : false;

    if(sig_sock) {
        signal_sock_listener(sig_sock, fullness, need_to_alert);
    }

    if(need_to_alert) {
        const char * app[10];
        char sLeft[20];
        int i;
        for(i = 0; (i < 8) && (i < argc); i++) {
            app[i] = argv[i];
        }
        snprintf(sLeft,sizeof(sLeft),"%i", left);
        app[i++] = sLeft;
        app[i] = NULL;
        alert(left, app);
    }
    return next_period;
}

/**
 * main entry point
 */
int main(int argc, const char * argv[])
{
    int i;
    int time_to_respawn = 15;
    int reminder_period = 5;
    int low_threshold = 25;
    const char * sig_sock = NULL;

    for(i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            switch(argv[i][1])
            {
                case 'p':
                    i++;
                    time_to_respawn = to_int(argv[i]);
                    break;

                case 'r':
                    i++;
                    reminder_period = to_int(argv[i]);
                    break;

                case 't':
                    i++;
                    low_threshold = to_int(argv[i]);
                    break;

                case 's':
                    i++;
                    sig_sock = argv[i];
            }
        }
        else {
            break;
        }
    }


    while(1) {
        const int remaining = check_batteries(argc - i, &argv[i], sig_sock,
                low_threshold);
        printf("Remaining %i\n", remaining);
        if( (reminder_period > time_to_respawn)
            || (remaining > time_to_respawn + reminder_period)) {
            break;
        }

        sleep(60 * reminder_period);
        time_to_respawn -= reminder_period;
    }
    return EXIT_SUCCESS;
}
