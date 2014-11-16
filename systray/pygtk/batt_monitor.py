#!/usr/bin/python2

import gtk
import gobject


class SystrayIconApp:
    def __init__(self):
        self.tray = gtk.StatusIcon()
        self.tray.set_from_stock(gtk.STOCK_ABOUT)
        self.tray.set_from_icon_name("battery-full")
        self.tray.connect('popup-menu', self.on_right_click)
        self.tray.set_tooltip(('battery status'))
        gobject.timeout_add_seconds(1, self.callback)

    def callback(self):
        print "hello"
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
        about_dialog.set_icon_name ("SystrayIcon")
        about_dialog.set_name('SystrayIcon')
        about_dialog.set_version('0.1')
        about_dialog.set_copyright("(C) 2014")
        about_dialog.set_comments(("Battery Mon"))
        about_dialog.set_authors(['xxx <xxx@yyy.net>'])
        about_dialog.run()
        about_dialog.destroy()

if __name__ == "__main__":
    SystrayIconApp()
    gtk.main()

