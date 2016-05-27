#!/usr/bin/env python
# -*- coding: utf-8 -*-

HOST = "localhost"
PORT = 4223
UID = "6e6MVj" # Change to your UID

from tinkerforge.ip_connection import IPConnection
from tinkerforge.brick_master import BrickMaster

if __name__ == "__main__":
    ipcon = IPConnection() # Create IP connection
    master = BrickMaster(UID, ipcon) # Create device object

    ipcon.connect(HOST, PORT) # Connect to brickd
    # Don't use device before ipcon is connected

    master.start_wifi2_bootloader()

    raw_input('Press key to exit\n') # Use input() in Python 3
    ipcon.disconnect()
