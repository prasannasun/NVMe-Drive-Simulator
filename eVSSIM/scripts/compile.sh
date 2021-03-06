#!/bin/bash

# compile osc-osd
cd eVSSIM/osc-osd/
make target
cd ../..

#compile host tests
cd eVSSIM/tests/host
make distclean
make mklink
make
 
#compile monitor
cd eVSSIM/MONITOR/SSD_MONITOR_PM
qmake -o Makefile ssd_monitor_p.pro
make

#compile QEMU
cd ../../QEMU
make distclean
./configure --enable-io-thread --enable-linux-aio --target-list=x86_64-softmmu --enable-sdl --enable-vssim --extra-cflags='-Wno-deprecated-declarations -Wno-unused-but-set-variable'
make
