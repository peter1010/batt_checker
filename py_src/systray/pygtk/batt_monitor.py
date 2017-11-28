#!/usr/bin/python2

##
# Copyright (c) 2014 Peter Leese
#
# Licensed under the GPL License. See LICENSE file in the project root for full license information.  
##

import gtk
import gobject
import socket
import os

BATTERY_FULL = 'battery-full'
BATTERY_GOOD = 'battery-good'
BATTERY_LOW = 'battery-low'
BATTERY_CAUTION = 'battery-caution'

icons = \
(
    BATTERY_FULL,
    BATTERY_GOOD,
    BATTERY_LOW,
    BATTERY_CAUTION,
)


class SystrayApp:

    def __init__(self):
        self.tray = gtk.StatusIcon()
        self.tray.set_from_icon_name(BATTERY_GOOD)
        self.tray.connect('popup-menu', self.on_right_click)
        self.tray.set_tooltip(('battery status ?? %'))
        self.open_listener()

    def __del__(self):
        os.unlink('/tmp/batt_checker')

    def open_listener(self):
        if os.path.exists('/tmp/batt_checker'):
            os.unlink('/tmp/batt_checker')
        fd = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        fd.bind('/tmp/batt_checker')
        gobject.io_add_watch(fd, gobject.IO_IN, self.cb_update_status)

    def cb_update_status(self, fd, condition):
        data = fd.recv(1024)
        tokens = data.split()
        if len(tokens) == 2 and tokens[1] == "ALERT":
            self.alert = True;
        else:
            self.alert = False;
        self.fullness = int(tokens[0])
        self.tray.set_tooltip(('battery status {} %'.format(
            self.fullness)
        ))
        if self.alert:
            retVal = self.tray.set_from_icon_name(BATTERY_CAUTION)
        elif self.fullness > 75:
            retVal = self.tray.set_from_icon_name(BATTERY_FULL)
        elif self.fullness >= 50:
            retVal = self.tray.set_from_icon_name(BATTERY_GOOD)
        else:
            retVal = self.tray.set_from_icon_name(BATTERY_LOW)
        print(retVal)

        return True

    def on_right_click(self, icon, event_button, event_time):
        self.make_menu(event_button, event_time)

    def make_menu(self, event_button, event_time):
        menu = gtk.Menu()
        # show about dialog
        about = gtk.MenuItem("About")
        about.show()
        menu.append(about)
        about.connect('activate', self.show_about_dialog)

        # add quit item
        quit = gtk.MenuItem("Quit")
        quit.show()
        menu.append(quit)
        quit.connect('activate', gtk.main_quit)

        menu.popup(None, None, gtk.status_icon_position_menu,
               event_button, event_time, self.tray)

    def show_about_dialog(self, widget):
        about_dialog = gtk.AboutDialog()
        about_dialog.set_destroy_with_parent (True)
        about_dialog.set_icon_name ("Battery Status")
        about_dialog.set_name('Battery Status')
        about_dialog.set_version('0.1')
        about_dialog.set_copyright("(C) 2014")
        about_dialog.set_comments(("Battery Mon"))
        about_dialog.set_authors(['xxx <xxx@yyy.net>'])
        about_dialog.run()
        about_dialog.destroy()

if __name__ == "__main__":
    SystrayApp()
    gtk.main()

