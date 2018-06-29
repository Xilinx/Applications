#GZip Settings

COMMON_REPO := ./
#Common Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk

#HOST_NAME:=xgzip_$(NCU)
HOST_NAME:=xil_gzip
XO_NAME:=gZip_cu1
XCLBIN_NAME:=gZip_app
# Host Application
$(HOST_NAME)_SRCS=./src/host.cpp $(oclHelper_SRCS) $(xcl2_SRCS) $(cmdparser_SRCS) $(logger_SRCS)
$(HOST_NAME)_SRCS+=./src/xil_gzip.cpp
$(HOST_NAME)_HDRS+=$(xcl2_HDRS) $(cmdparser_HDRS) $(logger_HDRS) 
$(HOST_NAME)_HDRS+=./src/xil_gzip_config.h 
$(HOST_NAME)_HDRS+=./src/xil_gzip.h
$(HOST_NAME)_CXXFLAGS=-I./src/ $(logger_CXXFLAGS) $(cmdparser_CXXFLAGS) $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS) $(oclHelper_CXXFLAGS) 
$(HOST_NAME)_CXXFLAGS+= -DCOMPUTE_UNITS=$(COMPUTE_UNITS) 
$(HOST_NAME)_LDFLAGS=$(opencl_LDFLAGS) 
$(HOST_NAME)_LDFLAGS+=-lrt 
$(HOST_NAME)_LDFLAGS+=-Wno-unused-label 
$(HOST_NAME)_LDFLAGS+=-std=c++0x
$(HOST_NAME)_LDFLAGS+=-DVEC_8

# Kernel
$(XO_NAME)_SRCS=./src/xil_gzip_kernel.cpp 
$(XO_NAME)_HDRS=./src/xil_gzip_config.h
K_CLFLAGS=--kernel_frequency=250 -g -DVEC_8 --xp "param:compiler.enableAutoPipelining=false" -I./src/ 
$(XO_NAME)_CLFLAGS=-k gZip_cu $(K_CLFLAGS) -DCOMPUTE_UNIT=1

XO_NAMES = $(XO_NAME)
$(XCLBIN_NAME)_LDCLFLAGS=--kernel_frequency=250 
$(XCLBIN_NAME)_LDCLFLAGS += --sp gZip_cu_1.m_axi_gmem0:bank0
$(XCLBIN_NAME)_LDCLFLAGS += --sp gZip_cu_1.m_axi_gmem1:bank1
$(XCLBIN_NAME)_LDCLFLAGS += --sp gZip_cu_1.m_axi_gmem2:bank1

XOS=$(XO_NAMES)

$(XCLBIN_NAME)_XOS=$(XOS)

EXES=$(HOST_NAME)
XCLBINS=$(XCLBIN_NAME)

# check
check_EXE=$(HOST_NAME)
check_XCLBINS=$(XCLBIN_NAME)

# To test/validate batch of files
check_ARGS= -l ./files.list -b 2 

# To compress a single file
#check_ARGS= -i benchmark/bible.txt

# To decompress a single file using standard gzip
# gzip -dc <filename>.xil.gz > <outputfile> 

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

EXTRA_CLEAN=./benchmark/*.gz ./benchmark/*/*.xil
