#!/usr/bin/env python3

import os
import pwd
from distutils.core import setup
from distutils.command import install_data, install


class my_install(install.install):
    def run(self):
        retVal = super().run()
        if not os.getenv("DONT_START"):
            from src import service
            service.start_service()
        return retVal;


setup(name='batt_checker',
      version='1.0',
      description="Check battery levels",
      url='https://github.com/peter1010/track_my_ip',
      author='Peter1010',
      author_email='peter1010@localnet',
      license='GPL',
      package_dir={'batt_checker':'py_src'},
      packages=['batt_checker'],
      data_files=[\
            ('/usr/lib/systemd/system',
                ('batt_checker.timer", "batt_checker.service'))],
      cmdclass = {'install' : my_install}
      )
