/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "xil_lzma.h"
#include <fstream>
#include <vector>
#include "cmdlineparser.h"
#include "xil_lzma_config.h"

void xil_compress_top(std::string & compress_mod, uint32_t block_size,int cu_run) {
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
	uint64_t enbytes = xlz.compress_file(lz_compress_in, lz_compress_out,cu_run);
	
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
    int cu_run;
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--compress",    "-c",      "Compress",        "");
    parser.addSwitch("--cu",    "-x",      "CU",        "0");
    parser.parse(argc, argv);
    
    std::string compress_mod      = parser.value("compress");   
    std::string cu      = parser.value("cu");
    uint32_t bSize = 0;
    // Block Size
    bSize = BLOCK_SIZE_IN_KB;
    if(cu.empty()) {
        printf("please give -x option for cu\n");
        exit(0);
    }else {
         cu_run = atoi(cu.c_str());
    }
    printf("cu:%d\n",cu_run);
	if(cu_run >= C_COMPUTE_UNIT) {
		printf("%d CU not available\n",cu_run);
		exit(0);
	} 
    // "-c" - Compress Mode
    if (!compress_mod.empty()) 
        xil_compress_top(compress_mod, bSize,cu_run);   
}
