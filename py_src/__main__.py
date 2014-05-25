import sys
import time
import os
import argparse
import logging
import stat

CACHE_FILE="/var/cache/batt_checker/history.txt"

def get_last():
    last_line = ""
    try:
        with open(CACHE_FILE) as in_fp:
            for line in in_fp:
                line.strip()
                if line:
                    lasy_line = line
    except FileNotFoundError:
        pass
    parts = last_line.split()
    if len(parts) > 0:
        return parts[0]
    return None

def parse_x_cmdline(argv):
    """Parse the X cmd for the display"""
    while argv:
        arg = argv.pop(0)
        if arg.startswith(":"):
            return arg

def get_uid(pid):
    """Get the UID of the PID"""
    return os.stat(os.path.join("/proc", pid)).st_uid


def get_environ(pid):
    """Get the environ for PID"""
    try:
        with open(os.path.join("/proc", pid, "environ")) as fp:
            environ = fp.read()
    except PermissionError:
        environ = ""
    return environ.split('\0')


def get_stdout(pid):
    try:
        return os.readlink(os.path.join("/proc", pid, "fd", "2"))
    except (FileNotFoundError, PermissionError):
        return None

#
def find_x_display():
    """Find the X-display so we can pop up a dialog box"""
    displays = set()
    stdouts = set()
    for pid in os.listdir("/proc"):
        if pid.isdigit():
            display = auth = None
            uid = get_uid(pid)
            stdout = get_stdout(pid)
            for token in get_environ(pid):
                if token.startswith("DISPLAY="):
                    display = token[8:]
                elif token.startswith("XAUTHORITY="):
                    auth = token[11:]
            if display and auth:
                displays.add((display, auth))
            stdouts.add(get_stdout(pid))
    for stdout in stdouts:
        print(stdout)
    for display in displays:
        print(display)

#            if tokens[0] in ["/usr/bin/X","/usr/bin/Xorg"]:
#                display.add(parse_x_cmdline(tokens))
#    return display


#def write_record(out_fp, ip_addr):
#    out_fp.write(ip_addr)
#    out_fp.write("\t")
#    out_fp.write(str(time.time()))
#    out_fp.write("\n")
 

#def make_path(pathname):
#    if os.path.exists(pathname):
#        return
#    root = os.path.dirname(pathname)
#    if not os.path.exists(root):
#        make_path(root)
#    os.mkdir(pathname)
    

#def record(ip_addr):
#    try:
#        with open(CACHE_FILE, "a") as out_fp:
#            write_record(out_fp, ip_addr)
#    except FileNotFoundError:
#        make_path(os.path.dirname(CACHE_FILE))
#        with open(CACHE_FILE, "w") as out_fp:
#            write_record(out_fp, ip_addr)

def run():
    parser = argparse.ArgumentParser(
            description="Battery Low alerter")
    parser.add_argument('-d','--debug', action="store_true", default=False, 
            help="Enable debug")
    args = parser.parse_args()
    log = logging.getLogger()
    if args.debug:
        print("Debug enabled")
        log.setLevel(logging.DEBUG)

    print(find_x_display())
run()
