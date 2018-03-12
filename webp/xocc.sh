#!/bin/bash

########################### Settings ###########################

### Software environment ###
export VP8_SDX=/proj/xbuilds/2017.1_sdx_daily_latest/installs/lin64/SDx/2017.1

### DSA setting ###
# aws
export DSA=xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0
export DSA_PLATFORM=xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0
export DSA_DEVICE=$DSA

# # ku115
# export DSA=xilinx:xil-accel-rd-ku115:4ddr-xpr:4.0
# export DSA_PLATFORM=xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0
# export DSA_DEVICE=$DSA

### Nb of instance: 1, 2, 3, 4 ###
export VP8_NBINSTANCES=1

### By default build for hardware can be set to
#     hw_emu for hardware emulation
#     hw for hardware 
export VP8_TARGET=hw_emu

### device frequency ###
export VP8_FREQUENCY=250

### Compiler and linker setting ###
export CXX=$VP8_SDX/Vivado/tps/lnx64/gcc-6.2.0/bin/g++
export LD_LIBRARY_PATH=$VP8_SDX/Vivado/tps/lnx64/gcc-6.2.0/lib64:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$VP8_SDX/Vivado/tps/lnx64/gcc-6.2.0/lib:$LD_LIBRARY_PATH

export PATH=$VP8_SDX/Vivado/tps/lnx64/binutils-2.26/bin:$PATH
export LD_LIBRARY_PATH=$VP8_SDX/Vivado/tps/lnx64/binutils-2.26/lib:$LD_LIBRARY_PATH

#############################################################


# dsa path (some are internal)
export VP8_DEVICE_REPO_PATH=$VP8_SDX/internal_platforms

# emulation
# export XCL_EMULATION_MODE="true"

# host runtime
export LD_LIBRARY_PATH=$VP8_SDX/runtime/lib/x86_64:$VP8_SDX/lib/lnx64.o:$VP8_SDX/Vivado/lib/lnx64.o:$LD_LIBRARY_PATH

$VP8_SDX/bin/sdx -version
$VP8_SDX/bin/xocc --version

# call make
make -f Makefile.xocc compile | tee make_xocc.log
