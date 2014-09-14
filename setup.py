#!/usr/bin/env python3

import os
import subprocess
import sys

from distutils.core import setup
from distutils.command import install, build

def find_c_src():
    c_src = "c_src"
    for i in range(5):
        if os.path.exists(c_src):
            break
        c_src = os.path.join("..", c_src)
    else:
        sys.exit(1)
    return c_src



class my_install(install.install):
    def run(self):
        retVal = super().run()
        if self.root is None or not self.root.endswith("dumb"):
            if not os.getenv("DONT_START"):
                print("Setup.py starting the services")
                from py_src import service
                service.start_service()
        return retVal

class my_build(build.build):
    def run(self):
        retVal = super().run()
        subprocess.call(['make', '-C', find_c_src()])
        return retVal


def get_batt_checker_exe(do_compile=True):
   return os.path.join(find_c_src(), "__%s__" % os.uname().machine, "batt_checker")


setup(
    name='batt_checker',
    version='1.0',
    description="Check battery levels",
    url='https://github.com/peter1010/batt_checker',
    author='Peter1010',
    author_email='peter1010@localnet',
    license='GPL',
    package_dir={'batt_checker': 'py_src'},
    packages=['batt_checker'],
    data_files=[
        ('/usr/lib/systemd/system',
         ('batt_checker.timer', 'batt_checker.service')),
        ('/usr/bin/', (get_batt_checker_exe(), ))],
    cmdclass={'install': my_install, 'build': my_build}
)
