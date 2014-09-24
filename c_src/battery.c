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

#include "battery_info.h"

#define SYS_PREFIX "/sys/class/power_supply"
#define CACHE_LOG  "/var/cache/batt_checker/data.log"
#define WORST_RATE "/var/cache/batt_checker/discharge"

/**
 * Close all file descriptors accept stdin, stdout and stderr
 * Used after a fork() so that child only has access to the
 * standard IO
 */
static void close_all_fds()
{
    const int maxFd = sysconf(_SC_OPEN_MAX);
    int i;
    for(i = 3; i <= maxFd; i++)
    {
        close(i);
    }
}

/**
 * Redirect stdout & stderr to /dev/null
 */
static void redirect_out()
{
    const int fd_null_out = open("/dev/null", O_RDONLY);
    if(fd_null_out > 0)
    {
	dup2(fd_null_out, 1);
	dup2(fd_null_out, 2);

        close(fd_null_out);
    }
}

/**
 * Redirect stdin & from /dev/null
 */
static void redirect_in()
{
    const int fd_null_in = open("/dev/null", O_WRONLY);
    if(fd_null_in > 0)
    {
        dup2(fd_null_in, 0);
        close(fd_null_in);
    }
}
/**
 * Generate an alert dialog to indicate battery is getting low.
 * This is done by forking another process to do the alert.
 *
 * @param[in] Estimated time left until battery is flat (in mins)
 * @param[in] The App plus args that does the alert dialog
 */
static void alert(int left, const char * app_argv[])
{
    printf("Alert Left =%i\n", left);

    pid_t pid = fork();
    if(pid == 0)      /* Child */
    {
        /* Group child and any grand children under the same process group */
        if(setsid() < 0)
        {
            fprintf(stderr, "Failed to setsid\n");
        }

        redirect_out();
        redirect_in();
        close_all_fds();

        pid_t pid = fork();
        if(pid == 0)
        {
            execvp(app_argv[0], (char **) app_argv);
            exit(EXIT_FAILURE);
        }
        else
        {
            exit(0);
        }
    }
    else if(pid > 0)
    {
        int status;
        wait(&status);
    }
}

/**
 * Convert text charging state value into two bool values
 *
 * @param[in] value The text charging state
 * @param[out] charging True if value implies charging is happening
 * @param[out] discharging True if value implies discharging is happening
 */
static void get_charging_state(const char * value, bool *charging, bool *discharging)
{
    bool up = false;
    bool down = false;

    if(strcmp(value, "Charging") == 0)
    {
        up = true;
    }
    else if(strcmp(value, "Discharging") == 0)
    {
        down = true;
    }
    else if( (strcmp(value, "Unknown") != 0)
            && (strcmp(value, "Full") != 0))
    {
        fprintf(stderr, "State = '%s'", value);
        exit(1);
    }
    if(charging)
        *charging = up;
    if(discharging)
        *discharging = down;
}


/**
 * Convert uW to W
 */
static float uwatts2watts(int value)
{
    return value / 1000000.0;
}

/**
 * Convert uWh to Joules
 */
static float uwatthr2joules(int value)
{
    return (value * 60.0 * 60.0)/1000000.0;
}

/**
 * Convert uWh to Joules
 */
static float uamphr2joules(int value, float volts)
{
    return uwatthr2joules(value * volts);
}

/**
 * Convert uV to V
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
static size_t read_sys(const char * base, const char * file, char * result, size_t maxlen)
{
    size_t length = 0;
    char pathname[1024];
    snprintf(pathname, sizeof(pathname), "%s/%s/%s", SYS_PREFIX, base, file);
//    printf("pathname='%s'\n", pathname);
    int f = open(pathname, O_RDONLY);
    if(f)
    {
        length = read(f, result, maxlen-1);
        while((length > 0) && (result[length-1] == '\n'))
            length--;
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
 * Fill in the BatterInfo_t structure with information read from the proc
 * filesystem about battery name
 *
 * @param[out] info The Battery status
 * @param[in] name The name of the battery
 */
static void check_battery(BatteryInfo_t * info, const char * name)
{
    memset(info, 0, sizeof(*info));

    char result[512];
    read_sys(name, "type", result, sizeof(result));
    if(strcmp(result,"Mains") == 0)
    {
        info->present = false;
    }
    else if(strcmp(result, "Battery") == 0)
    {
        read_sys(name, "present", result, sizeof(result));
        info->present = to_int(result);
    }
    else
    {
        fprintf(stderr, "Invalid type '%s'\n", result);
        exit(1);
    }
    if(!info->present)
        return;


    read_sys(name, "status", result, sizeof(result));
    get_charging_state(result, &info->charging, &info->discharging);

    read_sys(name, "voltage_now", result, sizeof(result));
    info->volts = uvolts2volts(to_int(result));
    {
        read_sys(name, "voltage_min_design", result, sizeof(result));
        const float min_voltage = uvolts2volts(to_int(result));
        if(info->volts < 0.5 * min_voltage)
        {
            printf("voltage reading is probably broken\n");
            info->volts = min_voltage;
        }
    }

    int status = read_sys(name, "energy_full", result, sizeof(result));
    if(status > 0)
    {
        info->last_full_capacity = uwatthr2joules(to_int(result));
    }
    else
    {
        read_sys(name, "charge_full", result, sizeof(result));
        info->last_full_capacity = uamphr2joules(to_int(result),
                info->volts);
    }

    status = read_sys(name, "energy_full_design", result, sizeof(result));
    if(status > 0)
    {
        info->max_capacity = uwatthr2joules(to_int(result));
    }
    else
    {
        read_sys(name, "charge_full_design", result, sizeof(result));
        info->max_capacity = uamphr2joules(to_int(result), info->volts);
    }

    status = read_sys(name, "energy_now", result, sizeof(result));
    if(status >0)
    {
        info->current_capacity = uwatthr2joules(to_int(result));
    }
    else
    {
        read_sys(name, "charge_now", result, sizeof(result));
        info->current_capacity = uamphr2joules(to_int(result),
                info->volts);
    }

    read_sys(name, "alarm", result, sizeof(result));
    if(status > 0)
    {
        info->min_capacity = uwatthr2joules(to_int(result));
    }
    else
    {
        info->min_capacity = uamphr2joules(to_int(result),
                info->volts);
    }

    status = read_sys(name, "power_now", result, sizeof(result));
    if(status > 0)
    {
        info->rate = uwatts2watts(to_int(result));
    }
    else
    {
        read_sys(name, "current_now", result, sizeof(result));
        info->rate = uwatts2watts(to_int(result) * info->volts);
    }
}

/**
 * Estimate in mins until battery reaches minimum charge
 *
 * @param[in] info The battery status
 * @param[in] min The minimum charge (in Joules)
 *
 * @return estimated time in minutes
 */
static int calc_left(const BatteryInfo_t * info, float min)
{
    if(info->discharging)
    {
        float left = info->current_capacity - min;
        if(info->rate > 0.0001)
        {
            return (int)(left / info->rate / 60.0 + 0.5);
        }
    }
    return 999;
}

/**
 * Open the database that contains history of previous measurements
 * and calculate the worst case (minimum time) for when battery will
 * go below the min threshold.
 *
 * @param[in] info The battery status
 * @param[in] min The minimum charge (in Joules)
 *
 * @return estimared time in minutes
 */
static int calc_next_period(const BatteryInfo_t * info, float min)
{
    float worst_rate = 15;
    float left = info->current_capacity - min;

    FILE * fp = fopen(WORST_RATE,"r");
    if(fp)
    {
        char buf[512];
        int got = fread(buf, sizeof(buf), 1, fp);
        buf[got] = '\0';
        fclose(fp);
        worst_rate = strtof(buf, 0);
    }
    if(worst_rate < info->rate)
    {
        worst_rate = info->rate;
    }
    return (int)(left / (60.0 * worst_rate) + 0.5);
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
    if(total > part)
    {
        return (int)(part*100/total + 0.5);
    }
    return -1;
}


/**
 * print info
 *
 * @param[in] info The battery status
 */
void print_self(const BatteryInfo_t * info)
{
    if(!info->present)
    {
        printf("No battery\n");
        return;
    }

    printf("Max      =%10.1f J (%3i%%)\n", info->max_capacity, 100);
    printf("Last full=%10.1f J (%3i%%)\n", info->last_full_capacity,
            calc_percent(info->last_full_capacity, info->max_capacity));
    printf("Current  =%10.1f J (%3i%%)\n", info->current_capacity,
            calc_percent(info->current_capacity, info->max_capacity));
    printf("Min      =%10.1f J (%3i%%)\n", info->min_capacity,
            calc_percent(info->min_capacity, info->max_capacity));

    printf("volts=%.2f v\n", info->volts);

//    info->min_capacity = 0
    if(info->charging)
    {
        float left = info->last_full_capacity - info->current_capacity;
        int minutes = (int)(left / info->rate / 60.0 + 0.5);
        printf("Charging rate=%f J/s\n", info->rate);
        printf( "%i mins left to reach last full\n", minutes);
    }
    if(info->discharging)
    {
        printf("Discharging rate=%f J/s\n", info->rate);
        printf("%i mins left before flat\n", calc_left(info, 0));
    }
}

/**
 * Write to the history database
 *
 * @param[in] info The battery status
 */
static void open_database(const BatteryInfo_t * info)
{
    time_t real_time;
    struct timespec up_time;
    real_time = time(NULL);
    clock_gettime(CLOCK_MONOTONIC, &up_time);

    const char status = info->charging ? '/' : info->discharging ? '\\' : '-';

    FILE * fp = fopen(CACHE_LOG,"a");
    if(fp)
    {
        fprintf(fp,"%lu\t%lu\t%c\t%9.1f\t%.2f\n",
                  real_time, up_time.tv_sec, status, info->current_capacity, info->volts);
        fclose(fp);
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
static int check_batteries(int argc, const char * argv[], int low_threshold)
{
    int left = 9999;
    int next_period = 9999;
    DIR * dir = opendir(SYS_PREFIX);
    if(dir)
    {
        struct dirent * entry;
        while((entry = readdir(dir)))
        {
            BatteryInfo_t info;
            if(entry->d_name[0] != '.')
            {
                check_battery(&info, entry->d_name);
                if(info.present)
                {
                    print_self(&info);
                    open_database(&info);
                    left = calc_left(&info, 0);
                    next_period = calc_next_period(&info, 0);
                }
            }
        }
        closedir(dir);
    }

    if(left < low_threshold)
    {
        const char * app[10];
        char sLeft[20];
        int i;
        for(i = 0; (i < 8) && (i < argc); i++)
        {
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

    for(i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-')
        {
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
            }
        }
        else
        {
            break;
        }
    }


    while(1)
    {
        const int remaining = check_batteries(argc - i, &argv[i], low_threshold);
        printf("Remaining %i\n", remaining);
        if( (reminder_period > time_to_respawn)
            || (remaining > time_to_respawn + reminder_period))
        {
            break;
        }

        sleep(60 * reminder_period);
        time_to_respawn -= reminder_period;
    }
    return EXIT_SUCCESS;
}
