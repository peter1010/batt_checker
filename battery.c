#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define SYS_PREFIX "/sys/class/power_supply"

#if 0
NOTIFY = "/usr/bin/notify-send"
TIMEOUT = 20
#endif

void alert(int left)
{
    printf("Left =%i\n", left);
#if 0
    msg ='Time left %3i mins' % left
    if os.path.exists(NOTIFY):
        args = [NOTIFY, "Battery flat", msg]
    proc = subprocess.Popen(args)
    proc.wait()
#endif
}

/**
 * Convert value into two bool values
 */
void get_charging_state(const char * value, int *charging, int *discharging)
{
    if(strcmp(value, "Charging") == 0)
    {
        *charging = true;
        *discharging = false;
    }
    else if(strcmp(value, "Discharging") == 0)
    {
        *charging = false;
        *discharging = true;
    }
    else if(strcmp(value, "Unknown") == 0)
    {
        *charging = false;
        *discharging = false;
    }
    else if(strcmp(value, "Full") == 0)
    {
        *charging = false;
        *discharging = false;
    }
    else
    {
        fprintf(stderr, "State = '%s'", value);
        exit(1);
    }
}


/**
 * Convert uW to W
 */
float uwatts2watts(int value)
{
    return value / 1000000.0;
}

/**
 * Convert uWh to Joules
 */
float uwatthr2joules(int value)
{
    return (value * 60.0 * 60.0)/1000000.0;
}

/**
 * Convert uWh to Joules
 */
float uamphr2joules(int value, float volts)
{
    return uwatthr2joules(value * volts);
}

/**
 * Convert uV to V
 */
float uvolts2volts(int value)
{
    return value / 1000000.0;
}

/**
 * read proc file system
 */
size_t read_sys(const char * base, const char * file, char * result, size_t maxlen)
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
        perror("");
        close(f);
    }
    result[length] ='\0';
    return length;
}

struct BatteryInfo_s
{
    int present;
    int charging;
    int discharging;
    float max_capacity;
    float last_full_capacity;
    float current_capacity;
    float min_capacity;
    float volts;
    float rate;
};

void check_battery(struct BatteryInfo_s * info, const char * name)
{
    info->present = false;
    info->charging = false;
    info->discharging = false;
    info->max_capacity = 0;
    info->last_full_capacity = 0;
    info->current_capacity = 0;
    info->min_capacity = 0;
    info->volts = 0;
    info->rate = 0;

    char result[512];
    read_sys(name, "type", result, sizeof(result));
    if(strcmp(result,"Mains") == 0)
    {
        info->present = false;
    }
    else if(strcmp(result, "Battery") == 0)
    {
        read_sys(name, "present", result, sizeof(result));
        info->present = strtol(result, NULL, 10);
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
    info->volts = uvolts2volts(strtol(result, NULL, 10));

    int status = read_sys(name, "energy_full", result, sizeof(result));
    if(status > 0)
    {
        info->last_full_capacity = uwatthr2joules(strtol(result, NULL, 10));
    }
    else
    {
        read_sys(name, "charge_full", result, sizeof(result));
        info->last_full_capacity = uamphr2joules(strtol(result,NULL,10), 
                info->volts);
    }

    status = read_sys(name, "energy_full_design", result, sizeof(result));
    if(status > 0)
    {
        info->max_capacity = uwatthr2joules(strtol(result, NULL, 10));
    }
    else
    {
        read_sys(name, "charge_full_design", result, sizeof(result));
        info->max_capacity = uamphr2joules(strtol(result, NULL,10), info->volts);
    }

    status = read_sys(name, "energy_now", result, sizeof(result));
    if(status >0)
    {
        info->current_capacity = uwatthr2joules(strtol(result, NULL, 10));
    }
    else
    {
        read_sys(name, "charge_now", result, sizeof(result));
        info->current_capacity = uamphr2joules(strtol(result, NULL, 10),
                info->volts);
    }

    read_sys(name, "alarm", result, sizeof(result));
    if(status > 0)
    {
        info->min_capacity = uwatthr2joules(strtol(result,NULL,10));
    }
    else
    {
        info->min_capacity = uamphr2joules(strtol(result, NULL, 10),
                info->volts);
    }

    status = read_sys(name, "power_now", result, sizeof(result));
    if(status > 0)
    {
        info->rate = uwatts2watts(strtol(result,NULL,10));
    }
    else
    {
        read_sys(name, "current_now", result, sizeof(result));
        info->rate = uwatts2watts(strtol(result,NULL,10) * info->volts);
    }
}

/**
 * print info
 */
void print_self(struct BatteryInfo_s * info)
{
    printf("Present=%i\n", info->present);
    printf("Charging=%i\n", info->charging);
    printf("Discharging=%i\n", info->discharging);
    printf("Max=%f\n", info->max_capacity);
    printf("Last full=%f\n", info->last_full_capacity);
    printf("Current=%f\n", info->current_capacity);
    printf("Min=%f\n", info->min_capacity);
    printf("volts=%.2f\n", info->volts);
    printf("rate=%f\n", info->rate);

//    info->min_capacity = 0
    if(info->charging)
    {
        float left = info->last_full_capacity - info->current_capacity;
        int minutes = (int)(left / info->rate / 60.0 + 0.5);
        printf( "mins letf=%i", minutes);
    }
    if(info->discharging)
    {
        float left = info->current_capacity - info->min_capacity;
        float total = info->last_full_capacity - info->min_capacity;
        int percent = (int)(left*100/total + 0.5);
        int minutes = (int)(left / info->rate / 60 + 0.5);
        printf("left %i %i", percent, minutes);
    }
}

#if 0
    def left(self):
        if self.discharging:
            left = self.current_capacity - self.min_capacity
            total = self.last_full_capacity - self.min_capacity
            percent = int(left*100/total + 0.5)
            minutes = int(left / self.rate / 60 + 0.5)
            print( percent, minutes)
            return minutes
        return 999



def open_database(obj):
    """Open the database"""
    try:
        os.mkdir("/var/cache/battery")
    except OSError:
        pass
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    if obj.charging:
        status = "/"
    elif obj.discharging:
        status = "\\"
    else:
        status = "-"
    try:
        with open("/var/lib/battery/data.log","a") as file_p:
            file_p.write("%s\t%s\t%9.1f\t%.2f\n" % \
                  (timestamp, status, obj.current_capacity, obj.volts))
    except FileNotFoundError:
        path = "/var/lib/battery"
        uid = os.getenv("SUDO_UID")
        gid = os.getenv("SUDO_GID")
        os.mkdir(path)
        if uid and gid:
            os.chown(path, int(uid), int(gid))
#endif

/**
 * Check all the batteries
 */
void check_batteries()
{
    int left = 0;
    DIR * dir = opendir(SYS_PREFIX);
    if(dir)
    {
        struct dirent * entry;
        while((entry = readdir(dir)))
        {
            struct BatteryInfo_s info;
            if(entry->d_name[0] != '.')
            {
                check_battery(&info, entry->d_name);
                if(info.present)
                {
                    printf( "---\n");
                    print_self(&info);
#if 0
            open_database(obj)
            left = obj.left()
#endif
                }
            }
        }
        closedir(dir);
    }
    if(left < 25)
    {
        alert(left);
    }
}

int main(int argv, char * argc[])
{
    check_batteries();
}   
