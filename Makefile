obj-$(CONFIG_SDBPK) := sdbpk.o

sdbpk-y = sdbp.o crc16ccitt.o descriptor.o communication.o attributes.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
