#LZ4 Settings
################################################################################
PARALLEL_BLOCK:=8
################################################################################

COMMON_REPO := ../
#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk

NPB:=$(PARALLEL_BLOCK)b

HOST_NAME:=xil_lz4_$(NPB)

XO1_C_NAME:=xil_lz4_xo_compress_$(NPB)
XO1_D_NAME:=xil_lz4_xo_decompress_$(NPB)

XCLBIN_C:=xil_lz4_compress_$(NPB)
XCLBIN_D:=xil_lz4_decompress_$(NPB)

# Host Application
$(HOST_NAME)_SRCS=./src/host.cpp $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
$(HOST_NAME)_SRCS+=./src/xil_lz4.cpp
$(HOST_NAME)_SRCS+=./xxhash/xxhash.c
$(HOST_NAME)_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS) 
$(HOST_NAME)_HDRS+=./src/xil_lz4_config.h 
$(HOST_NAME)_HDRS+=./src/xil_lz4.h
$(HOST_NAME)_HDRS+=./xxhash/xxhash.h
$(HOST_NAME)_CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS)
$(HOST_NAME)_CXXFLAGS+=-I./xxhash/ 
$(HOST_NAME)_CXXFLAGS+= -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
$(HOST_NAME)_LDFLAGS=$(opencl_LDFLAGS) 
$(HOST_NAME)_LDFLAGS+=-lrt 
$(HOST_NAME)_LDFLAGS+=-Wno-unused-label 
$(HOST_NAME)_LDFLAGS+=-Wno-narrowing 
$(HOST_NAME)_LDFLAGS+=-std=c++0x
$(HOST_NAME)_LDFLAGS+=-DVEC_8
$(HOST_NAME)_LDFLAGS+=-DVERBOSE

# Kernel
$(XO1_C_NAME)_SRCS=./src/xil_lz4_compress_kernel.cpp 

$(XO1_D_NAME)_SRCS=./src/xil_lz4_decompress_kernel.cpp 

$(XO1_C_NAME)_HDRS=./src/xil_lz4_config.h

$(XO1_D_NAME)_HDRS=./src/xil_lz4_config.h

K_CLFLAGS=--kernel_frequency=250 -g -DVEC_8 --xp "param:compiler.enableAutoPipelining=false" -I./src/ -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)

$(XO1_C_NAME)_CLFLAGS=-k xil_lz4_cu1 $(K_CLFLAGS)
$(XO1_D_NAME)_CLFLAGS=-k xil_lz4_dec_cu1 $(K_CLFLAGS)

$(XCLBIN_C)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1 
$(XCLBIN_D)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1

XO_C_NAMES = $(XO1_C_NAME)

XO_D_NAMES = $(XO1_D_NAME)

XOS=$(XO_C_NAMES) $(XO_D_NAMES)

$(XCLBIN_C)_XOS=$(XO_C_NAMES)
$(XCLBIN_D)_XOS=$(XO_D_NAMES)

EXES=$(HOST_NAME)
XCLBINS=$(XCLBIN_C) $(XCLBIN_D)

# check
check_EXE=$(HOST_NAME)
check_XCLBINS=$(XCLBIN_C) $(XCLBIN_D)
#check_ARGS= -l ./largecanter.list
check_ARGS= -l ./cantrbry.list
#check_ARGS= -l ./test.list 
#check_ARGS= -l ./silesia.list

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

EXTRA_CLEAN=../benchmark/*.lz4
