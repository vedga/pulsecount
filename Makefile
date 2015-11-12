obj-m	+= counters.o
obj-m	+= gpio-pulse.o

LINUX_SOURCE=/home/monster/src/armbian.com/linux-source
THIS_SOURCE=/home/monster/src/pulsecount
ARCH=arm
TOOLCHAIN=/home/monster/src/armbian.com/toolchain/bin
CROSS_COMPILE=arm-linux-gnueabihf-

all:
	export PATH=${PATH}:$(TOOLCHAIN); make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SOURCE) M=$(THIS_SOURCE) modules

clean:
	export PATH=${PATH}:$(TOOLCHAIN); make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SOURCE) M=$(THIS_SOURCE) clean
