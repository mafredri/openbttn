# OpenBttn 

OpenBttn is a custom open source firmware for [bt.tn](https://bt.tn) buttons. This firmware does not communicate with the official bt.tn servers and contains some features not available in the original firmware. It's written using the [libopencm3](https://github.com/libopencm3/libopencm3) firmware library for ARM Cortex-M3 microcontrollers.

This firmware is in no way associated with The Button Corporation Ltd.

Installing this firmware will most likely void your warranty.

> Herein, **bt.tn** refers to The Button Corporation Ltd., and **bttn** refers to a consumer device manufactured by said company.

## Features

Similar to stock firmware, OpenBttn supports:

* Single and long press button actions
* LED response to button presses

Plus:

* Low latency response times - no bt.tn server registration
* Wi-Fi SSIDs may contain spaces and other special characters
* Live configuration via bttn's web interface
* Built-in socket server provides API commands including [custom LED flashing](https://github.com/mafredri/openbttn/commit/d2a22cb6291fbe04f809ac2fbe771ca0c1953c66)
* OTA update of the SPWF01SA Wi-Fi module
* Password authentication for bttn configuration

## Hardware

#### Supported models

The following bttn models have been tested and confirmed working:

* bttn Wi-Fi

#### Untested models

* bttn Mobile Data
* bttn Mini Wi-Fi
* bttn Mini Mobile Data

## Gettings started

### Overview

1. Download the latest release or build the project yourself
2. (Optionally) backup the existing firmware
3. Install new firmware
4. (Optionally) update the Wi-Fi module firmware
4. Boot bttn in recovery mode to input your Wi-Fi settings
5. Reboot your bttn
5. Live configure the button press URLs using Web-UI
6. Enjoy your OpenBttn!

### Get the firmware

You may compile the firmware yourself or use an pre-compiled firmware image.

#### Download a pre-compiled firmware

Grab the newest [release](https://github.com/mafredri/openbttn/releases) and unzip it.

#### Build your own firmware from source

Install the [GNU ARM Embedded Toolchain](https://launchpad.net/gcc-arm-embedded/+download):

```shell
brew cask install gcc-arm-embedded
```

Check out and build the project:

```shell
git clone https://github.com/mafredri/openbttn && cd openbttn
git submodule update --init
make  # builds src/main.elf
cd src && make bin # builds src/main.bin
```

### Firmware flashing methods

This firmware can be installed in two ways, either via DFU mode or via the JTAG interface on the bttn board.

Only DFU mode is covered here.

### Boot bttn in DFU mode

1. Open the bttn by removing the three T10 torx screws ![torx](resources/torx.png "Torx head") at the bottom.
2. Connect the [Boot pins](./resources/bttn-boot-pins.png) on the board (note: only the Boot pins, not the FWSel ones). An easy way to do this is with [a paper clip and a bit of electrical tape](./resources/bttn-boot-pins-shorting.jpg) twice-folded so as to insulate the paper clip so that it does not touch the FWSel pins or any other contacts on the board.
3. Connect the bttn via USB to computer
4. Run via terminal:

```
dfu-util --list
```

You should see output like the following. Note that the device ID (product and vendor) will likely be the same if you're using a Wi-Fi bttn (`0483:df11`). This may vary with other bttn models (untested).

```
Found Runtime: [05ac:8290] ver=0146, devnum=5, cfg=1, intf=5, path="20-3", alt=0, name="UNKNOWN", serial="UNKNOWN"
Found DFU: [0483:df11] ver=2200, devnum=28, cfg=1, intf=0, path="20-1", alt=2, name="@DATA Memory /0x08080000/02*004Ke", serial="76B0A1336969"
Found DFU: [0483:df11] ver=2200, devnum=28, cfg=1, intf=0, path="20-1", alt=1, name="@Option Bytes  /0x1FF80000/01*032 e", serial="76B0A1336969"
Found DFU: [0483:df11] ver=2200, devnum=28, cfg=1, intf=0, path="20-1", alt=0, name="@Internal Flash  /0x08000000/1024*256 g", serial="76B0A1336969"
```

The device ID shown here is `0483:df11`.

If your output looks like the above, you are ready to proceed.

### Backup original firmware

To back-up the firmware from the device, we will tell `dfu-util` to *upload* it to us. Use the device ID from the output of running `dfu-util --list` above.

```
dfu-util --device 0483:df11 --dfuse-address 0x08000000 --alt 0 --upload backup.bin
```

This will upload the firmware from the bttn to your computer and save it as `backup.bin`. You may later use this backup to restore your bttn to its original state.

### Installing the firmware

#### Using firmware you've built from source

If you compiled the firmware yourself (above), you can now flash it to your Wi-Fi bttn:

```
cd openbttn/src && make download
```

`make download` has the same effect as manually issuing a `dfu-util` *download* command and requires a Wi-Fi bttn to be connected and the image built from source available as `src/main.bin`.

#### Using a downloaded release image

To install new firmware onto the bttn, we will tell `dfu-util` to *download* it to the device. Use the device ID from `dfu-util --list` above.

In the example below, a release image named `openbttn-v1.0.1.bin` has been downloaded.

Flash the bttn with the image:

```
dfu-util --device 0483:df11 --dfuse-address 0x08000000 --alt 0 --download openbttn-v1.0.1.bin
```

You will see output like the following as the original firmware is overwritten:

```
dfu-util: Invalid DFU suffix signature
dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
Opening DFU capable USB device...
ID 0483:df11
Run-time device DFU version 011a
Claiming USB DFU Interface...
Setting Alternate Setting #0 ...
Determining device status: state = dfuERROR, status = 10
dfuERROR, clearing status
Determining device status: state = dfuIDLE, status = 0
dfuIDLE, continuing
DFU mode device DFU version 011a
Device returned transfer size 2048
DfuSe interface name: "Internal Flash  "
dfu-util: Non-valid multiplier ' ', assuming bytes
Downloading to address = 0x08000000, size = 23288
Download	[=========================] 100%        23288 bytes
Download done.
File downloaded successfully
```

### Take bttn out of DFU mode

1. Unplug the device from power
2. Remove the jumper/short of the boot pins
3. (Optionally) reassemble the unit

### Boot bttn in recovery mode

Plug in the bttn while holding down the button. After 4 seconds it will enter recovery mode and start an open Wi-Fi access point.

1. Next, connect your computer to the `OpenBttn` Wi-Fi network.
2. Open a browser to [http://192.168.1.1/](http://192.168.1.1/)
3. Configure the bttn to use your network's Wi-Fi settings.

*You will not need to re-enter recovery mode unless your Wi-Fi network settings have changed.*

### Live configuration of bttn button press URLs

Use a web browser to connect directly to your bttn once it's on your network. You must know your bttn's IP address which you can find out from your router or DHCP server.

Navigate to `http://[bttn_IP_on_your_network]/` and set your short and long-press URLs.

You may reconfigure your bttn as often as desired.

### Built-in socket server

There are several functions available via the socket server. Note that URLs must be encoded to AT format before being passed to the socket server. Refer to [at.js](public/openbttn/at.js) for more information.

* `dump_config` - return current configuration
* `blink_leds` - change LED colors in a sequence
* `url1` - set short-press URL
* `url2` - set long-press URL
* `password` - set management password

#### Examples

Set a shell variable for the socket server's URL:

```console
$ export BTTNSOCK="http://[bttn_IP_on_your_network]:8774/socket"
```

Read configuration for button press actions:

```console
$ curl -w "\n" --data $'auth = PASSWORD\r\ndump_config' $BTTNSOCK
```

Example output (JSON):

> ```json
> {"url1":"10.1.5.13,/shortPress,4040","url2":"10.1.5.13,/longPress,4040"}
> ```

Turn on the LEDs (white) for 400ms and then them off:

```console
$ curl -w "\n" --data $'auth = PASSWORD\r\nblink_leds = ffffff;400;0;0' $BTTNSOCK
```

Example output (plaintext):

> ```
> Success!
> ```

### Development server

OpenBttn contains a development server written in Go, it can be used for developing the web UI locally or for serving the OTA update for the SPWF01SA module.

Installation via macOS:

```
$ brew install go
```

Usage:

```
go run cmd/openbttn/main.go -ip 192.168.0.123 -ota ./public/ota -public ./public/openbttn
```

`-ip` assumes button is at IP 192.168.0.123, used for redirecting some communication while developing the Web UI.

`-ota` path to folder containing Wi-Fi module OTA image (more on this below)

`-public` path to folder containing Web UI.

The development server serves everything at `http://localhost:8774`.

### OTA update of Wi-Fi module firmware

#### Overview

Depending on when your bttn was built, it may have shipped with an older Wi-Fi module firmware. It's recommended to upgrade this firmware, and the process is stable and safe, but as with all things, your mileage may vary.

As of this writing, version 3.5 is available and brings many improvements over older versions.

#### Updating the Wi-Fi module

Download the latest Wi-Fi module firmware from [here](https://my.st.com/content/my_st_com/en/products/embedded-software/wireless-connectivity-software/stsw-wifi001.html) (requires signing up for a free account).

Unzip it and locate the module firmware version you wish to use.

For our example, we've chosen:
> SPWF01S-160129-c5bf5ce-RELEASE-main.ota

Move the firmware into the `ota` folder:

```
$ mv ~/Downloads/STSW-WIFI001/Rel.\ 3.5/OTA/SPWF01S-160129-c5bf5ce-RELEASE-main.ota public/ota/
```

Run the included web server (more on this in "Development server" above):

```
$ cd public && go run ../cmd/openbttn/main.go
```

Boot bttn in recovery mode (like you did above), but this time get your IP address and use it to form the OTA update URL:

```
$ ifconfig | grep 192.
```

Example output:

> ```
> inet 192.168.1.2 netmask 0xffffff00 broadcast 192.168.1.255
> ```

Here our IP address is `192.168.1.2` so we'll use that to construct our OTA update URL:

```
http://192.168.1.2:8774/ota/SPWF01S-160129-c5bf5ce-RELEASE-main.ota
```

Next, open a web browser to [http://192.168.1.1](http://192.168.1.1) and verify that the update URL shown matches what you expect, and that you can reach that URL yourself (doing so should trigger a file download).

If it's working as expected, you may update the Wi-Fi module firmware. After it has succeeded, you may need to reconnect to the *OpenBttn* Wi-Fi access point to proceed with inputting your network settings.

## Current status

### TODO

* Document how LEDs can be specified or individually addressed in `blink_leds` command

### Nice to have

* OTA update of OpenBttn (we are already able to OTA update the Wi-Fi module, updating OpenBttn remotely would also be nice)
* More ways to interact with the bttn (double press, ulta-long press, etc?)

### Limitations

* We cannot use hardware flow control for the SPWF01SA Wi-Fi module because the CTS/RTS ports of the Wi-Fi module are incorrectly set up in the bttn hardware. The CTS/RTS is set up as output/input whereas is should be the other way around, input/output. This makes it impossible for the bttn (RTS) to send signals to the Wi-Fi module (CTS) and vice-versa.
    * Using hardware flow control would allow us to use a smaller ring buffer and ask the WiFi module to take breaks in sending it's data, thus relying on it's, much larger, RAM.

## Motivation

* I happened to have a bricked (after OTA update) bttn around to play with and wanted some experience with embedded programming
* Local requests in original firmware were too slow
    * A token request to the bt.tn servers were issued before the local request was performed
* Investigate Wi-Fi issues in bttn
    * Turns out it's an issue with the SPWF01SA Wi-Fi module
    * Can be worked around by running the WiFi module in power save mode or by connecting to a different WiFi AP
