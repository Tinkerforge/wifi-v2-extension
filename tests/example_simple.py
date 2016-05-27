#!/usr/bin/env python
# -*- coding: utf-8 -*-

HOST = "192.168.178.91"
PORT = 4223
UID = "uNS" # Change to your UID

import time
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_ambient_light_v2 import AmbientLightV2

if __name__ == "__main__":
    ipcon = IPConnection() # Create IP connection
    al = AmbientLightV2(UID, ipcon) # Create device object

    ipcon.connect(HOST, PORT) # Connect to brickd
    # Don't use device before ipcon is connected

    t = time.time()
    for i in range(100000):
        # Get current illuminance (unit is Lux/100)
        illuminance = al.get_illuminance()
        print('Illuminance: ' + str(illuminance/100.0) + ' Lux')

    print(time.time() - t)
    ipcon.disconnect()
