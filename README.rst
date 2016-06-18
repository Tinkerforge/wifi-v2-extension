WIFI Extension 2.0
==================

**This Extension is currently Work In Progress.**

This repository contains the hardware design files.
The software is included in the Master Brick software
and can be found at https://github.com/Tinkerforge/master-brick

Repository Content
------------------

hardware/:
 * Contains kicad project files and additionally schematics as pdf

datasheets/:
 * Contains datasheets for sensors and complex ICs that are used

Hardware
--------

The hardware is designed with the open source EDA Suite KiCad
(http://www.kicad-pcb.org). Before you are able to open the files,
you have to install the Tinkerforge kicad-libraries
(https://github.com/Tinkerforge/kicad-libraries). You can either clone
them directly in hardware/ or clone them in a separate folder and
symlink them into hardware/
(ln -s kicad_path/kicad-libraries project_path/hardware). After that you
can open the .pro file in hardware/ with kicad and from there view and
modify the schematics and the PCB layout.

Toolchain
---------

Clone and build toolchain from https://github.com/pfalcon/esp-open-sdk
into toolchain/ directory::

 mkdir toolchain
 cd toolchain
 git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
 cd esp-open-sdk
 make STANDALONE=y

Update toolchain::

 make clean
 git pull
 git submodule sync
 git submodule update
 make STANDALONE=y

Additionally we use libesphttpd::

 cd toolchain
 git clone --recursive http://git.spritesserver.nl/libesphttpd.git/

To be able to build the extension firmware using libesphttpd a patch must be applied to the ESP8266 SDK. This assumes that the toolchain is already successfully built. This patch applies for current ESP8266 SDK version 1.5.4::

 cd software
 patch -b -N -d ../toolchain/esp-open-sdk/sdk -p1 < esp8266sdk-c_types.patch

Build
-----

Provided that all the steps of the Toolchain section was followed properly the extension firmware can be built with::

 cd software
 make clean
 make
