import os
import argparse
import logging
import tkinter

CACHE_FILE = "/var/cache/batt_checker/history.txt"


def get_last():
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
        with open(os.path.join("/proc", pid, "environ")) as fp:
            environ = fp.read()
    except PermissionError:
        environ = ""
    return environ.split('\0')


def get_term(pid):
    try:
        fd = os.readlink(os.path.join("/proc", pid, "fd", "2"))
    except (FileNotFoundError, PermissionError):
        return None
    if fd.startswith("socket:"):
        return None
    if fd in ["/dev/null"]:
        return None
    return fd


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
    for term in terminals:
        with open(term, "w") as fp:
            fp.write("Battery is low (%i mins to go)\n" % left)


class Alert(tkinter.Frame):
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
        self.OK["command"] = self.quit
        self.OK.pack({"side": "bottom"})
        self.LABEL = tkinter.Label(self)
        self.LABEL["text"] = "Battery getting low %i mins left" % left
        self.LABEL.pack({"side": "top"})
        self.OK.bind('<Visibility>', self.painted)

    def painted(self, event):
        print("painted")
        self.after_idle(self.grab_focus_for_us)

    def grab_focus_for_us(self):
        self.root.grab_set_global()


def alert_display(left):
    root = tkinter.Tk()
    app = Alert(root, left)
    app.mainloop()


def alert_displays(displays, left):
    for (display, auth) in displays:
        st = os.stat(auth)
        uid, gid = st.st_uid, st.st_gid
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


def run():
    parser = argparse.ArgumentParser(
        description="Battery Low alerter"
    )
    parser.add_argument('-d', '--debug', action="store_true", default=False,
                        help="Enable debug")
    parser.add_argument('left', help="time left")
    args = parser.parse_args()
    log = logging.getLogger()
    if args.debug:
        print("Debug enabled")
        log.setLevel(logging.DEBUG)
    left = int(args.left)
    terminals, displays = find_displays()
    alert_terminals(terminals, left)
    alert_displays(displays, left)


run()
