import time


def analysis():
    prev_time = None
    prev_cap = None
    with open("/var/cache/batt_checker/data.log") as fp:
        for line in fp:
            tokens = line.split()
            tm = time.strptime(" ".join(tokens[:2]), "%Y-%m-%d %H:%M:%S")
            t = time.mktime(tm)
            cap = float(tokens[3])
            if prev_time:
                change = (prev_cap - cap)/(prev_time - t)
                print(change)
            prev_time, prev_cap = t, cap

analysis()
