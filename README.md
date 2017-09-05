# trueposctrl
Stm32 control program for TruePosition GPSDO

Currently, it mostly works.

![Screenshot of survey mode](https://raw.githubusercontent.com/wiki/pigrew/trueposctrl/images/screen_survey.jpg)

![Photograph of the blue-pill board with a display showing a locked GPSDO signal](https://raw.githubusercontent.com/wiki/pigrew/trueposctrl/images/TrueposCtrl_sm.jpg)


It's designed to use a cheap I<sup>2</sup>C SSD1306 0.96" OLED display.
SPI may be better, as it supports much higher data-rates so the display lag would be less.

Things implemented:
* Provides a USB CDC interface which mirrors the GPSDO output.
* Sends the $PROCEED to start up the GPSDO when needed.
* Displays:
  * UTC time, number of satellites, board temperature, DAC voltage, lock status, bad antenna, bad 10MHz, bad PPS, and survey remaining time
* Automatically enables the PPSDBG (to get tuning voltage, and more frequent status messages)
* STM32 board's LED shows lock status.
* Relays commands from USB to the GPSDO.
* Displays notification when the GPSDO stops responding.

Todo:

* UI for setting location/survey? Maybe not? A computer can be used via USB to communicate with the GPSDO.
* Use compile-time define for setting timezone + fixed location/survey.
* Display holdover duration or how long since last holdover?.

# General

This software uses STM32CubeMX to configure the STM libraries, and is built with System Workbench for STM32. It uses FreeRTOS.

Getting serial-port reads with arbrary length to work seems not well documented (and near-impossible with the ST HAL libraries). I ended up using a RTOS queue structure to buffer data. The command parser reads from the queue, as needed.

# Hardware

See [the wiki hardware page](https://github.com/pigrew/trueposctrl/wiki/Hardware-Details-and-Mods) for details.


# Software

See [the wiki software page](https://github.com/pigrew/trueposctrl/wiki/Software) for details.

# Weird issues encountered

The first issue was the availability of serial IO libraries. The provided HAL driver
couldn't immediately be used because it only supported fixed-length reads and writes.
The solution was to handle the serial-port interrupt by loading character data into
a FreeRTOS queue.

The second annoying issue was that I2C wasn't working. The serial port would hang. It ended
up that global variables were being corrupted. I think that the cause was the BOOT0 jumper
was set to 1. When the bootloader runs, it seems to overwrite globals and make bad things happen.
