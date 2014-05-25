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


def remove_group(group):
    try:
        info = grp.getgrnam(group)
        subprocess.call(["groupdel", group])
    except KeyError:
        pass



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


def remove_user(user):
    try:
        info = pwd.getpwnam(user)
        subprocess.call(["userdel", user])
    except KeyError:
        pass


def make_dir(path, uid, gid):
    if not os.path.exists(path):
        os.mkdir(path)
    if uid is not None:
        os.chown(path, uid, gid)
        for f in os.listdir(path):
            os.chown(os.path.join(path,f), uid, gid)


def start_service():
    home = os.path.join("/var","cache", "batt_checker")
    gid = create_group("batt_checker")
    uid = create_user("batt_checker", home, gid)
    make_dir(home, uid, gid)
#    make_dir(os.path.join("/etc","batt_checker"), None, None)
    subprocess.call(["systemctl","enable","batt_checker.timer"])
    subprocess.call(["systemctl","start","batt_checker.timer"])


def stop_service():
    subprocess.call(["systemctl","stop","batt_checker.timer"])
    subprocess.call(["systemctl","disable","batt_checker.timer"])
    remove_user("batt_checker")
    remove_group("batt_checker")

if __name__ == "__main__":
    if sys.argv[1] == "start":
        start_service()
    else:
        stop_service()
