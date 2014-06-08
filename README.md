batt_checker (battery checker)
==============================

A lightweight task that polls the battery status via systemd timer jobs.

If battery is getting low, a pop-up box (if X is running) and a message is broadcast to all ttys to inform the user the battery is getting low.

The objective was not to have a systray application, but to have something that uses as little resources as possible.

The checking app is written in C, the notfication code is in python but only run when notification is required.

In addition the current power is logged to a file for later analysis.
