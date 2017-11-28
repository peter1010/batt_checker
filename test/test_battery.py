#!/usr/bin/env python

##
# Copyright (c) 2014 Peter Leese
#
# Licensed under the GPL License. See LICENSE file in the project root for full license information.  
##


import sys
import os
import unittest
import subprocess
import platform
import shutil
import socket

test_dir = os.path.join(os.path.abspath(os.path.dirname(__file__)))

def tmp_test_dir():
    return os.path.join(test_dir, "tmp")

def chk_battery_exe():
    return os.path.abspath(
            os.path.join(
                    test_dir,
                    "..",
                    "c_src",
                    "__{}__".format(platform.machine()), 
                    "batt_checker"
            )
    )

def glibc_mocks():
    return os.path.abspath(
            os.path.join(
                    test_dir,
                    "__{}__".format(platform.machine()),
                    "glibc_mocks.so"
            )
    )

def unix_mock_from():
    return os.path.join(tmp_test_dir(), ".from_mock")

def unix_alert_sock():
    return os.path.join(tmp_test_dir(), ".from_batt_checker")

def run():
    args = [chk_battery_exe(), "-s", unix_alert_sock()]
    env = dict(os.environ)
    env["TMP_TEST_DIR"] = tmp_test_dir()
    env["LD_PRELOAD"] = glibc_mocks()
    env["TMP_MOCK_FROM"] = unix_mock_from()

    proc = subprocess.Popen(args, env=env)
    proc.wait()


def set_proc(base, name, value):
    if name.startswith("/"):
        name = name[1:]
    path = os.path.abspath(
            os.path.join(
                    tmp_test_dir(),
                    "sys/class/power_supply",
                    base,
                    name
            )
    )
    print(path)
    dirpath = os.path.dirname(path)
    os.makedirs(dirpath)
    with open(path, "wb") as out_fp:
        out_fp.write(str(value).encode("ascii"))

def create_listening_socks():
    if not os.path.exists(tmp_test_dir()):
        os.mkdir(tmp_test_dir())
    sock1 = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock1.bind(unix_mock_from())
    sock2 = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock2.bind(unix_alert_sock())
    return (sock1, sock2)

# type
# present
# status
# voltage_now
# voltage_min_design
# energy_full
# charge_full
# energy_full_design
# charge_full_design
# energy_now
# charge_now
# alarm
# power_now
# current_now

class TestFunctions(unittest.TestCase):
   
    def setUp(self):
        self.socks = create_listening_socks()

    def tearDown(self):
        path = tmp_test_dir()
        if os.path.exists(path):
            shutil.rmtree(path)
        for sock in self.socks:
            sock.close()


    def test_no_sys_dir(self):
        run()

    def test_bad_type(self):
        set_proc("AC", "type", "cobblers")
        run()

    def test_mains_type(self):
        set_proc("DC", "type", "Mains")
        run()

    def test_several_types(self):
        set_proc("AC", "type", "cobblers")
        set_proc("BAT0", "type", "battery")
        run()


if __name__ == '__main__':
    unittest.main()

