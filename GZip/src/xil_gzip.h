/**********
Copyright (c) 2017, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
#pragma once
#include "defns.h"

// Fixed Maximum Input Size
#define MAX_INPUT_SIZE 1024*1024*100 // 100MB


int validate(std::string & inFile_name, std::string & outFile_name);

class xil_gzip {
    public:
        // Device Configuration Process
        int init(const std::string& binaryFile);
        
        // Release OpenCL Parameters
        int release();

        // Input: Input File Buffer       <Single File Mode>
        //        Actual Input Size
        // Output: Output File Buffer
        uint32_t compress(uint8_t *in, uint8_t *out, long actual_size);

        // Input: Input File Name
        // Output: Compressed Stream Size
        uint32_t compress_file(std::string & inFile_name, std::string & outFile_name); 

        // Input:  Input File Names       <Batch File Mode>
        // Output: Output File Names 
        void compress_file(std::vector<std::string> & inFile_batch, 
                           std::vector<std::string> & outFile_batch,
                           std::vector<int> & enbytes,
                           int chunk_size
                          );

        // Input: Buffer pointers to Batch of Files
        //        Input File Sizes
        // Output: GZip Compressed Stream of Each File
        //         GZip Output Buffer Pointers
        //         Compressed Stream Size
        void compress(uint8_t *gzip_in[], 
                      uint8_t *gzip_out[],
                      std::vector<int> & input_size_cntr, 
                      uint32_t *file_encode_bytes
                     );

    private:
        
        // OpenCL Device Configuration Parameters
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        cl::Kernel* kernel_gzip;
        
        // Batch Device Buffers <Batch Mode>
        cl::Buffer* batch_buffer_input[2];
        cl::Buffer* batch_buffer_output[2];
        cl::Buffer* batch_buffer_size[2];
        
        // Host Output Buffers <Single File Mode>
        std::vector<uint8_t,aligned_allocator<uint8_t>> *local_out;
        std::vector<uint32_t,aligned_allocator<uint32_t>> *sizeOut;
        
        // Batch of Host Output Buffers  <Batch Mode>
        std::vector<uint8_t,aligned_allocator<uint8_t>> *batch_local_out[2];
        std::vector<uint32_t,aligned_allocator<uint32_t>> *batch_sizeOut[2];
        
        // Assigns Device Buffers with DDR
        cl_mem_ext_ptr_t get_buffer_extension(int ddr_no);
};

