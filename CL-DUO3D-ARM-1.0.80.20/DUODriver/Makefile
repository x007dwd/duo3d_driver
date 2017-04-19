# Uncomment the following to enable debugging.
#DEBUG = y

ifeq ($(DEBUG),y)
	DBGFLAGS = -O -g -DML_DEBUG
else
	DBGFLAGS = -O3
endif

ccflags-y += $(DBGFLAGS)

ifneq ($(KERNELRELEASE),)
    obj-m := duo.o

else
#        KERNELDIR ?= ~/Documents/Kernel/kernel
        KERNELDIR ?= ~/Downloads/linux-grinch-21.3.4
        PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif

