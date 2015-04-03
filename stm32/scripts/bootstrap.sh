#!/bin/bash


if [ ! -e gcc-arm-none-eabi-4_9-2014q4 ]; then
	echo "Downloading toolchain"
	wget https://launchpad.net/gcc-arm-embedded/4.9/4.9-2014-q4-major/+download/gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2
#	wget ftp://localhost/gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2
	tar xf gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2
	if [ ! $? == 0 ]; then
		echo "Failed"
		exit 1
	else
		echo "Success"
	fi
else
	echo "Toolchain unpacked... skipping"
fi


if [ ! -e STM32Cube_FW_F0_V1.2.0 ]; then
	echo "Unpacking STM32Cube..."
	if [ ! -e stm32cubef0.zip ]; then
		echo "Could not find stm32cubef0.zip... download this from the ST webiste"
		exit 1
	fi
	unzip -q stm32cubef0.zip
	if [ ! $? == 0 ]; then
		echo "Failed"
		exit 1
	else
		echo "Success"
	fi
else
	echo "Already have STM32Cube... skipping"
fi
