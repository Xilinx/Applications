#GZip Settings
################################################################################
#Select the number of Compute units. 
COMPUTE_UNITS:=2
################################################################################


COMMON_REPO := .
#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk

NCU:=$(COMPUTE_UNITS)cu
#HOST_NAME:=xgzip_$(NCU)
HOST_NAME:=xgzip
XO1_NAME:=gZip_cu1
XO2_NAME:=gZip_cu2
XO3_NAME:=gZip_cu3
XO4_NAME:=gZip_cu4
XCLBIN_NAME:=gZip_$(NCU)
# Host Application
$(HOST_NAME)_SRCS=./src/host.cpp $(oclHelper_SRCS) $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
$(HOST_NAME)_SRCS+=./src/lz77_cpu_decode.cpp 
$(HOST_NAME)_SRCS+=./src/lz77_fpga_encode.cpp
$(HOST_NAME)_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS)
$(HOST_NAME)_CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS) $(oclHelper_CXXFLAGS) 
$(HOST_NAME)_CXXFLAGS+= -DCOMPUTE_UNITS=$(COMPUTE_UNITS) 
$(HOST_NAME)_LDFLAGS=$(opencl_LDFLAGS) 
$(HOST_NAME)_LDFLAGS+=-lrt 
$(HOST_NAME)_LDFLAGS+=-Wno-unused-label 
$(HOST_NAME)_LDFLAGS+=-std=c++0x
$(HOST_NAME)_LDFLAGS+=-DVEC_8

# Kernel
$(XO1_NAME)_SRCS=./src/gZip_kernel.cpp 
$(XO2_NAME)_SRCS=./src/gZip_kernel.cpp 
$(XO3_NAME)_SRCS=./src/gZip_kernel.cpp 
$(XO4_NAME)_SRCS=./src/gZip_kernel.cpp 
K_CLFLAGS=--kernel_frequency=250 -g -DVEC_8 --xp "param:compiler.enableAutoPipelining=false" -I./src/ 
$(XO1_NAME)_CLFLAGS=-k gZip_cu1 $(K_CLFLAGS) -DCOMPUTE_UNIT=1
$(XO2_NAME)_CLFLAGS=-k gZip_cu2 $(K_CLFLAGS) -DCOMPUTE_UNIT=2
$(XO3_NAME)_CLFLAGS=-k gZip_cu3 $(K_CLFLAGS) -DCOMPUTE_UNIT=3
$(XO4_NAME)_CLFLAGS=-k gZip_cu4 $(K_CLFLAGS) -DCOMPUTE_UNIT=4

XO_NAMES = $(XO1_NAME)
$(XCLBIN_NAME)_LDCLFLAGS=--kernel_frequency=250 
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu1_1.M_AXI_GMEM0.core.OCL_REGION_0.M00_AXI
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu1_1.M_AXI_GMEM1.core.OCL_REGION_0.M00_AXI
ifneq ($(COMPUTE_UNITS),1)
XO_NAMES += $(XO2_NAME)
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu2_1.M_AXI_GMEM0.core.OCL_REGION_0.M01_AXI
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu2_1.M_AXI_GMEM1.core.OCL_REGION_0.M01_AXI
ifneq ($(COMPUTE_UNITS),2)
XO_NAMES += $(XO3_NAME)
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu3_1.M_AXI_GMEM0.core.OCL_REGION_0.M02_AXI
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu3_1.M_AXI_GMEM1.core.OCL_REGION_0.M02_AXI
ifneq ($(COMPUTE_UNITS),3)
XO_NAMES += $(XO4_NAME)
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu4_1.M_AXI_GMEM0.core.OCL_REGION_0.M03_AXI
$(XCLBIN_NAME)_LDCLFLAGS += --xp misc:map_connect=add.kernel.gZip_cu4_1.M_AXI_GMEM1.core.OCL_REGION_0.M03_AXI
endif
endif
endif

XOS=$(XO_NAMES)

$(XCLBIN_NAME)_XOS=$(XOS)

EXES=$(HOST_NAME)
XCLBINS=$(XCLBIN_NAME)

# check
check_EXE=$(HOST_NAME)
check_XCLBINS=$(XCLBIN_NAME)
check_ARGS= -l ./cantrbry.list
#check_ARGS= -l ./silesia.list
#check_ARGS= -i ./benchmark/cantrbry/alice29.txt
CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

EXTRA_CLEAN=benchmark/*.huffman ./benchmark/*/*.huffman ./benchmark/*.orig ./benchmark/*/*.orig ./benchmark/*.lz77 ./benchmark/*/*.lz77 ./benchmark/*/*.xgzip
