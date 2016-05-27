#!/usr/bin/env python
# -*- coding: utf-8 -*-

HOST = "192.168.178.91"
PORT = 4223
UID = "gy2" # Change to your UID

from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_distance_ir import DistanceIR

if __name__ == "__main__":
    ipcon = IPConnection() # Create IP connection
    dir = DistanceIR(UID, ipcon) # Create device object

    ipcon.connect(HOST, PORT) # Connect to brickd
    # Don't use device before ipcon is connected

    fails = 0

    while True:
        try:
            c = 0
            while True:
                c += 1
                if c == 256:
                    c = 0
                value = dir.get_sampling_point(c)
                if value != c:
                    fails += 1
                print('value: ' + str(c) + ' -> ' + str(value)) + ' -> ' + str(fails)
        except:
            import traceback
            traceback.print_exc()

        raw_input('Press key to exit\n') # Use input() in Python 3
    ipcon.disconnect()
