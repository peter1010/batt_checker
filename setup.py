#!/usr/bin/env python3

import os
import subprocess

from distutils.core import setup
from distutils.command import install


class my_install(install.install):
    def run(self):
        retVal = super().run()
        if self.root is None or not self.root.endswith("dumb"):
            if not os.getenv("DONT_START"):
                print("Setup.py starting the services")
                from py_src import service
                service.start_service()
        return retVal


def get_batt_checker_exe():
    subprocess.call(['make', '-C', 'c_src'])
    return os.path.join("c_src", "__%s__" % os.uname().machine, "batt_checker")


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
    cmdclass={'install': my_install}
)
