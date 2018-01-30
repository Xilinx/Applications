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

#include "xil_gzip.h"
#include <fstream>
#include <vector>
#include "cmdlineparser.h"
#include "xil_gzip_config.h"

// Display Statistics of GZip Compression
void display_stats(std::string & inFile_name, long input_size, uint32_t enbytes, int ret, bool flag) {
    
    float size_in_mb = (float)input_size/1000000;
    std::cout.precision(3);
    std::cout << "\t\t"
              << ((float)input_size/enbytes) << "\t\t"; 
    std::cout.precision(3);

    // Print messages to show case results status    
    if(flag)
        std::cout  << (ret ? "FAILED\t": "PASSED\t")<< "\t" << size_in_mb << "\t\t\t" << inFile_name << std::endl; 
    else
        std::cout  << ("DONE\t")<< "\t" << size_in_mb << "\t\t\t" << inFile_name << std::endl; 
}


// Process Single File (-i Option)
void process_file(std::string & inFile_name, xil_gzip& gzip) {
    
    std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
    if(!inFile) {
        std::cout << "Unable to open file";
        exit(1);
    }

    inFile.seekg(0, inFile.end);
    long input_size = inFile.tellg();
    inFile.seekg(0, inFile.beg);   
    inFile.close();
    
    // GZip encoding
    string gzip_encode_in  = inFile_name;
    string gzip_encode_out = inFile_name;
    gzip_encode_out =  gzip_encode_out + ".xil.gz";
    uint32_t enbytes = gzip.compress_file(gzip_encode_in, gzip_encode_out);

    display_stats(inFile_name, input_size, enbytes, 0, 0);    
}


// Process Batch of Files (-l, -b Options)
void process_batch(std::string & filelist, std::string & batch, xil_gzip & gzip) {
    
    int total_files = 0;
    std::string line; 
    std::vector<std::string> input_file_names;  
    std::vector<std::string> output_file_names;  
    std::vector<long> in_file_size; 

    std::ifstream infilelist(filelist.c_str());    

    while(std::getline(infilelist, line)) {
        total_files++;
        std::string temp = line + ".xil.gz";
        input_file_names.push_back(line);
        output_file_names.push_back(temp);

        std::ifstream inFile(line.c_str(), std::ifstream::binary);
        if(!inFile) {
            std::cout << "Unable to open file" << std::endl;
            exit(1);
        }

        inFile.seekg(0, inFile.end);
        long insize = inFile.tellg();
        in_file_size.push_back(insize);
        inFile.seekg(0, inFile.beg);
        inFile.close();
    }
    
    // Find out the size of batch
    int batch_size = 0;
    if(!batch.empty()) {
        // Figure out user provided batch count
        batch_size = atoi(batch.c_str());
    }
    else 
        batch_size = 0;

    // If -b is 0, process all the files in batch mode
    if(batch_size == 0)
        batch_size = total_files;

    std::vector<std::string> gzip_encode_in;  
    std::vector<std::string> gzip_encode_out;  
    std::vector<int> enbytes;  

    for(int i = 0; i < total_files; i+=batch_size) {
    
        int chunk_size = batch_size;
        
        if(i + batch_size > total_files)
            chunk_size = total_files - i;
        

        for(int j = 0; j < chunk_size; j++) {
            gzip_encode_in.push_back(input_file_names[i + j]);
            gzip_encode_out.push_back(output_file_names[i + j]);           
        }      
        // GZip file processing and Kernel execution on batch of files
        gzip.compress_file(gzip_encode_in, gzip_encode_out, enbytes, chunk_size);
        
        int ret = 0;

        // GZip standard decoder
        std::string gzip_cmd_encode;
 
        for(int k = 0; k < chunk_size; k++) {
            
            std::string gzip_cmp = gzip_encode_in[k];
            gzip_cmp = gzip_cmp + ".xil.gz";
            std::string gzip_dec = gzip_encode_in[k] + ".xil";
            gzip_cmd_encode = "gzip -dc " + gzip_cmp + " 1> " + gzip_dec + " 2> error";
            
            system(gzip_cmd_encode.c_str());

            // Validate results
            ret = validate(gzip_encode_in[k], gzip_dec);       

            // Delete Compressed and Decompressed Files
            if (ret == 0) {
                std::string val_del;
                val_del = "rm " + gzip_dec;
                system(val_del.c_str());
                val_del = "rm " + gzip_cmp;
                system(val_del.c_str());
            }
            else {
                std::cout << "Validation Failed" << gzip_dec.c_str() << std::endl;
                gzip.release();
                exit(1);
            }
           
            std::cout << "\t\t\t";    
            display_stats(gzip_encode_in[k], in_file_size[i + k], enbytes[i + k], ret, 1);
        }
        
       gzip_encode_in.resize(0);
       gzip_encode_out.resize(0);
    }

}

int main(int argc, char *argv[])
{
    std::string binaryFileName = "gZip_" + std::to_string(COMPUTE_UNITS) + "cu";
    
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--input_file",    "-i",     "Input Data File",        "");
    parser.addSwitch("--file_list",    "-l",      "\tList of Input Files",    "");
    parser.addSwitch("--batch",        "-b",      "\tParallel Batch Size <Default 4>",    "");
    parser.parse(argc, argv);
 
    // Create XGZip Object 
    // OpenCL Initialization     
    xil_gzip gzip;
    gzip.init(binaryFileName);
    
    std::string infile      = parser.value("input_file");   
    std::string filelist    = parser.value("file_list");   
    std::string batch       = parser.value("batch");   
    
    if (!filelist.empty()) {
        std::cout<<"\n";
        std::cout<<"Batch\t\tThroughput\t\tGZip_CR\t\tSTATUS\t\tFile Size(MB)\t\tFile Name"<<std::endl;
        std::cout<<"\n";
        process_batch(filelist,batch,gzip);
    }else if (!infile.empty()){
        std::cout<<"\n";
        std::cout<<"KT(MBps)\tGZip_CR\t\tSTATUS\t\tFile Size(MB)\t\tFile Name"<<std::endl;
        std::cout<<"\n";
        process_file(infile,gzip);   
    }  else{
        parser.printHelp();
        exit(EXIT_FAILURE);
    }

    // OpenCL Release
    gzip.release();
}
