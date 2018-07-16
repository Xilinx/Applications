#LZ4 Settings
################################################################################
C_COMPUTE_UNITS:=2
D_COMPUTE_UNITS:=2
PARALLEL_BLOCK:=8
SINGLE_XCLBIN:=true
################################################################################

COMMON_REPO := ../
#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk
include $(COMMON_REPO)/libs/stream_utils/stream_utils.mk
include $(COMMON_REPO)/libs/lz_compress/lz_compress.mk

NPB:=$(PARALLEL_BLOCK)b

# Host Executable
HOST_NAME:=xil_lz4_$(NPB)

# Binary
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	XCLBIN:=xil_lz4_compress_decompress_$(NPB)
else	
	XCLBIN_C:=xil_lz4_compress_$(NPB)
	XCLBIN_D:=xil_lz4_decompress_$(NPB)
endif

# Kernel Flags
K_CLFLAGS=--kernel_frequency=230 --xp "param:compiler.enableAutoPipelining=false" -I./src/ -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
K_CLFLAGS+=$(stream_utils_CLFLAGS) $(lz_compress_CLFLAGS)

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
$(HOST_NAME)_CXXFLAGS+= -DC_COMPUTE_UNIT=$(C_COMPUTE_UNITS)
$(HOST_NAME)_CXXFLAGS+= -DD_COMPUTE_UNIT=$(D_COMPUTE_UNITS)
$(HOST_NAME)_CXXFLAGS+= -DSINGLE_XCLBIN=$(SINGLE_XCLBIN)
# Enable following flag if kernel, read, write events
# information is required, by default disabled
#$(HOST_NAME)_CXXFLAGS+= -DEVENT_PROFILE
$(HOST_NAME)_LDFLAGS=$(opencl_LDFLAGS) 
$(HOST_NAME)_LDFLAGS+=-lrt 
$(HOST_NAME)_LDFLAGS+=-Wno-unused-label 
$(HOST_NAME)_LDFLAGS+=-Wno-narrowing 
$(HOST_NAME)_LDFLAGS+=-std=c++0x
$(HOST_NAME)_LDFLAGS+=-DVERBOSE

ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1
else
	$(XCLBIN_C)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1
	$(XCLBIN_D)_LDCLFLAGS+=--xp vivado_param:project.writeIntermediateCheckpoints=1
endif
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--kernel_frequency=230
else
	$(XCLBIN_C)_LDCLFLAGS+=--kernel_frequency=230
	$(XCLBIN_D)_LDCLFLAGS+=--kernel_frequency=230
endif

#####################
# Compression Setup #
#####################
XO1_C_NAME:=xil_lz4_xo_compress_$(NPB)_cu1
XO_C_NAMES = $(XO1_C_NAME)
$(XO1_C_NAME)_SRCS=./src/xil_lz4_compress_kernel.cpp 
$(XO1_C_NAME)_HDRS=./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS)
$(XO1_C_NAME)_CLFLAGS=-k xil_lz4_cu1 $(K_CLFLAGS) -DC_COMPUTE_UNIT=1

ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_cu1_1.m_axi_gmem0:bank0
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_cu1_1.m_axi_gmem1:bank0
else
	$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lz4_cu1_1.m_axi_gmem0:bank0
	$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lz4_cu1_1.m_axi_gmem1:bank0
endif

ifneq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN_C)_LDCLFLAGS+=--xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/userPostSysLinkcomp.tcl
	$(XCLBIN_D)_LDCLFLAGS+=--xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/userPostSysLinkdecomp.tcl
endif

# Two Compute units
ifneq ($(C_COMPUTE_UNITS),1)
XO2_C_NAME:=xil_lz4_xo_compress_$(NPB)_cu2
XO_C_NAMES += $(XO2_C_NAME)
$(XO2_C_NAME)_SRCS=./src/xil_lz4_compress_kernel.cpp
$(XO2_C_NAME)_HDRS=./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS)
$(XO2_C_NAME)_CLFLAGS=-k xil_lz4_cu2 $(K_CLFLAGS) -DC_COMPUTE_UNIT=2
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_cu2_1.m_axi_gmem0:bank3
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_cu2_1.m_axi_gmem1:bank3
	$(XCLBIN)_LDCLFLAGS+=--xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/userPostSysLink.tcl
else
	$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lz4_cu2_1.m_axi_gmem0:bank3
	$(XCLBIN_C)_LDCLFLAGS+=--sp xil_lz4_cu2_1.m_axi_gmem1:bank3
endif
endif

#######################
# Decompression Setup #
#######################
XO1_D_NAME:=xil_lz4_xo_decompress_$(NPB)_cu1
XO_D_NAMES = $(XO1_D_NAME)
$(XO1_D_NAME)_SRCS=./src/xil_lz4_decompress_kernel.cpp 
$(XO1_D_NAME)_HDRS=./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS)
$(XO1_D_NAME)_CLFLAGS=-k xil_lz4_dec_cu1 $(K_CLFLAGS) -DD_COMPUTE_UNIT=1
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_dec_cu1_1.m_axi_gmem0:bank1
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_dec_cu1_1.m_axi_gmem1:bank1
else
	$(XCLBIN_D)_LDCLFLAGS+=--sp xil_lz4_dec_cu1_1.m_axi_gmem0:bank1
	$(XCLBIN_D)_LDCLFLAGS+=--sp xil_lz4_dec_cu1_1.m_axi_gmem1:bank1
endif
# Two Compute Units
ifneq ($(D_COMPUTE_UNITS),1)
XO2_D_NAME:=xil_lz4_xo_decompress_$(NPB)_cu2
XO_D_NAMES += $(XO2_D_NAME)
$(XO2_D_NAME)_SRCS=./src/xil_lz4_decompress_kernel.cpp 
$(XO2_D_NAME)_HDRS=./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS)
$(XO2_D_NAME)_CLFLAGS=-k xil_lz4_dec_cu2 $(K_CLFLAGS) -DD_COMPUTE_UNIT=2
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_dec_cu2_1.m_axi_gmem0:bank2
	$(XCLBIN)_LDCLFLAGS+=--sp xil_lz4_dec_cu2_1.m_axi_gmem1:bank2
	$(XCLBIN)_LDCLFLAGS+=--xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/userPostSysLink.tcl
else
	$(XCLBIN_D)_LDCLFLAGS+=--sp xil_lz4_dec_cu2_1.m_axi_gmem0:bank2
	$(XCLBIN_D)_LDCLFLAGS+=--sp xil_lz4_dec_cu2_1.m_axi_gmem1:bank2
endif
endif

XOS=$(XO_C_NAMES) $(XO_D_NAMES)

ifeq ($(strip $(SINGLE_XCLBIN)),true)
	$(XCLBIN)_XOS=$(XO_C_NAMES) $(XO_D_NAMES)
else
	$(XCLBIN_C)_XOS=$(XO_C_NAMES)
	$(XCLBIN_D)_XOS=$(XO_D_NAMES)
endif

EXES=$(HOST_NAME)

ifeq ($(strip $(SINGLE_XCLBIN)),true)
	XCLBINS=$(XCLBIN)
else
	XCLBINS=$(XCLBIN_C) $(XCLBIN_D)
endif

# check
check_EXE=$(HOST_NAME)
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	check_XCLBINS=$(XCLBIN)
else
	check_XCLBINS=$(XCLBIN_C) $(XCLBIN_D)
endif
#check_ARGS= -l ./largecanter.list -x 3
check_ARGS= -l ./cantrbry.list
#check_ARGS= -l ./test.list 
#check_ARGS= -l ./silesia.list -x 3

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

EXTRA_CLEAN=../benchmark/*.lz4
