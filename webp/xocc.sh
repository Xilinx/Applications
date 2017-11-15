#!/bin/env bash
if [ "$RDI_REGR_SCRIPTS" != "" ]; then
	WRAPPER=$RDI_ROOT/hierdesign/fisInfra/sprite/bin/rdiWrapper
else
  export RDI_REGR_SCRIPTS=/proj/rdi/env/stable/rdi/util/regression
	WRAPPER=/proj/rdi/env/stable/hierdesign/fisInfra/sprite/bin/rdiWrapper
fi
. $RDI_REGR_SCRIPTS/init.sh
pragma_normal

VP8_SDX=/proj/xbuilds/2017.4_daily_latest

# set sdx build
# export XILINX_SDX=$VP8_SDX/installs/lin64/SDx/2017.4
# export XILINX_SDK=$VP8_SDX/installs/lin64/SDx/2017.4/SDK

# export XILINX_SDACCEL=$XILINX_SDX
# export XBINST_DSA_PATH=${XILINX_DSA_PATH:=$XILINX_SDX/platforms}

# VP8_SDX=/proj/xbuilds/2017.2_sdx_daily_latest

# # set sdx build
# export XILINX_SDX=$VP8_SDX/installs/lin64/SDx/2017.2
# export XILINX_SDK=$VP8_SDX/installs/lin64/SDx/2017.2/SDK

# export XILINX_SDACCEL=$XILINX_SDX
# export XBINST_DSA_PATH=${XILINX_DSA_PATH:=$XILINX_SDX/platforms}


VP8_SDX=/proj/xbuilds/2017.1_sdx_daily_latest
export XILINX_SDX=$VP8_SDX/installs/lin64/SDx/2017.1
export XILINX_SDK=$VP8_SDX/installs/lin64/SDx/2017.1/SDK
export XILINX_SDACCEL=$XILINX_SDX
export XBINST_DSA_PATH=${XILINX_DSA_PATH:=$XILINX_SDX/platforms}

# aws
export DSA=xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0
export DSA_PLATFORM=xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0
export DSA_DEVICE=$DSA

# # vu9p
# export DSA_PLATFORM=xilinx_xil-accel-rd-vu9p_4ddr-xpr_4_2
# export DSA=$DSA_PLATFORM
# export DSA_DEVICE=xilinx:xil-accel-rd-vu9p:4ddr-xpr:4.2

# ku115
# export DSA_PLATFORM=xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0
# export DSA=$DSA_PLATFORM
# export DSA_DEVICE=

# dsa path (some are internal)
export VP8_DEVICE_REPO_PATH=$VP8_SDX/internal_platforms

# nb of instances: 1, 2, 3, or 4
export VP8_NBINSTANCES=1

# By default build for hardware can be set to
#   hw_emu for hardware emulation
#   hw for hardware 
export VP8_TARGET=hw_emu

# device frequency
export VP8_FREQUENCY=250

if [[ $DSA_PLATFORM =~ .*_minotaur-.* || $DSA_PLATFORM =~ .*_aws-.*  ]]; then
  export XBINST_COPY_AWSSAK=1
fi

if [[ $DSA_PLATFORM =~ .*_4_0 || $DSA_PLATFORM =~ .*_4_1 ]]; then
  export XBINST_COPY_XBSAK_NG=1
fi

if [ -z "$LINK_EXTRA_FLAGS" ]; then
  export LINK_EXTRA_FLAGS=""
else
  export LINK_EXTRA_FLAGS
fi

if [ -z "$COMPILE_EXTRA_FLAGS" ]; then
  export COMPILE_EXTRA_FLAGS=""
else
  export COMPILE_EXTRA_FLAGS
fi

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

if [ $VP8_TARGET == hw ]; then
	make -f Makefile.xocc compile | tee make_xocc.log
	mstatus=${PIPESTATUS[0]}
else
	make -f Makefile.xocc -j4 compile | tee make_xocc.log
	mstatus=${PIPESTATUS[0]}
fi


if [[ $DSA_PLATFORM =~ .*_minotaur-.* || $DSA_PLATFORM =~ .*_aws-.* ]]; then
  export XBINST_COPY_AWSSAK=0
fi

if [[ $DSA_PLATFORM =~ .*_4_0 || $DSA_PLATFORM =~ .*_4_1 ]]; then
  export XBINST_COPY_XBSAK_NG=0
fi

if [ $mstatus -eq 0 ]; then
	echo "MAKE PASSED. STATUS = $mstatus"
else
	echo "MAKE FAILED. STATUS = $mstatus"
	
	if [ $mstatus == 2 ]; then
		exit 1
	else
		exit $mstatus
	fi
	
fi
