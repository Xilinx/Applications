.PHONY: help

help::
	$(ECHO) "Makefile Usage:"
	$(ECHO) "  make all TARGET=<sw_emu/hw_emu/hw> DEVICE=<FPGA platform>"
	$(ECHO) "      Command to generate the design for specified Target and Device."
	$(ECHO) ""
	$(ECHO) "  make clean "
	$(ECHO) "      Command to remove the generated non-hardware files."
	$(ECHO) ""
	$(ECHO) "  make cleanall"
	$(ECHO) "      Command to remove all the generated files."
	$(ECHO) ""
	$(ECHO) "  make check TARGET=<sw_emu/hw_emu/hw> DEVICE=<FPGA platform>"
	$(ECHO) "      Command to run application in emulation."
	$(ECHO) ""

COMMON_REPO = ../
include ./utils.mk

TARGETS := hw
TARGET := $(TARGETS)
DEVICES := xilinx_vcu1525_dynamic
DEVICE := $(DEVICES)
DSA := $(call device2sandsa, $(DEVICE))

CXX := $(XILINX_SDX)/bin/xcpp
XOCC := $(XILINX_SDX)/bin/xocc

#Include Libraries
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk
include $(COMMON_REPO)/libs/stream_utils/stream_utils.mk
include $(COMMON_REPO)/libs/lz_compress/lz_compress.mk

#LZ4 Settings
################################################################################
C_COMPUTE_UNITS:=2
D_COMPUTE_UNITS:=2
PARALLEL_BLOCK:=8
SINGLE_XCLBIN:=false
################################################################################

NPB:=$(PARALLEL_BLOCK)b

# Host Executable
EXECUTABLE:=xil_lz4_$(NPB)

EMCONFIG_DIR = $(XCLBIN)/$(DSA)

# Host Application
HOST_SRCS=./src/host.cpp $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
HOST_SRCS+=./src/xil_lz4.cpp
HOST_SRCS+=./xxhash/xxhash.c
HOST_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS) 
HOST_HDRS+=./src/xil_lz4_config.h
HOST_HDRS+=./src/xil_lz4.h
HOST_HDRS+=./xxhash/xxhash.h
HOST_HDRS+=$(stream_utils_HDRS) $(lz_compress_HDRS)
HOST_HDRS+=./src/xil_lz4_compress_engine.h ./src/xil_lz4_decompress_engine.h
CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS)
CXXFLAGS+=-I./xxhash/ 
CXXFLAGS+= -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
CXXFLAGS+= -DC_COMPUTE_UNIT=$(C_COMPUTE_UNITS)
CXXFLAGS+= -DD_COMPUTE_UNIT=$(D_COMPUTE_UNITS)
CXXFLAGS+= -DSINGLE_XCLBIN=$(SINGLE_XCLBIN)
CXXFLAGS+= -DOVERLAP_HOST_DEVICE
CLFLAGS= -t $(TARGET) --platform $(DEVICE)
# Enable following flag if kernel, read, write events
# information is required, by default disabled
#$(HOST_NAME)_CXXFLAGS+= -DEVENT_PROFILE
LDFLAGS=$(opencl_LDFLAGS) 
LDFLAGS+=-lrt 
LDFLAGS+=-Wno-unused-label 
LDFLAGS+=-Wno-narrowing
LDFLAGS+=-std=c++0x
LDFLAGS+=-DVERBOSE

XCLBIN = ./xclbin

# Binary
ifeq ($(strip $(SINGLE_XCLBIN)),true)
	BINARY_CONTAINERS += $(XCLBIN)/xil_lz4_compress_decompress_$(NPB).$(TARGET).$(DSA).xclbin

	BINARY_CONTAINERS_xil_lz4_compress_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu1.$(TARGET).$(DSA).xo
        ifeq ($(strip $(C_COMPUTE_UNITS)), 2)
		BINARY_CONTAINERS_xil_lz4_compress_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu2.$(TARGET).$(DSA).xo	 	
        endif
	
	BINARY_CONTAINERS_xil_lz4_compress_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu1.$(TARGET).$(DSA).xo
        ifeq ($(strip $(D_COMPUTE_UNITS)), 2)
		BINARY_CONTAINERS_xil_lz4_compress_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu2.$(TARGET).$(DSA).xo	 	
        endif
else
	BINARY_CONTAINERS += $(XCLBIN)/xil_lz4_compress_$(NPB).$(TARGET).$(DSA).xclbin
	BINARY_CONTAINERS += $(XCLBIN)/xil_lz4_decompress_$(NPB).$(TARGET).$(DSA).xclbin
	
	BINARY_CONTAINERS_xil_lz4_compress_OBJS += $(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu1.$(TARGET).$(DSA).xo
        ifeq ($(strip $(C_COMPUTE_UNITS)), 2)
		BINARY_CONTAINERS_xil_lz4_compress_OBJS += $(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu2.$(TARGET).$(DSA).xo
        endif

	BINARY_CONTAINERS_xil_lz4_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu1.$(TARGET).$(DSA).xo
        ifeq ($(strip $(D_COMPUTE_UNITS)), 2)	
		BINARY_CONTAINERS_xil_lz4_decompress_OBJS += $(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu2.$(TARGET).$(DSA).xo
        endif
endif

CP = cp -rf

.PHONY: all clean cleanall docs emconfig
all: $(EXECUTABLE) $(BINARY_CONTAINERS) emconfig

.PHONY: exe
exe: $(EXECUTABLE)

# Kernel linker flags
C_LDCLFLAGS= --sp xil_lz4_cu1_1.m_axi_gmem0:bank0 
C_LDCLFLAGS+= --sp xil_lz4_cu1_1.m_axi_gmem1:bank0
D_LDCLFLAGS= --sp xil_lz4_dec_cu1_1.m_axi_gmem0:bank1
D_LDCLFLAGS+= --sp xil_lz4_dec_cu1_1.m_axi_gmem1:bank1
LDCLFLAGS+= --xp vivado_param:project.writeIntermediateCheckpoints=1 
	
ifeq ($(strip $(C_COMPUTE_UNITS)), 2)
	C_LDCLFLAGS+= --sp xil_lz4_cu2_1.m_axi_gmem0:bank3
	C_LDCLFLAGS+= --sp xil_lz4_cu2_1.m_axi_gmem1:bank3
endif
ifeq ($(strip $(D_COMPUTE_UNITS)), 2)
	D_LDCLFLAGS+= --sp xil_lz4_dec_cu2_1.m_axi_gmem0:bank2
	D_LDCLFLAGS+= --sp xil_lz4_dec_cu2_1.m_axi_gmem1:bank2
endif

BUILD_DIR := ./_x.$(TARGET).$(DSA)
BUILD_DIR_compress := $(BUILD_DIR)/compress
BUILD_DIR_decompress := $(BUILD_DIR)/decompress

K_CLFLAGS=--xp "param:compiler.enableAutoPipelining=false" -I./src/ -DPARALLEL_BLOCK=$(PARALLEL_BLOCK) 
K_CLFLAGS+=$(stream_utils_CLFLAGS) $(lz_compress_CLFLAGS)

$(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu1.$(TARGET).$(DSA).xo: ./src/xil_lz4_compress_kernel.cpp ./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lz4_compress_engine.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lz4_cu1 $(K_CLFLAGS) -DC_COMPUTE_UNIT=1 -o'$@' $(CLFLAGS) -I'$(<D)' '$<'
   	
$(XCLBIN)/xil_lz4_xo_compress_$(NPB)_cu2.$(TARGET).$(DSA).xo: ./src/xil_lz4_compress_kernel.cpp ./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lz4_compress_engine.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lz4_cu2 $(K_CLFLAGS) -DC_COMPUTE_UNIT=2 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 

$(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu1.$(TARGET).$(DSA).xo: ./src/xil_lz4_decompress_kernel.cpp ./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lz4_decompress_engine.h
	$(XOCC) --temp_dir $(BUILD_DIR_decompress) --report_dir reports/$(BUILD_DIR_decompress) -c -s -k xil_lz4_dec_cu1 $(K_CLFLAGS) -DD_COMPUTE_UNIT=1 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 

$(XCLBIN)/xil_lz4_xo_decompress_$(NPB)_cu2.$(TARGET).$(DSA).xo: ./src/xil_lz4_decompress_kernel.cpp ./src/xil_lz4_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lz4_decompress_engine.h
	$(XOCC) --temp_dir $(BUILD_DIR_decompress) --report_dir reports/$(BUILD_DIR_decompress) -c -s -k xil_lz4_dec_cu2 $(K_CLFLAGS) -DD_COMPUTE_UNIT=2 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 


ifeq ($(strip $(SINGLE_XCLBIN)),true)
$(XCLBIN)/xil_lz4_compress_decompress_$(NPB).$(TARGET).$(DSA).xclbin: $(BINARY_CONTAINERS_xil_lz4_compress_decompress_OBJS)
	mkdir -p $(XCLBIN)
ifeq ($(strip $(C_COMPUTE_UNITS)), 1)
	$(XOCC) --report_dir reports/$(BUILD_DIR_compress) $(CLFLAGS) -l $(LDCLFLAGS) $(C_LDCLFLAGS) $(D_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/single_compute_units/userPostSysLink.tcl -o'$@' $(+)
else
	$(XOCC) --report_dir reports/$(BUILD_DIR_compress) $(CLFLAGS) -l $(LDCLFLAGS) $(C_LDCLFLAGS) $(D_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/two_compute_units/userPostSysLink.tcl -o'$@' $(+)
endif
else    
$(XCLBIN)/xil_lz4_compress_$(NPB).$(TARGET).$(DSA).xclbin: $(BINARY_CONTAINERS_xil_lz4_compress_OBJS)
	mkdir -p $(XCLBIN)
ifeq ($(strip $(C_COMPUTE_UNITS)), 1)
	$(XOCC) $(CLFLAGS) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -l $(LDCLFLAGS) $(C_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/single_compute_units/userPostSysLinkcomp.tcl -o'$@' $(+)
else
	$(XOCC) $(CLFLAGS) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -l $(LDCLFLAGS) $(C_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/two_compute_units/userPostSysLinkcomp.tcl -o'$@' $(+)
endif

$(XCLBIN)/xil_lz4_decompress_$(NPB).$(TARGET).$(DSA).xclbin: $(BINARY_CONTAINERS_xil_lz4_decompress_OBJS)
	mkdir -p $(XCLBIN)
ifeq ($(strip $(D_COMPUTE_UNITS)), 1)
	$(XOCC) $(CLFLAGS) --temp_dir $(BUILD_DIR_decompress) --report_dir reports/$(BUILD_DIR_decompress) -l $(LDCLFLAGS) $(D_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/single_compute_units/userPostSysLinkdecomp.tcl -o'$@' $(+)
else
	$(XOCC) $(CLFLAGS) --temp_dir $(BUILD_DIR_decompress) --report_dir reports/$(BUILD_DIR_decompress) -l $(LDCLFLAGS) $(D_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/two_compute_units/userPostSysLinkdecomp.tcl -o'$@' $(+)
endif
endif

# Building Host
$(EXECUTABLE): $(HOST_SRCS)
	mkdir -p $(XCLBIN)
	$(CXX) $(CXXFLAGS) $(HOST_SRCS) -o $@ $(LDFLAGS)

emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(DEVICE) --od $(EMCONFIG_DIR)

check: all
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	$(CP) $(EMCONFIG_DIR)/emconfig.json .
	XCL_EMULATION_MODE=$(TARGET) ./$(EXECUTABLE) -l ./cantrbry.list
else
	 ./$(EXECUTABLE) -l ./cantrbry.list
endif

# Cleaning stuff
clean:
	-$(RMDIR) $(EXECUTABLE) $(XCLBIN)/{*sw_emu*,*hw_emu*} 
	-$(RMDIR) sdaccel_* TempConfig system_estimate.xtxt *.rpt
	-$(RMDIR) src/*.ll _xocc_* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb

cleanall: clean
	-$(RMDIR) $(XCLBIN)
	-$(RMDIR) ./_x*
	-$(RMDIR) ../benchmark/cantrbry/*.lz4* ../benchmark/silesia/*.lz4*

