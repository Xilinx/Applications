#!/bin/bash

########################### Settings ###########################

### Software environment ###
export WEBP_SDX=/proj/xbuilds/2017.1_sdx_daily_latest/installs/lin64/SDx/2017.1

### DSA setting ###
# aws
export WEBP_DSA=xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0
export WEBP_DSA_PLATFORM=xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0

# # ku115
# export WEBP_DSA=xilinx:xil-accel-rd-ku115:4ddr-xpr:4.0
# export WEBP_DSA_PLATFORM=xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0
# export WEBP_DSA_DEVICE=$DSA

### Nb of instance: 1, 2, 3, 4 ###
export WEBP_NBINSTANCES=1

### By default build for hardware can be set to
#     hw_emu for hardware emulation
#     hw for hardware 
export WEBP_TARGET=hw

### device frequency ###
export WEBP_FREQUENCY=250

### Compiler and linker setting ###
export WEBP_CXX=$WEBP_SDX/Vivado/tps/lnx64/gcc-6.2.0/bin/g++
export LD_LIBRARY_PATH=$WEBP_SDX/Vivado/tps/lnx64/gcc-6.2.0/lib64:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$WEBP_SDX/Vivado/tps/lnx64/gcc-6.2.0/lib:$LD_LIBRARY_PATH

export PATH=$WEBP_SDX/Vivado/tps/lnx64/binutils-2.26/bin:$PATH
export LD_LIBRARY_PATH=$WEBP_SDX/Vivado/tps/lnx64/binutils-2.26/lib:$LD_LIBRARY_PATH

#############################################################

# emulation
# export XCL_EMULATION_MODE="true"

# host runtime
export LD_LIBRARY_PATH=$WEBP_SDX/runtime/lib/x86_64:$WEBP_SDX/lib/lnx64.o:$WEBP_SDX/Vivado/lib/lnx64.o:$LD_LIBRARY_PATH

$WEBP_SDX/bin/sdx -version
$WEBP_SDX/bin/xocc --version

# call make
make -f Makefile.xocc compile | tee make_xocc.log
