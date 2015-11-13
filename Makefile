obj-m	+= counters.o
obj-m	+= gpio-pulse.o

LINUX_SOURCE=/home/monster/src/armbian.com/linux-source
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SOURCE) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(LINUX_SOURCE) M=$(PWD) clean
