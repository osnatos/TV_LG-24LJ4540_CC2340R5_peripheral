# Hardware module for converting command line into IR signal.
This repository contains the FW for the MCU of hardware module that is part of the TV LG-24LJ4540 Remote Control System. <br/>
The module uses a microcontroller CC2340R5.

The remote control system consists of a smartphone application and a hardware module.

The application works on a smartphone without infrared port (IR) with the Android operating system. <br>
Its source code is provided in the repository: [Android_TV_LG-24LJ4540](https://github.com/osnatos/Android_TV_LG-24LJ4540_central.git).

Hardware module is designed to receive from the smartphone, via the BLE interface, the command string and converts it into an encoded IR signal. The module is used as a BLE peripheral and uses an CC2340R5 microcontroller


