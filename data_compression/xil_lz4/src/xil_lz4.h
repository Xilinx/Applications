/**********
 * Copyright (c) 2017, Xilinx, Inc.
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

// Macro timers
#define TIMING

#define MAX_INPUT_SIZE 1024*1024*100 // 100MB
#define HOST_BUFFER_SIZE (64*1024*1024) // 64MB
#define BLOCK_SIZE_IN_KB 64 
#define MAX_NUMBER_BLOCKS (HOST_BUFFER_SIZE / (BLOCK_SIZE_IN_KB * 1024))

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
        uint32_t compress(uint8_t *in, uint8_t *out, uint32_t actual_size);
        uint32_t compress_file(std::string & inFile_name, std::string & outFile_name, int flow=0); 
        uint32_t decompress_file(std::string & inFile_name, std::string & outFile_name, int flow=0);
        uint32_t decompress(uint8_t *in, uint8_t *out, uint32_t actual_size, uint32_t original_size, uint32_t block_size);
        xil_lz4();
        ~xil_lz4();
    private:
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        
        // Compression related
        std::vector<uint8_t, aligned_allocator<uint8_t>> h_buf_in;
        std::vector<uint8_t, aligned_allocator<uint8_t>> h_buf_out;
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_blksize;
        std::vector<uint32_t, aligned_allocator<uint8_t>> h_compressSize;
        std::vector<uint32_t > m_blkSize;
        std::vector<uint32_t > m_compressSize;
        std::vector<bool>      m_is_compressed;        
};

