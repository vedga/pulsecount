obj-m	+= counters.o
obj-m	+= gpio-pulse.o

all:
	export PATH=${PATH}:/home/monster/src/armbian.com/toolchain/bin; make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/monster/src/armbian.com/linux-source M=/home/monster/src/pulsecount modules

clean:
	export PATH=${PATH}:/home/monster/src/armbian.com/toolchain/bin; make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/monster/src/armbian.com/linux-source M=/home/monster/src/pulsecount clean
