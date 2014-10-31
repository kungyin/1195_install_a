# You can adjust the toolchain as you need
#TOOLCHAIN_PATH = ~/toolchain/asdk-4.8.1-a7-EL-3.10-0.9.33-a32nt-130828/
TOOLCHAIN_PATH = /home/flash/Realtek-1195/TRUNK/SDK/bootcode/tmp/asdk-4.8.1-a7-EL-3.10-0.9.33-a32nt-130828/

CROSS_COMPILER = arm-linux
#CROSS_COMPILER = arm-linux-gnueabihf
rm=/bin/rm -f
cp=/bin/cp -f
CC= $(TOOLCHAIN_PATH)/bin/$(CROSS_COMPILER)-g++
AR= $(TOOLCHAIN_PATH)/bin/$(CROSS_COMPILER)-ar cr
RANLIB=$(TOOLCHAIN_PATH)/bin/$(CROSS_COMPILER)-ranlib
STRIP=$(TOOLCHAIN_PATH)/bin/$(CROSS_COMPILER)-strip

REV=684895M

INCS= -I./include -I./
CFLAGS =  -g -Os -march=armv7-a -Wall

OBJS= src/rtk_main.o src/rtk_burn.o src/rtk_urltar.o src/rtk_tar.o src/rtk_imgdesc.o src/rtk_fwdesc.o src/rtk_mtd.o src/rtk_common.o src/rtk_config.o src/rtk_factory.o src/rtk_parameter.o src/rtk_tagflow.o src/rtk_boottable.o src/rtk_customer.o

LIBS += -Wl,--start-group -L./lib/ -lMCP -lion -lpthread -lefuse -Wl,--end-group

DEFINES += -DEMMC_SUPPORT
#DEFINES += -DUPDATE_8198C_FW
CFLAGS += $(INCS) $(DEFINES)

all: subdir install

subdir:

install: $(OBJS) install_a.cpp
	$(shell echo "#define SVN_REV \"$(REV)\"" > include/svnver.h)
	$(CC) $(CFLAGS) -c install_a.cpp
	$(CC) -o install_a $(CFLAGS) $(OBJS) install_a.o $(LIBS) -static
	$(STRIP) install_a

clean:
	rm -rf $(OBJS) *.a *.o *.bak mapfile rtktest install_a oldup newup ./src/*.bak ./include/*.bak *.s *.ii customer

%.o: %.cpp
	$(CC) -c $< $(CFLAGS) -o $@

%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@
