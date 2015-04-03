Laser Cutter Power Monitor
==================

This repository holds the code for VHS's laser cutter power monitor.  The device's job is to monitor the signals modulating the laser and reporting the duty cycle and frequency of the laser to a server.

The device consists of an ESP8266 connected to an STM32F0 development board.  The STM32F0's purpose is to measure the characteristics of the signal (duty and frequency) and report it back to the ESP via UART.  The ESP interprets the report and sends a json-formatted report back to a server via periodic UDP packets.

The packets are typically sent 4 times per second.  The payload of the UDP datagram is as follows:

    {"f":20133, "d":8312}\n

f - Frequency in hz
d - Duty cycle in percentage * 100.  ie. Above indicates 83.12%

Two binaries need to be built and programmed, one for the ESP and one for the STM32. 

Building the STM32
---------------------

* STM32cubeF0
ST's provided drivers, hal and CMSIS layers

* baremetal arm toolchain (:inaro gcc v. 4.9 2014 q4 is what I used)
A compiler with which you can compile this with! Imagine that.

Steps

1.
Create a build directory in this repo's root and change to that dir. 

    mkdir stm32-build
    cd stm32-build

2.
Navigate ST's website and download version 1.2.0 of STM32CubeF0 and put that into the 'stm32' directory of this repo.

     cp ${MY_DL_DIR}/stm32cubef0.zip .

3.
Run the script to setup dependencies

    ../stm32/scripts/boostrap.sh

If that doesn't work, then just unzip both the STM32F0 cube stuff and this fille: https://launchpad.net/gcc-arm-embedded/4.9/4.9-2014-q4-major/+download/gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2 into the stm32-build directory.

4.
Run:

    cmake ../stm32
    make

FIXME: I seem to need to run cmake TWICE before it actually works right. I have been getting the following errors if not done this way:
    
    /tmp/ccK3UI0L.s:312: Error: selected processor does not support ARM mode `dsb'

Building the ESP8266
------------------------

Based off of: https://github.com/cnlohr/ws2812esp8266
I will update this soon, promise!

