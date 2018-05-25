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
#include "xil_lz4.h"
#include "xxhash.h"
#define BLOCK_SIZE 64
#define KB 1024

uint32_t xil_lz4::compress_file(std::string & inFile_name, 
                                std::string & outFile_name,
                                int flow   
                               ) 
{
    if (flow == 0) { // Xilinx FPGA compression flow
        std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
        std::ofstream outFile(outFile_name.c_str(), std::ofstream::binary);
        
        if(!inFile) {
            std::cout << "Unable to open file";
            exit(1);
        }

        uint32_t input_size = get_file_size(inFile);
        
        std::vector<uint8_t,aligned_allocator<uint8_t>> in (input_size);
        std::vector<uint8_t,aligned_allocator<uint8_t>> out(input_size * 4);
        
        inFile.read((char *)in.data(),input_size); 
     
        // LZ4 header
        outFile.put(4);
        outFile.put(34);
        outFile.put(77);
        outFile.put(24);
        
        // FLG & BD bytes
        // --no-frame-crc flow
        // --content-size
        outFile.put(104);
        // 64K block size predeterminded
        // To overwrite this figure out
        // Appropriate size by running stdtesting/dump_run
        // -B4 option used for block size
        outFile.put(64);
        
        uint8_t temp_buff[10] = {104,
                                 64,
                                 input_size,
                                 input_size >> 8,
                                 input_size >> 16,
                                 input_size >> 24,
                                 0,0,0,0
                                };
        
        // xxhash is used to calculate hash value
        uint32_t xxh = XXH32(temp_buff, 10, 0);
        
        outFile.write((char*)&temp_buff[2], 8);        

        // Header CRC 
        outFile.put((uint8_t)(xxh>>8));
       
        // LZ77 encoding 
        uint32_t enbytes = compress(in.data(), out.data(), input_size);

        // Writing compressed data
        outFile.write((char *)out.data(), enbytes);
     
        outFile.put(0);  
        outFile.put(0);  
        outFile.put(0);  
        outFile.put(0);  

        // Close file 
        inFile.close();
        outFile.close();
        return enbytes;
    } else { // Standard LZ4 flow
        //std::string command = "./lz4 --no-frame-crc --content-size -B4 -f -q " + inFile_name;
        std::string command = "./lz4 --no-frame-crc --content-size -f -q " + inFile_name;
        system(command.c_str());
        std::string output = inFile_name + ".lz4";
        std::string rout = inFile_name + ".std.lz4";
        std::string rename = "mv " + output + " " + rout;
        system(rename.c_str());
        return 0;
    }
}

int validate(std::string & inFile_name, std::string & outFile_name) {

    std::string command = "cmp " + inFile_name + " " + outFile_name;
    int ret = system(command.c_str());
    return ret;
}

xil_lz4::xil_lz4(){
    h_buf_in.reserve(HOST_BUFFER_SIZE);
    h_buf_out.reserve(2 * HOST_BUFFER_SIZE);
    h_blksize.reserve(MAX_NUMBER_BLOCKS);
    h_compressSize.reserve(MAX_NUMBER_BLOCKS);
    m_compressSize.reserve(MAX_NUMBER_BLOCKS);
    m_blkSize.reserve(MAX_NUMBER_BLOCKS);
}
xil_lz4::~xil_lz4(){
}

int xil_lz4::init(const std::string& binaryFileName)
{
    // The get_xil_devices will return vector of Xilinx Devices 
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    //Creating Context and Command Queue for selected Device 
    m_context = new cl::Context(device);
    m_q = new cl::CommandQueue(*m_context, device, 
            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
    std::cout << "Found Device=" << device_name.c_str() << std::endl;

    // import_binary() command will find the OpenCL binary file created using the 
    // xocc compiler load into OpenCL Binary and return as Binaries
    // OpenCL and it can contain many functions which can be executed on the
    // device.
    std::string binaryFile = xcl::find_binary_file(device_name,binaryFileName.c_str());
    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
    devices.resize(1);
    m_program = new cl::Program(*m_context, devices, bins);
    return 0;
}

int xil_lz4::release()
{
    delete(m_q);
    delete(m_program);
    delete(m_context);
    return 0;
}

uint32_t xil_lz4::decompress_file(std::string & inFile_name, 
                                  std::string & outFile_name,
                                  int flow
                                 ) {
    if (flow == 0) {
        std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
        std::ofstream outFile(outFile_name.c_str(), std::ofstream::binary);
        
        if(!inFile) {
            std::cout << "Unable to open file";
            exit(1);
        }

        uint32_t input_size = get_file_size(inFile);
            
        std::vector<uint8_t,aligned_allocator<uint8_t>> in(input_size);

        // Read magic header 4 bytes
        char c = 0;
        char magic_hdr[] = {4, 34, 77, 24, 104};
        for (int i = 0; i < 5; i++) {
            inFile.get(c);
            if (c == magic_hdr[i])
                continue;
            else {
                std::cout << "Problem with magic header " << c << " " << i << std::endl;
                exit(1);
            }
        }
        
        // Check if block size is 64 KB
        inFile.get(c);

        uint32_t block_size = 0;        

        switch(c) {

            case 64:block_size = 64;
                    break;
            case 80:block_size = 256;
                    break;
            case 96:block_size = 1024;
                    break;
            case 112:block_size = 4096;
                    break;
            default:
                    std::cout << "Invalid Block Size" << std::endl;
                    break;
        }

        // Original size
        uint32_t original_size=0; 
        inFile.read((char *)&original_size,8); 
        inFile.get(c);

        // Allocat output size
        std::vector<uint8_t,aligned_allocator<uint8_t>> out(original_size);
        // Read block data from compressed stream .lz4
        inFile.read((char *)in.data(), (input_size - 15));
        uint32_t debytes = decompress(in.data(), out.data(), (input_size - 15), original_size, block_size);
        outFile.write((char *)out.data(), debytes);
        // Close file 
        inFile.close();
        outFile.close();
        return debytes;
    } else {
        std::string command = "./lz4 --no-frame-crc --content-size -f -q -d " + inFile_name;
        system(command.c_str());
        return 0;
    }
}

uint32_t xil_lz4::decompress(uint8_t *in,
                            uint8_t *out,
                            uint32_t input_size,
                            uint32_t original_size,
                            uint32_t block_size_in_kb
                           ) {
    std::string kernel_names = "xil_lz4_dec_cu1";
    cl::Kernel* kernel_lz4  = new  cl::Kernel(*m_program,kernel_names.c_str());
    uint32_t block_size_in_bytes = block_size_in_kb * 1024;

    // Total number of blocks exists for this file
    int total_block_cnt = (original_size - 1) / block_size_in_bytes + 1;
    int block_cntr = 0;
    int done_block_cntr = 0;

    uint32_t no_compress_case=0;
    std::chrono::duration<double, std::nano> kernel_time_ns_1(0);
    uint32_t inIdx = 0;
    uint32_t total_decomression_size=0;

    for (uint32_t outIdx = 0 ; outIdx < original_size ; outIdx +=HOST_BUFFER_SIZE ){
        uint32_t chunk_size  = HOST_BUFFER_SIZE;
        if (chunk_size + outIdx> original_size){
            chunk_size = original_size-outIdx;
        }
        uint32_t nblocks= 0;
        uint32_t bufblocks= 0;
        uint32_t total_size = 0;
        uint32_t buf_size = 0;
        uint32_t block_size = 0;
        uint32_t compressed_size = 0;
        for (uint32_t cIdx = 0; cIdx < chunk_size ; cIdx +=block_size_in_bytes,nblocks++,total_size +=block_size)
        {
            //memcpy(&block_size,&in[inIdx],4); inIdx +=4;
            if (block_cntr == (total_block_cnt - 1)) {
                block_size = original_size - done_block_cntr * block_size_in_bytes;
            } else {
                block_size = block_size_in_bytes;
            }

            memcpy(&compressed_size,&in[inIdx],4); inIdx +=4;

            uint32_t tmp = compressed_size;
            tmp >>= 24;

            if (tmp == 128) {
                uint8_t b1 = compressed_size;
                uint8_t b2 = compressed_size >> 8;
                uint8_t b3 = compressed_size >> 16;
                //uint8_t b4 = compressed_size >> 24;
            
                if (b3 == 1) {
                    compressed_size = block_size_in_bytes;
                } else {
                    uint16_t size = 0;
                    size = b2;
                    size <<= 8;
                    uint16_t temp = b1;
                    size |= temp;
                    compressed_size = size;       
                }
            }
    
            m_blkSize[nblocks] = block_size; 
            m_compressSize[nblocks] = compressed_size; 
            if (compressed_size < block_size){
                h_compressSize[bufblocks]  = compressed_size;
                h_blksize[bufblocks] = block_size;
                std::memcpy(&(h_buf_in.data()[buf_size]),&in[inIdx],compressed_size);
                inIdx +=compressed_size;
                buf_size +=block_size_in_bytes;
                bufblocks++;
            }else if (compressed_size == block_size){
                no_compress_case++;
                //No compression block
                std::memcpy(&(out[outIdx + cIdx]),&in[inIdx],block_size);
                inIdx +=block_size;
            }else{
                assert(0);
            }
            block_cntr++;
            done_block_cntr++;
        }
        
        assert(total_size<=original_size);
        // Update input size based on 
        
        if (nblocks == 1 && compressed_size == block_size)
            break;    
    
        // Device buffers
        cl::Buffer* buffer_input;
        cl::Buffer* buffer_output;
        cl::Buffer* buffer_block_size;
        cl::Buffer* buffer_compressed_size;
    
        // Device buffer allocation
        buffer_input     = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,  
                buf_size, h_buf_in.data());

        buffer_output    = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 
                buf_size,h_buf_out.data());

        buffer_block_size      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, 
                sizeof(uint32_t) * bufblocks , h_blksize.data());
        
        buffer_compressed_size      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, 
                sizeof(uint32_t) * bufblocks, h_compressSize.data());
        
        // Set kernel arguments
        uint32_t narg = 0;
        kernel_lz4->setArg(narg++, *(buffer_input));
        kernel_lz4->setArg(narg++, *(buffer_output));
        kernel_lz4->setArg(narg++, *(buffer_block_size));
        kernel_lz4->setArg(narg++, *(buffer_compressed_size));
        kernel_lz4->setArg(narg++, block_size_in_kb);
        kernel_lz4->setArg(narg++, bufblocks);

        std::vector<cl::Memory> inBufVec;
        inBufVec.push_back(*(buffer_input));
        inBufVec.push_back(*(buffer_block_size));
        inBufVec.push_back(*(buffer_compressed_size));
    
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
        m_q->finish();
        auto kernel_start = std::chrono::high_resolution_clock::now(); 
        // Kernel invocation
        m_q->enqueueTask(*kernel_lz4);
        m_q->finish();
        auto kernel_end = std::chrono::high_resolution_clock::now();    
        auto duration   = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
        kernel_time_ns_1 += duration;
        
        std::vector<cl::Memory> outBufVec;
        outBufVec.push_back(*(buffer_output));
        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
        m_q->finish();
        uint32_t bufIdx=0;
        for (uint32_t bIdx = 0, idx=0 ; bIdx < nblocks; bIdx++, idx +=block_size_in_bytes){
            uint32_t block_size         = m_blkSize[bIdx];
            uint32_t compressed_size    = m_compressSize[bIdx];
            if (compressed_size < block_size){
                std::memcpy(&out[outIdx+idx],&h_buf_out[bufIdx],block_size);
                bufIdx +=block_size_in_bytes;
                total_decomression_size += block_size;
            }
        }
        
        delete (buffer_input);
        delete (buffer_output);
        delete (buffer_block_size);
        delete (buffer_compressed_size);
    }
    delete(kernel_lz4);
    float throughput_in_mbps_1 = (float)total_decomression_size* 1000 / kernel_time_ns_1.count();
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1;
    return original_size;

} // End of decompress

uint32_t xil_lz4::compress(uint8_t *in, 
                     uint8_t *out, 
                     uint32_t input_size
                    ) {
    std::string kernel_names = "xil_lz4_cu1";
    cl::Kernel* kernel_lz4  = new  cl::Kernel(*m_program,kernel_names.c_str());
    uint32_t block_size_in_kb = BLOCK_SIZE_IN_KB;
    uint32_t block_size_in_bytes = block_size_in_kb * 1024;

    // Device buffers
    cl::Buffer* buffer_input;
    cl::Buffer* buffer_output;
    cl::Buffer* buffer_compressed_size;
    cl::Buffer* buffer_block_size;
    uint32_t no_compress_case=0;
   
    std::chrono::duration<double, std::nano> kernel_time_ns_1(0);
    uint32_t outIdx = 0;
    for (uint32_t inIdx = 0 ; inIdx < input_size ; inIdx+= HOST_BUFFER_SIZE){
        uint32_t buf_size  = HOST_BUFFER_SIZE;
        if (buf_size + inIdx > input_size){
            buf_size = input_size-inIdx;
        }
        uint32_t nblocks = (buf_size- 1) / block_size_in_bytes + 1;
        std::memcpy(h_buf_in.data(),&in[inIdx],buf_size);
       
        uint32_t bIdx = 0; 
        for (uint32_t i = 0; i < buf_size; i+=block_size_in_bytes) {
            uint32_t block_size = block_size_in_bytes;
            if (i+block_size > buf_size){
                block_size = buf_size - i;
            }
            h_blksize[bIdx++] = block_size;
        }

        uint32_t buffer_size_in_bytes = ((buf_size-1)/64 + 1) * 64;
        // Device buffer allocation
        buffer_input     = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,  buffer_size_in_bytes, h_buf_in.data());

        buffer_output    = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, 2 * buffer_size_in_bytes, h_buf_out.data());

        buffer_compressed_size      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, sizeof(uint32_t) * nblocks, 
                h_compressSize.data());

        buffer_block_size      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, sizeof(uint32_t) * nblocks, 
                h_blksize.data());
        
        // Set kernel arguments
        int narg = 0;
        kernel_lz4->setArg(narg++, *(buffer_input));
        kernel_lz4->setArg(narg++, *(buffer_output));
        kernel_lz4->setArg(narg++, *(buffer_compressed_size));
        kernel_lz4->setArg(narg++, *(buffer_block_size));
        kernel_lz4->setArg(narg++, block_size_in_kb);
        kernel_lz4->setArg(narg++, buf_size);

        std::vector<cl::Memory> inBufVec;
        inBufVec.push_back(*(buffer_input));
        inBufVec.push_back(*(buffer_block_size));
        
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
        m_q->finish();
        auto kernel_start = std::chrono::high_resolution_clock::now(); 
        m_q->enqueueTask(*kernel_lz4);
        m_q->finish();
        auto kernel_end = std::chrono::high_resolution_clock::now();    
        auto duration   = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
        kernel_time_ns_1 += duration;
        std::vector<cl::Memory> outBufVec;
        outBufVec.push_back(*(buffer_output));
        outBufVec.push_back(*(buffer_compressed_size));

        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
        m_q->finish();

        // Copy data into out buffer 
        // Include compress and block size data
        uint32_t idx=0;
        for(uint32_t bIdx = 0; bIdx < nblocks; bIdx++, idx += block_size_in_bytes) {
            uint32_t block_size = block_size_in_bytes;
            if (idx+block_size > buf_size){
                block_size = buf_size -idx;
            }
            uint32_t compressed_size = h_compressSize.data()[bIdx];
            assert(compressed_size != 0);
            //assert(block_size > 2*VEC);
            //memcpy(&out[outIdx],&block_size,4); outIdx +=4;
            int orig_block_size = buf_size;
            int perc_cal = orig_block_size * 10;
            perc_cal = perc_cal / block_size;
            
            if (compressed_size < block_size && perc_cal >= 10){ 
                memcpy(&out[outIdx],&compressed_size,4); outIdx +=4;
                std::memcpy(&out[outIdx], &(h_buf_out.data()[2 * bIdx * block_size_in_bytes]), compressed_size); 
                outIdx += compressed_size;
            }else{
                // No Compression, so copy raw data
                no_compress_case++;
                if (block_size == 65536) {
                    out[outIdx++] = 0;
                    out[outIdx++] = 0;
                    out[outIdx++] = 1;
                    out[outIdx++] = 128;                
                } else {
                    uint8_t temp = 0;
                    temp = block_size;
                    out[outIdx++] = temp;       
                    temp = block_size >> 8;
                    out[outIdx++] = temp;
                    out[outIdx++] = 0;       
                    out[outIdx++] = 128;    
                }
                //memcpy(&out[outIdx],&block_size,4); outIdx +=4;
                std::memcpy(&out[outIdx], &in[inIdx + idx], block_size);          
                outIdx += block_size;
            }
        }
        delete (buffer_input);
        delete (buffer_output);
        delete (buffer_compressed_size);
        delete (buffer_block_size);
    }
    delete(kernel_lz4);
    float throughput_in_mbps_1 = (float)input_size * 1000 / kernel_time_ns_1.count();
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1;
    return outIdx;
}

