#LZMA Settings
################################################################################
C_COMPUTE_UNITS:=1
PARALLEL_BLOCK:=1
SINGLE_XCLBIN:=false
################################################################################
#PROFILE:=yes
MFREQ:=250
COMMON_REPO := ../
#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk
include $(COMMON_REPO)/libs/stream_utils/stream_utils.mk
include $(COMMON_REPO)/xil_lzma/lz_compress/lz_compress.mk

NPB:=$(PARALLEL_BLOCK)b

# Host Executable
HOST_NAME:=xil_lzma_$(NPB)

XCLBIN_C:=xil_lzma_compress_$(NPB)

# Kernel Flags
K_CLFLAGS=--kernel_frequency=$(MFREQ) --xp "param:compiler.enableAutoPipelining=false" -I./src/ -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
K_CLFLAGS+=$(stream_utils_CLFLAGS) $(lz_compress_CLFLAGS)

# Host Application
$(HOST_NAME)_SRCS=./src/host.cpp $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
$(HOST_NAME)_SRCS+=./src/xil_lzma.cpp

$(HOST_NAME)_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS) 

$(HOST_NAME)_HDRS+=./src/xil_lzma_config.h 
$(HOST_NAME)_HDRS+=./src/xil_lzma.h
$(HOST_NAME)_CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS)
$(HOST_NAME)_CXXFLAGS+=-I./xxhash/ 
$(HOST_NAME)_CXXFLAGS+= -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
$(HOST_NAME)_CXXFLAGS+= -DC_COMPUTE_UNIT=$(C_COMPUTE_UNITS)
$(HOST_NAME)_CXXFLAGS+= -DSINGLE_XCLBIN=$(SINGLE_XCLBIN)
# Enable following flag if kernel, read, write events
# information is required, by default disabled
#$(HOST_NAME)_CXXFLAGS+= -DEVENT_PROFILE
$(HOST_NAME)_LDFLAGS=$(opencl_LDFLAGS) 
$(HOST_NAME)_LDFLAGS+=-lrt 
$(HOST_NAME)_LDFLAGS+=-Wno-unused-label 
$(HOST_NAME)_LDFLAGS+=-Wno-narrowing 
$(HOST_NAME)_LDFLAGS+=-std=c++0x #-g
$(HOST_NAME)_LDFLAGS+=-DVERBOSE 

$(XCLBIN_C)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1
$(XCLBIN_C)_LDCLFLAGS+=--kernel_frequency=$(MFREQ)

#####################
# Compression Setup #
#####################
XO1_C_NAME:=xil_lzma_xo_compress_$(NPB)_cu1
XO_C_NAMES = $(XO1_C_NAME)
$(XO1_C_NAME)_SRCS=./src/xil_lzma_compress_kernel.cpp 
$(XO1_C_NAME)_HDRS=./src/xil_lzma_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lzma_rc_kernel.h
$(XO1_C_NAME)_CLFLAGS=-k xil_lzma_cu1 $(K_CLFLAGS) -DC_COMPUTE_UNIT=1 

$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lzma_cu1_1.m_axi_gmem0:bank0
$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lzma_cu1_1.m_axi_gmem1:bank0
$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lzma_cu1_1.m_axi_gmem2:bank1
#$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lzma_cu1_1.m_axi_gmem3:bank2

XOS=$(XO_C_NAMES) 

$(XCLBIN_C)_XOS=$(XO_C_NAMES)

EXES=$(HOST_NAME)

XCLBINS=$(XCLBIN_C)

# check
check_EXE=$(HOST_NAME)
check_XCLBINS=$(XCLBIN_C) 
check_ARGS= -c ../benchmark/cantrbry/alice29.txt 

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

EXTRA_CLEAN=../benchmark/*.xz
