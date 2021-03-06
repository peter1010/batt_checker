##
# Copyright (c) 2014 Peter Leese
#
# Licensed under the GPL License. See LICENSE file in the project root for full license information.  
##

def analysis_charge(change):
#    print(change)
    pass


min_charge = 0


def analysis_discharge(charge):
    global min_charge
    if charge < min_charge:
        min_charge = charge
        print("Min charge=", min_charge)


def parse():
    prev_time = None
    prev_cap = None
    with open("/var/cache/batt_checker/data.log") as fp:
        for line in fp:
            tokens = line.split()
            if not (tokens[0].isdigit() or tokens[1].isdigit()):
                continue
            real_time, up_time = int(tokens[0]), int(tokens[1])
            cap = float(tokens[3])
            if prev_time and (up_time > prev_time):
                # Time in unit of seconds, max error is +/-0.5
                period = (up_time - prev_time)
                min_period = period - 1
                max_period = period + 1
                if min_period > 60:
                    change = (cap - prev_cap)/period
                    if change < 0:
#                        print("Change", change, "Period", period)
                        analysis_discharge(change)
                    else:
                        analysis_charge(change)
            prev_time, prev_cap = up_time, cap

parse()
