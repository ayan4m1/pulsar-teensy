# Pulsar for Teensy

This is an open-source hardware (OSHW) project to run scripted firing sequences on an Evolv DNA device from a Teensy-based dongle.

**NOTE**: Because this device can fire a vaporizer, it can start an **actual fire** if used improperly. Please read the liability disclaimer in `LICENSE` before using this software.

## Hardware

 * Teensy (v3.6 only at present)
 * micro-USB breakout board/cable (VBUS/D+/D-/GND)
 * Pushbutton/tactile switch (Normally Open)
 * microSD Card, FAT32 formatted

## Software

Find the latest firmware [here](https://github.com/ayan4m1/pulsar-teensy/releases). Flash the firmware using [Teensy Loader](https://www.pjrc.com/teensy/loader.html).

Alternatively, clone this repository and use [PlatformIO](https://platformio.org/) to flash it to your Teensy.

## Setup/Usage

Wire the switch between GND and GPIO1. Wire the micro-USB breakout to the USB Host Pins on the Teensy. Place `test.csv` on the microSD card. Insert the SD card into the Teensy. Supply USB power to the Teensy, then connect it to the DNA device. Press the switch to play back the script contained in `test.csv`.

## Puff Files

A puff file is a .csv file like this:

```csv
# Lines starting with a hash are comments and are ignored
# The next line sets wattage to 10 watts
W,10
# The next line fires for 5 seconds
F,5
# The next line waits for 7 seconds
P,7
# Now a simple "wattage curve" for demonstration
W,30
F,3
P,1
W,50
F,3
P,2
W,40
F,3
P,5
```
