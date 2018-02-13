#!/bin/bash

########################### Settings ###########################

### Software environment ###
VP8_SDX=/proj/xbuilds/2017.1_sdx_daily_latest

### DSA setting ###
# aws
export DSA=xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0
export DSA_PLATFORM=xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0
export DSA_DEVICE=$DSA

# ku115
# export DSA_PLATFORM=xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0
# export DSA=$DSA_PLATFORM
# export DSA_DEVICE=

### Nb of instance: 1, 2, 3, 4 ###
export VP8_NBINSTANCES=1

### By default build for hardware can be set to
#     hw_emu for hardware emulation
#     hw for hardware 
export VP8_TARGET=hw_emu

### device frequency ###
export VP8_FREQUENCY=250


#############################################################


export XILINX_SDX=$VP8_SDX/installs/lin64/SDx/2017.1
export XILINX_SDK=$VP8_SDX/installs/lin64/SDx/2017.1/SDK
export XILINX_SDACCEL=$XILINX_SDX
export XBINST_DSA_PATH=${XILINX_DSA_PATH:=$XILINX_SDX/platforms}

# dsa path (some are internal)
export VP8_DEVICE_REPO_PATH=$VP8_SDX/internal_platforms

$XILINX_SDX/bin/sdx -version
$XILINX_SDX/bin/xocc --version

# emulation
# export XCL_EMULATION_MODE="true"

# host runtime + kernel bin
export XILINX_OPENCL=$XILINX_SDX
export XILINX_VIVADO=${SDX_VIVADO:=$XILINX_SDX/Vivado}
export LD_LIBRARY_PATH=$XILINX_SDX/runtime/lib/x86_64:$XILINX_SDX/lib/lnx64.o:$XILINX_VIVADO/lib/lnx64.o:$XILINX_SDX/lnx64/tools/opencv:$LD_LIBRARY_PAT

# set g++ path
export CXX=${SDX_CXX_PATH:=/tools/batonroot/rodin/devkits/lnx64/gcc-6.1.0/bin/g++}
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/tools/batonroot/rodin/devkits/lnx64/gcc-6.1.0/lib64

# call make
make -f Makefile.xocc compile | tee make_xocc.log
