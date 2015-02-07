import os
import sys
import argparse
import logging
import tkinter
import tkinter.font as tkFont

CACHE_FILE = "/var/cache/batt_checker/history.txt"
PID_FILE = "/tmp/batt_checker.pid"


def get_last():
    """Get last line in history file"""
    last_line = ""
    try:
        with open(CACHE_FILE) as in_fp:
            for line in in_fp:
                line.strip()
                if line:
                    last_line = line
    except FileNotFoundError:
        pass
    parts = last_line.split()
    if len(parts) > 0:
        return parts[0]
    return None


def get_uid(pid):
    """Get the UID of the PID"""
    return os.stat(os.path.join("/proc", pid)).st_uid


def get_environ(pid):
    """Get the environ for PID"""
    try:
        with open(os.path.join("/proc", pid, "environ")) as in_fp:
            environ = in_fp.read()
    except PermissionError:
        environ = ""
    return environ.split('\0')


def get_term(pid):
    """Get the output terminal used by the process 'pid'"""
    try:
        stdout_fd = os.readlink(os.path.join("/proc", pid, "fd", "2"))
    except (FileNotFoundError, PermissionError):
        return None
    if stdout_fd.startswith("socket:"):
        return None
    if stdout_fd in ["/dev/null"]:
        return None
    return stdout_fd


def find_displays():
    """Find the X-display so we can pop up a dialog box"""
    displays = set()
    terminals = set()
    for pid in os.listdir("/proc"):
        if pid.isdigit():
            display = auth = None
            uid = get_uid(pid)
            for token in get_environ(pid):
                if token.startswith("DISPLAY="):
                    display = token[8:]
                    if display.find(".", display.rfind(":")) < 0:
                        display += ".0"
                elif token.startswith("XAUTHORITY="):
                    auth = token[11:]
            if display and auth:
                displays.add((display, auth))
            term = get_term(pid)
            if term:
                terminals.add(get_term(pid))
    return terminals, displays


# def write_record(out_fp, ip_addr):
#    out_fp.write(ip_addr)
#    out_fp.write("\t")
#    out_fp.write(str(time.time()))
#    out_fp.write("\n")


# def make_path(pathname):
#    if os.path.exists(pathname):
#        return
#    root = os.path.dirname(pathname)
#    if not os.path.exists(root):
#        make_path(root)
#    os.mkdir(pathname)


# def record(ip_addr):
#    try:
#        with open(CACHE_FILE, "a") as out_fp:
#            write_record(out_fp, ip_addr)
#    except FileNotFoundError:
#        make_path(os.path.dirname(CACHE_FILE))
#        with open(CACHE_FILE, "w") as out_fp:
#            write_record(out_fp, ip_addr)

def alert_terminals(terminals, left):
    """Send an alert message to the terminals"""
    for term in terminals:
        with open(term, "w") as out_fp:
            out_fp.write("Battery is low ({} mins to go)\n".format(left))


class Alert(tkinter.Frame):
    """TK alert box"""

    def __init__(self, master, left):
        tkinter.Frame.__init__(self, master)
        self.root = master
        self.root.geometry("+0-10")
        self.root.overrideredirect(True)
        self.pack()
        master.transient()
        master.attributes("-topmost", True)
        self.OK = tkinter.Button(self)
        self.OK["text"] = "OK"
        self.OK["font"] = tkFont.Font(size="30")
        self.OK["command"] = self.quit
        self.OK.pack({"side": "bottom"})
        self.LABEL = tkinter.Label(self)
        self.LABEL["text"] = "Battery getting low {} mins left" .format(
            left
        )
        self.LABEL["font"] = tkFont.Font(size="30")
        self.LABEL.pack({"side": "top"})
        self.OK.bind('<Visibility>', self.painted)

    def painted(self, event):
        """Callback once the panel has been painted"""
        self.after_idle(self.grab_focus_for_us)

    def grab_focus_for_us(self):
        """Grab the focus"""
        try:
            self.root.grab_set_global()
        except tkinter.TclError:
            pass
        self.root.after(5*60*1000, self.quit)

    def quit(self):
        self.root.quit()


def alert_display(left):
    """Send an alert message to X-server"""
    root = tkinter.Tk()
    app = Alert(root, left)
    app.mainloop()


def alert_displays(displays, left):
    """Send alert messages"""
    for (display, auth) in displays:
        fst = os.stat(auth)
        uid, gid = fst.st_uid, fst.st_gid
        saved_uid, saved_gid = os.getuid(), os.getgid()
        try:
            os.setegid(gid)
            os.seteuid(uid)
            os.putenv("DISPLAY", display)
            os.putenv("XAUTHORITY", auth)
            alert_display(left)
        finally:
            os.seteuid(saved_uid)
            os.setegid(saved_gid)

def lock():
    if os.path.exists(PID_FILE):
        with open(PID_FILE, "r") as in_fp:
            pid = int(in_fp.read())
        cmdline = "/proc/{}/cmdline".format(pid)
        if os.path.exists(cmdline):
            with open(cmdline, "r") as in_fp:
                cmdline = in_fp.read()
            if cmdline.startswith("python"):
                return False
    if not os.path.exists(os.path.dirname(PID_FILE)):
        os.mkdir(os.path.dirname(PID_FILE))
    our_pid = os.getpid()
    with open(PID_FILE, "w") as out_fp:
        out_fp.write("{}".format(our_pid))
    with open(PID_FILE, "r") as in_fp:
        pid = int(in_fp.read())
    return pid == our_pid


def run():
    """Main entry point"""
    parser = argparse.ArgumentParser(
            description="Battery Low alerter"
    )
    parser.add_argument(
        '-d',
        '--debug',
        action="store_true",
        default=False,
        help="Enable debug"
    )
    parser.add_argument('left', help="time left")
    args = parser.parse_args()
    log = logging.getLogger()
    if args.debug:
        print("Debug enabled")
        log.setLevel(logging.DEBUG)
    left = int(args.left)
    terminals, displays = find_displays()
    alert_terminals(terminals, left)
    # Avoid double focus grab :)
    if lock():
        try:
            alert_displays(displays, left)
        finally:
            os.unlink(PID_FILE)


if __name__ == "__main__":
    run()
