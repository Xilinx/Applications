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
#include "xil_lzma.h"
#include <fstream>
#include <vector>
#include "cmdlineparser.h"
#include "xil_lzma_config.h"

void xil_compress_top(std::string & compress_mod, uint32_t block_size) {
    // Xilinx LZMA object 
    xil_lzma xlz;

    // LZMA Compression Binary Name
    std::string binaryFileName;
    if(SINGLE_XCLBIN) binaryFileName = "xil_lzma_compress_decompress_" + std::to_string(PARALLEL_BLOCK) + "b";
    else binaryFileName = "xil_lzma_compress_" + std::to_string(PARALLEL_BLOCK) + "b";
    xlz.m_bin_flow = 1;
    // Create xil_lzma object
    xlz.init(binaryFileName);
#ifdef VERBOSE        
    std::cout<<"\n";
    std::cout<<"E2E(MBps)\tKT(MBps)\tLZMA_CR\t\tFile Size(MB)\t\tFile Name"<<std::endl;
    std::cout<<"\n";
#endif 
        
    std::ifstream inFile(compress_mod.c_str(), std::ifstream::binary);
    if(!inFile) {
        std::cout << "Unable to open file";
        exit(1);
    }
    uint64_t input_size = get_bigfile_size(inFile);
    
    std::string lz_compress_in  = compress_mod;
    std::string lz_compress_out = compress_mod;
    lz_compress_out =  lz_compress_out + ".xz";

    // Update class membery with block_size    
    xlz.m_block_size_in_kb = block_size;    
    
    // 0 means Xilinx flow
    xlz.m_switch_flow = 0;

    // Call LZMA compression
	uint64_t enbytes = xlz.compress_file(lz_compress_in, lz_compress_out);
	
    //std::cout<<__func__<<":"<<__LINE__<<"\n";
#ifdef VERBOSE 
    std::cout.precision(3);
    std::cout   << "\t\t" << (float) input_size/enbytes 
                << "\t\t" << (float) input_size/1000000 
                << "\t\t\t" << lz_compress_in << std::endl;
    std::cout << "\n"; 
    std::cout << lz_compress_out.c_str() << std::endl;        
#endif
    xlz.release();
    //std::cout<<__func__<<":"<<__LINE__<<"\n";
}

int main(int argc, char *argv[])
{
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--compress",    "-c",      "Compress",        "");
    parser.parse(argc, argv);
    
    std::string compress_mod      = parser.value("compress");   

    uint32_t bSize = 0;
    // Block Size
    bSize = BLOCK_SIZE_IN_KB;

    // "-c" - Compress Mode
    if (!compress_mod.empty()) 
        xil_compress_top(compress_mod, bSize);   
}
