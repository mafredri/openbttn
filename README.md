# Open bttn firmware

This is custom firmware for bt.tns that avoids communicating with the official bt.tn server. It's written using the [libopencm3](https://github.com/libopencm3/libopencm3) firmware library for ARM Cortex-M3 microcontrollers.

This firmware is in no way associated with The Button Corporation Ltd.

Installing this firmware will most likely void your warranty.

## Gettings started

Install the [GNU ARM Embedded Toolchain](https://launchpad.net/gcc-arm-embedded/+download):

```shell
brew cask install gcc-arm-embedded
```

Checkout and build the project:

```shell
git clone https://github.com/mafredri/openbttn && cd openbttn
git submodule update --init
make  # builds src/
```

Upload `main.elf` to your bttn using OpenOCD.


## Current status

### What works

* Board power
* Debugging USART (e.g. for `printf` output)
* Leds (rotating lights at the moment)
* Pressing the button (prints duration in ms)

### Todo

* Send commands to the SPWF01SA WIFI module
* Read / parse http responses from the WIFI module
* Write led flashing presets
* Short / long (/ extra long) press of button

### Nice to have

* OTA updates
* Double press of button
* Live configuration through WebUI
