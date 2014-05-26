import sys
import grp
import pwd
import subprocess
import os

def create_group(group):
    try:
        info = grp.getgrnam(group)
        gid = info.gr_gid
    except KeyError:
        subprocess.call(["groupadd","-r", group])
        info = grp.getgrnam(group)
        gid = info.gr_gid
    return gid


def create_user(user, home, gid):
    try:
        info = pwd.getpwnam(user)
        uid = info.pw_uid
    except KeyError:
        subprocess.call(["useradd","-r", "-g", str(gid), 
                "-s", "/bin/false", "-d", home, user])
        info = pwd.getpwnam(user)
        uid = info.pw_uid
    return uid



def make_dir(path, uid, gid):
    if not os.path.exists(path):
        os.mkdir(path)
    if uid is not None:
        os.chown(path, uid, gid)
        for f in os.listdir(path):
            os.chown(os.path.join(path,f), uid, gid)


def start_service():
    home = os.path.join("/var","cache", "batt_checker")
    make_dir(home, 1, 1)
#    make_dir(os.path.join("/etc","batt_checker"), None, None)
    subprocess.call(["systemctl","enable","batt_checker.timer"])
    subprocess.call(["systemctl","start","batt_checker.timer"])


def stop_service():
    subprocess.call(["systemctl","stop","batt_checker.timer"])
    subprocess.call(["systemctl","disable","batt_checker.timer"])

if __name__ == "__main__":
    if sys.argv[1] == "start":
        start_service()
    else:
        stop_service()
