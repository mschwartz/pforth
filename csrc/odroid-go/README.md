# Odroid Go pforth

The Odroid Go is an ESP32 (processor) based handheld device, similar to a GameBoy.  It has a 320x240 18-bit color
display, 4 megabytes of SLOW RAM, 16 megabytes of FLASH for program execution and file storage, a joystick, A and B
buttons plus 4 buttons below the LCD, 8 bit DAC, WiFi, Bluetooth, SD Card reader/writer, etc.  

Programs are uploaded to the device via a USB cable (serial).  Once a program is running, it can read/write console
style over the USB to a terminal program running on the host.

The ESP32 SoC has 512K of RAM on board and 32K of CPU cache, as well as dual processor cores.  The initial version of
pForth for Odroid Go only supports one core.  Preemptive multicore threading is possible with a bit of work.

Some of the  512K of RAM is used by the ESP-IDF framework and FreeRTOS - not all of it is available for Forth
dictionary.  The available RAM is also fragmented.  

## TODO
1) The 4K of SPI RAM should be available for allocation, malloc() style (amiga-like)
2) Forth isn't ideal for interrupt handlers, but it sure would be nice to be able to imoplement drivers entirely in
Forth.

## Rresources
1) [Leo Brodie's Starting Forth (online book, tutorial)](http://home.iae.nl/users/mhx/sf.html)

