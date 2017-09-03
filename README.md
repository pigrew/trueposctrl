# trueposctrl
Stm32 control program for TruePosition GPSDO

Currently, it only very slightly works.

Things implemented:
* Provides a USB CDC interface which mirrors the GPSDO output.
* Sends the $PROCEED to start up the GPSDO when needed.

Todo:

* Relay commands from USB to the GPSDO
* Display info on a screen?
* LED to show lock status?
* Automatically enable the PPSDBG (to get tuning voltage)

# General

This software uses STM32CubeMX to configure the STM libraries, and is built with System Workbench for STM32. It uses FreeRTOS.

Getting serial-port reads with arbrary length to work seems not well documented (and near-impossible with the ST HAL libraries). I ended up using a RTOS queue structure to buffer data. The command parser reads from the queue, as needed.

# Hardware

This is targetting a "blue pill" STM32F103C8 board. The board requires a few modifications:

1. Cut the 5V track between the USB connector and the tee that goes to Vreg and the 5Vpin, so that the USB Vbus is isolated.
2. Add a wire between B12 and VBUS (used to detect if the USB port is connected).
3. Remove the 10k R10 (USB D+ pull-up).
4. Add a 1.5k pull-up between the R10 D+ pad and A15.
5. Add more solder + hot-glue to make the USB port stronger.
6. Double-check through-hole pins. Many were cold-solder joints.

# Pin-mapping
(parenthesis mark internal board connectons):

| Pin       | Usage           
| ----------|-------------
| GND       | GND to Trueposition GND (pin 1 or 3)
| PB10      | TX to TruePosition UART (pin 2)
| PB11      | RX from TruePosition UART (pin 4)
| (PA11)    | USB DN
| (PA12)    | USB DP
| (PA13)    | JTAG/SWDIO
| (PA14)    | JTAG/SWCLK
| (PA15)    | USB pull-up
| (PB12) | USB VBus
| (PC13) | Blue pill LED
| (PC14) | 32 kHz crystal
| (PC15) | 32 kHz crystal
| (PD0)  | 8 MHz crystal  
| (PD1)  | 8 MHz crystal  
| (V33/V5)| Input power to microcontroller board
