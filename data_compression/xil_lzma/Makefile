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
include $(COMMON_REPO)/xil_lzma/lz_compress/lz_compress.mk

#LZMA Settings
################################################################################
C_COMPUTE_UNITS:=1
PARALLEL_BLOCK:=1
SINGLE_XCLBIN:=false
################################################################################

NPB:=$(PARALLEL_BLOCK)b

# Host Executable
EXECUTABLE:=xil_lzma_$(NPB)
LIBLZMA:=liblzma.so

EMCONFIG_DIR = $(XCLBIN)/$(DSA)

# Host Application
HOST_SRCS= $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
HOST_SRCS+=./src/xil_lzma.cpp
HOST_SRCS+=./src/xlzma.cpp ./src/lzma.cpp
HOST_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS)
HOST_HDRS+=./src/xil_lzma_config.h
HOST_HDRS+=./src/xil_lzma.h
HOST_HDRS+=./src/xlzma.h ./src/lmza.h
#HOST_HDRS+=./xxhash/xxhash.h
#HOST_HDRS+=$(stream_utils_HDRS) $(lz_compress_HDRS)
#HOST_HDRS+=./src/xil_lzma_compress_engine.h ./src/xil_lzma_decompress_engine.h
CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS)
#CXXFLAGS+=-I./xxhash/ 
CXXFLAGS+= -DPARALLEL_BLOCK=$(PARALLEL_BLOCK)
CXXFLAGS+= -DC_COMPUTE_UNIT=$(C_COMPUTE_UNITS)
CXXFLAGS+= -DD_COMPUTE_UNIT=$(D_COMPUTE_UNITS)
CXXFLAGS+= -DSINGLE_XCLBIN=$(SINGLE_XCLBIN)
#CXXFLAGS+= -DOVERLAP_HOST_DEVICE
CLFLAGS= -t $(TARGET) --platform $(DEVICE)
# Enable following flag if kernel, read, write events
# information is required, by default disabled
#$(HOST_NAME)_CXXFLAGS+= -DEVENT_PROFILE
LDFLAGS=$(opencl_LDFLAGS) 
LDFLAGS+=-lrt 
LDFLAGS+=-Wno-unused-label 
LDFLAGS+=-Wno-narrowing
LDFLAGS+=-std=c++11 -g
LDFLAGS+=-DVERBOSE

XCLBIN = ./xclbin

BINARY_CONTAINERS += $(XCLBIN)/xil_lzma_compress_$(NPB).$(TARGET).$(DSA).xclbin

BINARY_CONTAINERS_xil_lzma_compress_OBJS += $(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu1.$(TARGET).$(DSA).xo
ifeq ($(strip $(C_COMPUTE_UNITS)), 4)
	BINARY_CONTAINERS_xil_lzma_compress_OBJS += $(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu2.$(TARGET).$(DSA).xo
	BINARY_CONTAINERS_xil_lzma_compress_OBJS += $(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu3.$(TARGET).$(DSA).xo
	BINARY_CONTAINERS_xil_lzma_compress_OBJS += $(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu4.$(TARGET).$(DSA).xo
endif

CP = cp -rf

.PHONY: all clean cleanall docs emconfig
all: $(LIBLZMA) $(EXECUTABLE) $(BINARY_CONTAINERS) emconfig

.PHONY: exe
exe: $(EXECUTABLE)

.PHONY: lib
lib: $(LIBLZMA)

# Kernel linker flags
C_LDCLFLAGS= --sp xil_lzma_cu1_1.m_axi_gmem0:bank0 
C_LDCLFLAGS+= --sp xil_lzma_cu1_1.m_axi_gmem1:bank0
C_LDCLFLAGS+= --sp xil_lzma_cu1_1.m_axi_gmem2:bank0
LDCLFLAGS+= --xp vivado_param:project.writeIntermediateCheckpoints=1 
LDCLFLAGS+= --kernel_frequency=250
	
ifeq ($(strip $(C_COMPUTE_UNITS)), 4)
	C_LDCLFLAGS+= --sp xil_lzma_cu2_1.m_axi_gmem0:bank2
	C_LDCLFLAGS+= --sp xil_lzma_cu2_1.m_axi_gmem1:bank2
	C_LDCLFLAGS+= --sp xil_lzma_cu2_1.m_axi_gmem2:bank2
	
	C_LDCLFLAGS+= --sp xil_lzma_cu3_1.m_axi_gmem0:bank3
	C_LDCLFLAGS+= --sp xil_lzma_cu3_1.m_axi_gmem1:bank3
	C_LDCLFLAGS+= --sp xil_lzma_cu3_1.m_axi_gmem2:bank3

	C_LDCLFLAGS+= --sp xil_lzma_cu4_1.m_axi_gmem0:bank1
	C_LDCLFLAGS+= --sp xil_lzma_cu4_1.m_axi_gmem1:bank1
	C_LDCLFLAGS+= --sp xil_lzma_cu4_1.m_axi_gmem2:bank1
endif

BUILD_DIR := ./_x.$(TARGET).$(DSA)
BUILD_DIR_compress := $(BUILD_DIR)/compress
BUILD_DIR_decompress := $(BUILD_DIR)/decompress

K_CLFLAGS=--kernel_frequency=250 --xp "param:compiler.axiDeadLockFree=false" --xp "param:compiler.enableAutoPipelining=false" -I./src/ -DPARALLEL_BLOCK=$(PARALLEL_BLOCK) 
K_CLFLAGS+=$(stream_utils_CLFLAGS) $(lz_compress_CLFLAGS)
K_CLFLAGS += --profile_kernel "data:all:all:all" -g

$(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu1.$(TARGET).$(DSA).xo: ./src/xil_lzma_compress_kernel.cpp ./src/xil_lzma_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lzma_rc_kernel.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lzma_cu1 $(K_CLFLAGS) -DC_COMPUTE_UNIT=1 -o'$@' $(CLFLAGS) -I'$(<D)' '$<'

$(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu2.$(TARGET).$(DSA).xo: ./src/xil_lzma_compress_kernel.cpp ./src/xil_lzma_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lzma_rc_kernel.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lzma_cu2 $(K_CLFLAGS) -DC_COMPUTE_UNIT=2 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 

$(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu3.$(TARGET).$(DSA).xo: ./src/xil_lzma_compress_kernel.cpp ./src/xil_lzma_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lzma_rc_kernel.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lzma_cu3 $(K_CLFLAGS) -DC_COMPUTE_UNIT=3 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 

$(XCLBIN)/xil_lzma_xo_compress_$(NPB)_cu4.$(TARGET).$(DSA).xo: ./src/xil_lzma_compress_kernel.cpp ./src/xil_lzma_config.h $(stream_utils_HDRS) $(lz_compress_HDRS) ./src/xil_lzma_rc_kernel.h
	$(XOCC) --temp_dir $(BUILD_DIR_compress) --report_dir reports/$(BUILD_DIR_compress) -c -s -k xil_lzma_cu4 $(K_CLFLAGS) -DC_COMPUTE_UNIT=4 -o'$@' $(CLFLAGS) -I'$(<D)' '$<' 
#$(XCLBIN)/xil_lzma_compress_$(NPB).$(TARGET).$(DSA).xclbin: $(BINARY_CONTAINERS_xil_lzma_compress_OBJS)
$(BINARY_CONTAINERS): $(BINARY_CONTAINERS_xil_lzma_compress_OBJS)
	mkdir -p $(XCLBIN)
	$(XOCC) --report_dir reports/$(BUILD_DIR_compress) $(CLFLAGS) -l $(LDCLFLAGS) $(C_LDCLFLAGS) --xp param:compiler.userPostSysLinkTcl=$(CURDIR)/scripts/two_compute_units/userPostSysLinkcomp.tcl -o'$@' $(+)

# Building Host
$(EXECUTABLE): $(HOST_SRCS)
	mkdir -p $(XCLBIN)
	$(CXX) $(CXXFLAGS) ./src/host.cpp $(HOST_SRCS) -o $@ $(LDFLAGS)

# liblzma.so
$(LIBLZMA): $(HOST_SRCS)
	mkdir -p $(XCLBIN)
	$(CXX) $(CXXFLAGS) -shared -fPIC $(HOST_SRCS) -o $@ $(LDFLAGS)

emconfig:$(EMCONFIG_DIR)/emconfig.json
$(EMCONFIG_DIR)/emconfig.json:
	emconfigutil --platform $(DEVICE) --od $(EMCONFIG_DIR)

check: all
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	$(CP) $(EMCONFIG_DIR)/emconfig.json .
	XCL_EMULATION_MODE=$(TARGET) ./$(EXECUTABLE) -c ../benchmark/cantrbry/alice29.txt -x 0
else
	 ./$(EXECUTABLE) -c ../benchmark/cantrbry/alice29.txt -x 0
endif

# Cleaning stuff
clean:
	-$(RMDIR) $(EXECUTABLE) $(LIBLZMA) $(XCLBIN)/{*sw_emu*,*hw_emu*} 
	-$(RMDIR) sdaccel_* TempConfig system_estimate.xtxt *.rpt
	-$(RMDIR) src/*.ll _xocc_* .Xil emconfig.json dltmp* xmltmp* *.log *.jou *.wcfg *.wdb

cleanall: clean
	-$(RMDIR) $(XCLBIN)
	-$(RMDIR) ./_x* .run/
	-$(RMDIR) ../benchmark/cantrbry/*.xz* ../benchmark/silesia/*.xz*
	-$(RMDIR) ../benchmark/*/*.xe2sd
	
