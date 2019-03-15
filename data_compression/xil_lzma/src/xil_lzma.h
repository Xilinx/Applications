/**********
 * Copyright (c) 2019, Xilinx, Inc.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * **********/
#pragma once
#include "defns.h"

// Maximum compute units supported
#define MAX_COMPUTE_UNITS 1

// Maximum host buffer used to operate
// per kernel invocation
//#define HOST_BUFFER_SIZE 64*1024 //(1024*1024)
#define HOST_BUFFER_SIZE (2*1024*1024)

// Default block size
#define BLOCK_SIZE_IN_KB 64 

// Value below is used to associate with
// Overlapped buffers, ideally overlapped 
// execution requires 2 resources per invocation
#define OVERLAP_BUF_COUNT 2 

// Maximum number of blocks based on host buffer size
#define MAX_NUMBER_BLOCKS (HOST_BUFFER_SIZE / (BLOCK_SIZE_IN_KB * 1024))

// Below are the codes as per LZMA standard for 
// various maximum block sizes supported.
#define BSIZE_STD_64KB 64
#define BSIZE_STD_256KB 80
#define BSIZE_STD_1024KB 96
#define BSIZE_STD_4096KB 112

// Maximum block sizes supported by LZMA
#define MAX_BSIZE_64KB 65536
#define MAX_BSIZE_256KB 262144
#define MAX_BSIZE_1024KB 1048576
#define MAX_BSIZE_4096KB 4194304

#define MEM_ALLOC_CPU (16*4*16*1024*1024)

int validate(std::string & inFile_name, std::string & outFile_name);

/*
static uint32_t get_file_size(std::ifstream &file){
    file.seekg(0,file.end);
    uint32_t file_size = file.tellg();
    file.seekg(0,file.beg);
    return file_size;
}
*/

static uint64_t get_bigfile_size(std::ifstream &file){
    file.seekg(0,file.end);
    uint64_t file_size = file.tellg();
    file.seekg(0,file.beg);
    return file_size;
}

typedef struct {
	size_t uncompressed_size;
	size_t h_compressed_size;
}RECORD_LIST;

class xil_lzma {
    public:
        int init(const std::string& binaryFile);
        int release();
        //uint32_t compress_sequential(uint8_t *in, uint8_t *out, uint32_t actual_size);
        uint32_t compress(uint8_t *in, uint8_t *out, uint32_t actual_size,
		                  uint32_t host_buffer_size,std::ofstream &ofs,void *Gdict1,
		                  void *Gdict2,void *Gdict3,void *Gdict4);
	    uint64_t compress_file(std::string & inFile_name, std::string & outFile_name);
        uint64_t get_event_duration_ns(const cl::Event &event);
        void buffer_extension_assignments(bool flow);
        // Binary flow compress/decompress        
        bool m_bin_flow;        
        
        // Block Size
        uint32_t m_block_size_in_kb;       

        // Switch between FPGA/Standard flows
        bool m_switch_flow;
 
        xil_lzma();
        ~xil_lzma();
    private:
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        cl::Kernel* compress_kernel_lzma[C_COMPUTE_UNIT];

        // Compression related
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_buf_in[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        std::vector<uint8_t, aligned_allocator<uint8_t>>  h_buf_out[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_blksize[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_compressSize[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        
        // Device buffers
        cl::Buffer* buffer_input[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl::Buffer* buffer_output[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl::Buffer* buffer_compressed_size[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl::Buffer* buffer_block_size[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        
        // Decompression related
        std::vector<uint32_t> m_blkSize[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        std::vector<uint32_t> m_compressSize[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        std::vector<bool>     m_is_compressed[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];        
        
        // DDR buffer extensions
        cl_mem_ext_ptr_t inExt[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl_mem_ext_ptr_t outExt[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl_mem_ext_ptr_t csExt[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl_mem_ext_ptr_t bsExt[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        
        // Read, Write and Kernel events
        cl::Event kernel_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl::Event read_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        cl::Event write_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
        
        // Kernel names 
        std::vector<std::string> compress_kernel_names = {"xil_lzma_cu1",
                                                         "xil_lzma_cu2",                                               
                                                         "xil_lzma_cu3",                                               
                                                         "xil_lzma_cu4",                                               
                                                         "xil_lzma_cu5",                                               
                                                         "xil_lzma_cu6",                                               
                                                         "xil_lzma_cu7",                                               
                                                         "xil_lzma_cu8",                                               
                                                         };
        
        // DDR numbers
        std::vector<uint32_t> comp_ddr_nums = {XCL_MEM_DDR_BANK0,
                                          XCL_MEM_DDR_BANK3,
                                          XCL_MEM_DDR_BANK2,
                                          XCL_MEM_DDR_BANK3,
                                          XCL_MEM_DDR_BANK0,
                                          XCL_MEM_DDR_BANK1,
                                          XCL_MEM_DDR_BANK2,
                                          XCL_MEM_DDR_BANK3,
                                        };

};

