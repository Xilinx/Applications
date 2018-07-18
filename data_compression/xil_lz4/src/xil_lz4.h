/**********
 * Copyright (c) 2018, Xilinx, Inc.
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
#define MAX_COMPUTE_UNITS 2

// Maximum host buffer used to operate
// per kernel invocation
#define HOST_BUFFER_SIZE (2*1024*1024)

// Default block size
#define BLOCK_SIZE_IN_KB 64 

// Value below is used to associate with
// Overlapped buffers, ideally overlapped 
// execution requires 2 resources per invocation
#define OVERLAP_BUF_COUNT 2 

// Maximum number of blocks based on host buffer size
#define MAX_NUMBER_BLOCKS (HOST_BUFFER_SIZE / (BLOCK_SIZE_IN_KB * 1024))

// Below are the codes as per LZ4 standard for 
// various maximum block sizes supported.
#define BSIZE_STD_64KB 64
#define BSIZE_STD_256KB 80
#define BSIZE_STD_1024KB 96
#define BSIZE_STD_4096KB 112

// Maximum block sizes supported by LZ4
#define MAX_BSIZE_64KB 65536
#define MAX_BSIZE_256KB 262144
#define MAX_BSIZE_1024KB 1048576
#define MAX_BSIZE_4096KB 4194304

// This value is used to set 
// uncompressed block size value
// 4th byte is always set to below 
// and placed as uncompressed byte
#define NO_COMPRESS_BIT 128

// In case of uncompressed block 
// Values below are used to set
// 3rd byte to following values
// w.r.t various maximum block sizes 
// supported by standard
#define BSIZE_NCOMP_64 1
#define BSIZE_NCOMP_256 4
#define BSIZE_NCOMP_1024 16
#define BSIZE_NCOMP_4096 64

int validate(std::string & inFile_name, std::string & outFile_name);

static uint32_t get_file_size(std::ifstream &file){
    file.seekg(0,file.end);
    uint32_t file_size = file.tellg();
    file.seekg(0,file.beg);
    return file_size;
}

class xil_lz4 {
    public:
        int init(const std::string& binaryFile);
        int release();
        uint32_t compress_sequential(uint8_t *in, uint8_t *out, uint32_t actual_size);
        uint32_t compress(uint8_t *in, uint8_t *out, uint32_t actual_size, uint32_t host_buffer_size);
        uint32_t compress_file(std::string & inFile_name, std::string & outFile_name); 
        uint32_t decompress_file(std::string & inFile_name, std::string & outFile_name);
        uint32_t decompress_sequential(uint8_t *in, uint8_t *out, uint32_t actual_size, uint32_t original_size);
        uint32_t decompress(uint8_t *in, uint8_t *out, uint32_t actual_size, uint32_t original_size, uint32_t host_buffer_size);
        uint64_t get_event_duration_ns(const cl::Event &event);
        void buffer_extension_assignments(bool flow);
        // Binary flow compress/decompress        
        bool m_bin_flow;        
        
        // Block Size
        uint32_t m_block_size_in_kb;       

        // Switch between FPGA/Standard flows
        bool m_switch_flow;
 
        xil_lz4();
        ~xil_lz4();
    private:
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        cl::Kernel* compress_kernel_lz4[C_COMPUTE_UNIT];
        cl::Kernel* decompress_kernel_lz4[D_COMPUTE_UNIT];

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
        
        // Kernel names 
        std::vector<std::string> compress_kernel_names = {"xil_lz4_cu1",
                                                         "xil_lz4_cu2",                                               
                                                         "xil_lz4_cu3",                                               
                                                         "xil_lz4_cu4",                                               
                                                         "xil_lz4_cu5",                                               
                                                         "xil_lz4_cu6",                                               
                                                         "xil_lz4_cu7",                                               
                                                         "xil_lz4_cu8",                                               
                                                         };
        
        std::vector<std::string> decompress_kernel_names = {"xil_lz4_dec_cu1",
                                                            "xil_lz4_dec_cu2",                                               
                                                            "xil_lz4_dec_cu3",                                               
                                                            "xil_lz4_dec_cu4",                                               
                                                            "xil_lz4_dec_cu5",                                               
                                                            "xil_lz4_dec_cu6",                                               
                                                            "xil_lz4_dec_cu7",                                               
                                                            "xil_lz4_dec_cu8",                                               
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
        std::vector<uint32_t> decomp_ddr_nums = {XCL_MEM_DDR_BANK1,
                                          XCL_MEM_DDR_BANK2,
                                          XCL_MEM_DDR_BANK2,
                                          XCL_MEM_DDR_BANK3,
                                          XCL_MEM_DDR_BANK0,
                                          XCL_MEM_DDR_BANK1,
                                          XCL_MEM_DDR_BANK2,
                                          XCL_MEM_DDR_BANK3,
                                        };

};

