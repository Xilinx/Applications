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
#include "xil_lz4.h"
#include "xxhash.h"
#define BLOCK_SIZE 64
#define KB 1024
#define MAGIC_HEADER_SIZE 4
#define MAGIC_BYTE_1 4
#define MAGIC_BYTE_2 34
#define MAGIC_BYTE_3 77
#define MAGIC_BYTE_4 24
#define FLG_BYTE 104

uint64_t xil_lz4::get_event_duration_ns(const cl::Event &event){

    uint64_t start_time=0, end_time=0;

    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START, &start_time);
    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END, &end_time);
    return (end_time - start_time);
}

uint32_t xil_lz4::compress_file(std::string & inFile_name, 
                                std::string & outFile_name
                               ) 
{
    if (m_switch_flow == 0) { // Xilinx FPGA compression flow
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
        outFile.put(MAGIC_BYTE_1);
        outFile.put(MAGIC_BYTE_2);
        outFile.put(MAGIC_BYTE_3);
        outFile.put(MAGIC_BYTE_4);
        
        // FLG & BD bytes
        // --no-frame-crc flow
        // --content-size
        outFile.put(FLG_BYTE);
       
        // Default value 64K
        uint8_t block_size_header = 0;
        switch(m_block_size_in_kb) {

            case 64:outFile.put(BSIZE_STD_64KB);
                    block_size_header = BSIZE_STD_64KB;
                    break;
            case 256:outFile.put(BSIZE_STD_256KB);
                     block_size_header = BSIZE_STD_256KB;
                     break;
            case 1024:outFile.put(BSIZE_STD_1024KB);
                      block_size_header = BSIZE_STD_1024KB;
                      break;
            case 4096:outFile.put(BSIZE_STD_4096KB);
                      block_size_header = BSIZE_STD_4096KB;
                      break;
            default:
                    std::cout << "Invalid Block Size" << std::endl;
                    break;
        }

        uint32_t host_buffer_size = HOST_BUFFER_SIZE;
        uint32_t acc_buff_size = m_block_size_in_kb * 1024 * PARALLEL_BLOCK;
        if (acc_buff_size > host_buffer_size){
            host_buffer_size = acc_buff_size;
        }
        if (host_buffer_size > input_size){
            host_buffer_size = input_size;
        }
        uint8_t temp_buff[10] = {FLG_BYTE,
                                 block_size_header,
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
        
        // LZ4 overlap & multiple compute unit compress 
        uint32_t enbytes = compress(in.data(), out.data(), input_size, host_buffer_size);
   
        // LZ4 multiple/single cu sequential version
        //uint32_t enbytes = compress_sequential(in.data(), out.data(), input_size);
    
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
        std::string command = "./lz4 --content-size -f -q " + inFile_name;
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

void xil_lz4::buffer_extension_assignments(bool flow){
    for (int i = 0; i < MAX_COMPUTE_UNITS; i++) {
        for (int j = 0; j < OVERLAP_BUF_COUNT; j++){
            if(flow){
                inExt[i][j].flags = comp_ddr_nums[i];
                inExt[i][j].obj   = h_buf_in[i][j].data();
                
                outExt[i][j].flags = comp_ddr_nums[i];
                outExt[i][j].obj   = h_buf_out[i][j].data();
                
                csExt[i][j].flags = comp_ddr_nums[i];
                csExt[i][j].obj   = h_compressSize[i][j].data();
                
                bsExt[i][j].flags = comp_ddr_nums[i];
                bsExt[i][j].obj   = h_blksize[i][j].data();
            }
            else{
                inExt[i][j].flags = decomp_ddr_nums[i];
                inExt[i][j].obj   = h_buf_in[i][j].data();
                
                outExt[i][j].flags = decomp_ddr_nums[i];
                outExt[i][j].obj   = h_buf_out[i][j].data();
                
                csExt[i][j].flags = decomp_ddr_nums[i];
                csExt[i][j].obj   = h_compressSize[i][j].data();
                
                bsExt[i][j].flags = decomp_ddr_nums[i];
                bsExt[i][j].obj   = h_blksize[i][j].data();
            }
        }
    }
}

// Constructor
xil_lz4::xil_lz4(){

    for (int i = 0; i < MAX_COMPUTE_UNITS; i++) {
        for (int j = 0; j < OVERLAP_BUF_COUNT; j++){
            // Index calculation
            h_buf_in[i][j].resize(HOST_BUFFER_SIZE);
            h_buf_out[i][j].resize(HOST_BUFFER_SIZE);
            h_blksize[i][j].resize(MAX_NUMBER_BLOCKS);
            h_compressSize[i][j].resize(MAX_NUMBER_BLOCKS);    
            
            m_compressSize[i][j].reserve(MAX_NUMBER_BLOCKS);
            m_blkSize[i][j].reserve(MAX_NUMBER_BLOCKS);
        }
    }
}   

// Destructor
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
   
    if (SINGLE_XCLBIN){
        // Create Compress kernels
        for (int i = 0; i < C_COMPUTE_UNIT; i++) 
            compress_kernel_lz4[i] = new cl::Kernel(*m_program, compress_kernel_names[i].c_str());

        // Create Decompress kernels
        for (int i = 0; i < D_COMPUTE_UNIT; i++)
            decompress_kernel_lz4[i] = new cl::Kernel(*m_program, decompress_kernel_names[i].c_str());
    } 
    else {
        if (m_bin_flow) {
            // Create Compress kernels
            for (int i = 0; i < C_COMPUTE_UNIT; i++) 
                compress_kernel_lz4[i] = new cl::Kernel(*m_program, compress_kernel_names[i].c_str());
        } else {
            // Create Decompress kernels
            for (int i = 0; i < D_COMPUTE_UNIT; i++)
                decompress_kernel_lz4[i] = new cl::Kernel(*m_program, decompress_kernel_names[i].c_str());
        }
    }
   
    return 0;
}

int xil_lz4::release()
{
   
    if (m_bin_flow) {
        for(int i = 0; i < C_COMPUTE_UNIT; i++)
            delete(compress_kernel_lz4[i]);
    } else {
        for(int i = 0; i < D_COMPUTE_UNIT; i++)
            delete(decompress_kernel_lz4[i]);
    }
    delete(m_program);
    delete(m_q);
    delete(m_context);

    return 0;
}

uint32_t xil_lz4::decompress_file(std::string & inFile_name, 
                                  std::string & outFile_name
                                 ) {
    if (m_switch_flow == 0) {
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
        char magic_hdr[] = {MAGIC_BYTE_1, MAGIC_BYTE_2, MAGIC_BYTE_3, MAGIC_BYTE_4};
        for (int i = 0; i < MAGIC_HEADER_SIZE; i++) {
            inFile.get(c);
            if (c == magic_hdr[i])
                continue;
            else {
                std::cout << "Problem with magic header " << c << " " << i << std::endl;
                exit(1);
            }
        }
            
        // Header Checksum
        inFile.get(c);
        
        // Check if block size is 64 KB
        inFile.get(c);


        switch(c) {

            case BSIZE_STD_64KB:  m_block_size_in_kb = 64;break;
            case BSIZE_STD_256KB: m_block_size_in_kb = 256;break;
            case BSIZE_STD_1024KB:m_block_size_in_kb = 1024;break;
            case BSIZE_STD_4096KB:m_block_size_in_kb = 4096;break;
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

        uint32_t host_buffer_size = HOST_BUFFER_SIZE;
        uint32_t acc_buff_size = m_block_size_in_kb * 1024 * PARALLEL_BLOCK;
        if (acc_buff_size > host_buffer_size){
            host_buffer_size = acc_buff_size;
        }
        if (host_buffer_size > original_size){
            host_buffer_size = original_size;
        }

        // Decompression Overlapped multiple cu solution
        uint32_t debytes = decompress(in.data(), out.data(), (input_size - 15), original_size, host_buffer_size);

        // Decompression Sequential multiple cus.
        // uint32_t debytes = decompress_sequential(in.data(), out.data(), (input_size - 15), original_size);

        outFile.write((char *)out.data(), debytes);
        // Close file 
        inFile.close();
        outFile.close();
        return debytes;
    } else {
        std::string command = "./lz4 --content-size -f -q -d " + inFile_name;
        system(command.c_str());
        return 0;
    }
}

uint32_t xil_lz4::decompress(uint8_t *in,
                             uint8_t *out,
                             uint32_t input_size,
                             uint32_t original_size,
                             uint32_t host_buffer_size
                            ) {
    
    uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;
    uint64_t total_kernel_time = 0;

    // Total number of blocks exist for this file
    int total_block_cnt = (original_size - 1) / block_size_in_bytes + 1;
    int block_cntr = 0;
    int done_block_cntr = 0;
    uint32_t overlap_buf_count = OVERLAP_BUF_COUNT;

    // Read, Write and Kernel events
    cl::Event kernel_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event read_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event write_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];

    //Assignment to the buffer extensions
    buffer_extension_assignments(0);

    // Total chunks in input file
    // For example: Input file size is 12MB and Host buffer size is 2MB
    // Then we have 12/2 = 6 chunks exists 
    // Calculate the count of total chunks based on input size
    // This count is used to overlap the execution between chunks and file
    // operations

    uint32_t total_chunks = (original_size - 1) / host_buffer_size + 1;
    if(total_chunks < 2) overlap_buf_count = 1;

    // Find out the size of each chunk spanning entire file
    // For eaxmple: As mentioned in previous example there are 6 chunks
    // Code below finds out the size of chunk, in general all the chunks holds
    // HOST_BUFFER_SIZE except for the last chunk
    uint32_t sizeOfChunk[total_chunks];
    uint32_t blocksPerChunk[total_chunks];
    uint32_t computeBlocksPerChunk[total_chunks];
    uint32_t idx = 0;
    for (uint32_t i = 0; i < original_size; i += host_buffer_size, idx++) {
        uint32_t chunk_size = host_buffer_size;
        if (chunk_size + i > original_size) {
            chunk_size = original_size - i;
        }
        // Update size of each chunk buffer
        sizeOfChunk[idx] = chunk_size;
        // Calculate sub blocks of size BLOCK_SIZE_IN_KB for each chunk
        // 2MB(example)
        // Figure out blocks per chunk
        uint32_t nblocks = (chunk_size - 1) / block_size_in_bytes + 1;
        blocksPerChunk[idx] = nblocks;
        computeBlocksPerChunk[idx] = nblocks;
    }

    uint32_t temp_nblocks = (host_buffer_size - 1)/block_size_in_bytes + 1;
    host_buffer_size = ((host_buffer_size-1)/64 + 1) * 64;

    // Device buffer allocation
    for (int cu = 0; cu < D_COMPUTE_UNIT;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            // Input:- This buffer contains input chunk data
            buffer_input[cu][flag] = new cl::Buffer(*m_context, 
                                                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                    host_buffer_size,
                                                    &inExt[cu][flag]
                                                   );

            // Output:- This buffer contains compressed data written by device
            buffer_output[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &outExt[cu][flag]);

            // Ouput:- This buffer contains compressed block sizes
            buffer_compressed_size[cu][flag] = new cl::Buffer(*m_context, 
                                                          CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                          temp_nblocks * sizeof(uint32_t),
                                                          &csExt[cu][flag]);

            // Input:- This buffer contains origianl input block sizes
            buffer_block_size[cu][flag] = new cl::Buffer(*m_context, 
                                                     CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                     temp_nblocks * sizeof(uint32_t),
                                                     &bsExt[cu][flag]);
        }
    }   

    // Counter which helps in tracking output buffer index
    uint32_t outIdx = 0;

    // Track the flags of remaining chunks 
    int chunk_flags[total_chunks];
    int cu_order[total_chunks];

    // Finished bricks
    int completed_bricks = 0;

    int flag = 0;
    int lcl_cu = 0;
    uint32_t inIdx = 0;
    uint32_t total_decompression_size = 0;
    
    uint32_t init_itr = 0;
    if (total_chunks < 2)
        init_itr = 1;
    else 
        init_itr = 2 * D_COMPUTE_UNIT;

    auto total_start = std::chrono::high_resolution_clock::now();
    // Copy first few buffers
    for (uint32_t itr = 0, brick = 0; brick < init_itr; brick += D_COMPUTE_UNIT, itr++, flag = !flag) {
        
        lcl_cu = D_COMPUTE_UNIT;
        if (brick + lcl_cu > total_chunks)
            lcl_cu = total_chunks - brick;
        
        for (int cu = 0; cu < lcl_cu; cu++) {
            uint32_t total_size = 0;
            uint32_t compressed_size = 0;
            uint32_t block_size = 0;
            uint32_t nblocks = 0;
            uint32_t bufblocks = 0;
            uint32_t buf_size = 0;
            uint32_t no_compress_size = 0;
                
            for (uint32_t cIdx = 0; cIdx < sizeOfChunk[brick + cu]; cIdx+=block_size_in_bytes, total_size += block_size)  {              
                
                if (block_cntr == (total_block_cnt - 1)) {
                    block_size = original_size - done_block_cntr * block_size_in_bytes;
                } else {
                    block_size = block_size_in_bytes;
                } 

                std::memcpy(&compressed_size, &in[inIdx], 4); inIdx += 4;
                
                uint32_t tmp = compressed_size;
                tmp >>= 24;
            
                if (tmp == NO_COMPRESS_BIT) {
                    uint8_t b1 = compressed_size;
                    uint8_t b2 = compressed_size >> 8;
                    uint8_t b3 = compressed_size >> 16;
                
                    if (b3 == BSIZE_NCOMP_64 || b3 == BSIZE_NCOMP_4096 || b3 == BSIZE_NCOMP_256 || b3 == BSIZE_NCOMP_1024) {
                        compressed_size = block_size_in_bytes;
                    } else {
                        uint32_t size = 0;
                        size = b3;
                        size <<= 16;
                        uint32_t temp = b2; temp <<= 8;
                        size |= temp;
                        temp = b1; 
                        size |= temp;
                        compressed_size = size;       
                    }
                }
                
                m_blkSize[cu][flag].data()[nblocks] = block_size; 
                m_compressSize[cu][flag].data()[nblocks] = compressed_size; 
                nblocks++;
                
                if (compressed_size < block_size){
                    h_compressSize[cu][flag].data()[bufblocks]  = compressed_size;
                    h_blksize[cu][flag].data()[bufblocks] = block_size;
                    std::memcpy(&(h_buf_in[cu][flag].data()[buf_size]),&in[inIdx],compressed_size);
                    inIdx +=compressed_size;
                    buf_size +=block_size_in_bytes;
                    bufblocks++;
                }else if (compressed_size == block_size){
                    no_compress_size++;
                    
                    int outChunkIdx = brick + cu;
                    //No compression block
                    std::memcpy(&(out[outChunkIdx * host_buffer_size + cIdx]),&in[inIdx],block_size);
                    inIdx += block_size;
                    computeBlocksPerChunk[outChunkIdx]--;
                }else{
                    assert(0);
                }
                block_cntr++;
                done_block_cntr++;
               
            }
        }
    }
    flag = 0;
    // Main loop of overlap execution
    // Loop below runs over total bricks i.e., host buffer size chunks
    for (uint32_t brick = 0, itr = 0; brick < total_chunks; brick += D_COMPUTE_UNIT, itr++, flag=!flag) {
        
        lcl_cu = D_COMPUTE_UNIT;
        if (brick + lcl_cu > total_chunks)
            lcl_cu = total_chunks - brick;
        
        // Loop below runs over number of compute units
        for (int cu = 0; cu < lcl_cu; cu++) {
            chunk_flags[brick + cu] = flag;      
            cu_order[brick + cu] = cu;
            if (itr >= 2) {
                read_events[cu][flag].wait();

                completed_bricks++;

                // Accumulate Kernel time
                total_kernel_time += get_event_duration_ns(kernel_events[cu][flag]);
#ifdef EVENT_PROFILE
                // Accumulate Write time
                total_write_time += get_event_duration_ns(write_events[cu][flag]);
                // Accumulate Read time
                total_read_time += get_event_duration_ns(read_events[cu][flag]);
#endif

                int brick_flag_idx = brick - (D_COMPUTE_UNIT * overlap_buf_count - cu);
                uint32_t bufIdx = 0;
                for (uint32_t bIdx = 0; bIdx < blocksPerChunk[brick_flag_idx];bIdx++, idx += block_size_in_bytes) {
                    uint32_t block_size = m_blkSize[cu][flag].data()[bIdx];
                    uint32_t compressed_size = m_compressSize[cu][flag].data()[bIdx];
                    if (compressed_size < block_size) {
                        std::memcpy(&out[outIdx], &h_buf_out[cu][flag].data()[bufIdx], block_size);
                        outIdx += block_size;
                        bufIdx+=block_size_in_bytes;
                        total_decompression_size += block_size;
                    } else if (compressed_size == block_size) {
                        outIdx += block_size;
                        total_decompression_size += block_size;
                    }
                } // For loop ends here 
                
                uint32_t total_size = 0;
                uint32_t compressed_size = 0;
                uint32_t block_size = 0;
                uint32_t nblocks = 0;
                uint32_t bufblocks = 0;
                uint32_t buf_size = 0;
                uint32_t no_compress_size = 0;
                for (uint32_t cIdx = 0; cIdx < sizeOfChunk[brick + cu]; cIdx+=block_size_in_bytes, total_size += block_size)  {              
                    if (block_cntr == (total_block_cnt - 1)) {
                        block_size = original_size - done_block_cntr * block_size_in_bytes;
                    } else {
                        block_size = block_size_in_bytes;
                    } 

                    std::memcpy(&compressed_size, &in[inIdx], 4); inIdx += 4;
                    
                    uint32_t tmp = compressed_size;
                    tmp >>= 24;
                
                    if (tmp == NO_COMPRESS_BIT) {
                        uint8_t b1 = compressed_size;
                        uint8_t b2 = compressed_size >> 8;
                        uint8_t b3 = compressed_size >> 16;
                    
                        if (b3 == BSIZE_NCOMP_64 || b3 == BSIZE_NCOMP_4096 || b3 == BSIZE_NCOMP_256 || b3 == BSIZE_NCOMP_1024) {
                            compressed_size = block_size_in_bytes;
                        } else {
                            uint32_t size = 0;
                            size = b3;
                            size <<= 16;
                            uint32_t temp = b2; temp <<= 8;
                            size |= temp;
                            temp = b1; 
                            size |= temp;
                            compressed_size = size;       
                        }
                    }
                    
                    m_blkSize[cu][flag].data()[nblocks] = block_size; 
                    m_compressSize[cu][flag].data()[nblocks] = compressed_size; 
                    nblocks++;
                    if (compressed_size < block_size){
                        h_compressSize[cu][flag].data()[bufblocks]  = compressed_size;
                        h_blksize[cu][flag].data()[bufblocks] = block_size;
                        std::memcpy(&(h_buf_in[cu][flag].data()[buf_size]),&in[inIdx],compressed_size);
                        inIdx +=compressed_size;
                        buf_size +=block_size_in_bytes;
                        bufblocks++;
                    }else if (compressed_size == block_size){
                        no_compress_size++;
                        int outChunkIdx = brick + cu;
                        //No compression block
                        std::memcpy(&(out[outChunkIdx * host_buffer_size + cIdx]),&in[inIdx],block_size);
                        inIdx +=block_size;
                        computeBlocksPerChunk[outChunkIdx]--;
                    }else{
                        assert(0);
                    }
                    block_cntr++;
                    done_block_cntr++;
                } // Input forloop ends here
            } // If condition ends here
    

            // Set kernel arguments
            int narg = 0;
            decompress_kernel_lz4[cu]->setArg(narg++, *(buffer_input[cu][flag]));
            decompress_kernel_lz4[cu]->setArg(narg++, *(buffer_output[cu][flag]));
            decompress_kernel_lz4[cu]->setArg(narg++, *(buffer_block_size[cu][flag]));
            decompress_kernel_lz4[cu]->setArg(narg++, *(buffer_compressed_size[cu][flag]));
            decompress_kernel_lz4[cu]->setArg(narg++, m_block_size_in_kb);
            decompress_kernel_lz4[cu]->setArg(narg++, computeBlocksPerChunk[brick + cu]);
            
            // Kernel wait events for writing & compute
            std::vector<cl::Event> kernelWriteWait;
            std::vector<cl::Event> kernelComputeWait;

            // Migrate memory - Map host to device buffers
            m_q->enqueueMigrateMemObjects({*(buffer_input[cu][flag]), *(buffer_compressed_size[cu][flag]), *(buffer_block_size[cu][flag])},
                                           0, NULL, &(write_events[cu][flag]) /* 0 means from host*/);

            // Kernel write events update
            kernelWriteWait.push_back(write_events[cu][flag]);

            // Launch kernel
            m_q->enqueueTask(*decompress_kernel_lz4[cu], &kernelWriteWait, &(kernel_events[cu][flag]));
            
            // Update kernel events flag on computation
            kernelComputeWait.push_back(kernel_events[cu][flag]);

            // Migrate memory - Map device to host buffers
            m_q->enqueueMigrateMemObjects({*(buffer_output[cu][flag])}, 
                                          CL_MIGRATE_MEM_OBJECT_HOST, &kernelComputeWait, &(read_events[cu][flag]));
            
        }// Compute unit loop

    } // End of main loop    
    m_q->flush();
    m_q->finish();
    
    uint32_t leftover = total_chunks - completed_bricks;
    int stride = 0;

    if ((total_chunks < overlap_buf_count * D_COMPUTE_UNIT))
        stride = overlap_buf_count * D_COMPUTE_UNIT;
    else
        stride = total_chunks;

    // Handle leftover bricks
    for (uint32_t ovr_itr = 0, brick = stride - overlap_buf_count * D_COMPUTE_UNIT; ovr_itr < leftover; ovr_itr += D_COMPUTE_UNIT, brick += D_COMPUTE_UNIT) {

        lcl_cu = D_COMPUTE_UNIT;
        if (ovr_itr + lcl_cu > leftover)
            lcl_cu = leftover - ovr_itr;

        // Handle multiple bricks with multiple CUs
        for (int j = 0; j < lcl_cu; j++) {
            int cu = cu_order[brick + j];
            int flag = chunk_flags[brick + j];

            // Run over each block within brick
            int brick_flag_idx = brick + j;

            // Accumulate Kernel time
            total_kernel_time += get_event_duration_ns(kernel_events[cu][flag]);
#ifdef EVENT_PROFILE
            // Accumulate Write time
            total_write_time += get_event_duration_ns(write_events[cu][flag]);
            // Accumulate Read time
            total_read_time += get_event_duration_ns(read_events[cu][flag]);
#endif
            uint32_t bufIdx = 0;
            for (uint32_t bIdx = 0, idx = 0; bIdx < blocksPerChunk[brick_flag_idx];bIdx++, idx += block_size_in_bytes) {
                uint32_t block_size = m_blkSize[cu][flag].data()[bIdx];
                uint32_t compressed_size = m_compressSize[cu][flag].data()[bIdx];
                if (compressed_size < block_size) {
                    std::memcpy(&out[outIdx], &h_buf_out[cu][flag].data()[bufIdx], block_size);
                    outIdx += block_size;
                    bufIdx+=block_size_in_bytes;
                    total_decompression_size += block_size;
                } else if (compressed_size == block_size) {
                    outIdx += block_size;
                    total_decompression_size += block_size;
                }    
            } // For loop ends here 
        } // End of multiple CUs
    } // End of leftover bricks
    // Delete device buffers
    
    auto total_end = std::chrono::high_resolution_clock::now();   
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    float throughput_in_mbps_1 = (float)original_size * 1000 / total_time_ns.count();
    float kernel_throughput_in_mbps_1 = (float)original_size * 1000 / total_kernel_time;
#ifdef EVENT_PROFILE
    std::cout << "Total Kernel Time " << total_kernel_time << std::endl;
    std::cout << "Total Write Time " << total_write_time << std::endl;
    std::cout << "Total Read Time " << total_read_time << std::endl;
#endif    
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1 << "\t\t";
    std::cout << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
    
    for (int dBuf = 0; dBuf < D_COMPUTE_UNIT; dBuf++) {
        for (uint32_t flag = 0; flag < overlap_buf_count; flag++) {
            delete (buffer_input[dBuf][flag]);
            delete (buffer_output[dBuf][flag]);
            delete (buffer_compressed_size[dBuf][flag]);
            delete (buffer_block_size[dBuf][flag]);
        }
    }
    return original_size;
} // Decompress Overlap

// Note: Various block sizes supported by LZ4 standard are not applicable to
// this function. It just supports Block Size 64KB
uint32_t xil_lz4::decompress_sequential(uint8_t *in,
                            uint8_t *out,
                            uint32_t input_size,
                            uint32_t original_size
                           ) {
    uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;

    // Total number of blocks exists for this file
    int total_block_cnt = (original_size - 1) / block_size_in_bytes + 1;
    int block_cntr = 0;
    int done_block_cntr = 0;

    uint32_t no_compress_case=0;
    std::chrono::duration<double, std::nano> kernel_time_ns_1(0);
    uint32_t inIdx = 0;
    uint32_t total_decomression_size=0;

    uint32_t hostChunk_cu[D_COMPUTE_UNIT];
    int compute_cu;
    int output_idx = 0;    

    for (uint32_t outIdx = 0 ; outIdx < original_size ; outIdx +=HOST_BUFFER_SIZE * D_COMPUTE_UNIT){
        compute_cu = 0;
        uint32_t chunk_size  = HOST_BUFFER_SIZE;

        // Figure out the chunk size for each compute unit
        for (int bufCalc = 0; bufCalc < D_COMPUTE_UNIT; bufCalc++) {
            hostChunk_cu[bufCalc] = 0;
            if (outIdx + (chunk_size * (bufCalc + 1)) > original_size) {
                hostChunk_cu[bufCalc] = original_size - (outIdx + HOST_BUFFER_SIZE * bufCalc);
                compute_cu++;   
                break;
            } else {
                hostChunk_cu[bufCalc] = chunk_size;
                compute_cu++;
            }
        }

        uint32_t nblocks[D_COMPUTE_UNIT];
        uint32_t bufblocks[D_COMPUTE_UNIT];
        uint32_t total_size[D_COMPUTE_UNIT];
        uint32_t buf_size[D_COMPUTE_UNIT];
        uint32_t block_size = 0;
        uint32_t compressed_size = 0;
        
        for (int cuProc = 0; cuProc < compute_cu; cuProc++) {
            nblocks[cuProc] = 0;
            buf_size[cuProc] = 0;
            bufblocks[cuProc] = 0;
            total_size[cuProc] = 0;
            for (uint32_t cIdx = 0; cIdx < hostChunk_cu[cuProc]; cIdx +=block_size_in_bytes,nblocks[cuProc]++,total_size[cuProc] +=block_size)
            {
                if (block_cntr == (total_block_cnt - 1)) {
                    block_size = original_size - done_block_cntr * block_size_in_bytes;
                } else {
                    block_size = block_size_in_bytes;
                }

                std::memcpy(&compressed_size,&in[inIdx],4); inIdx +=4;

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
                
                // Fill original block size and compressed size        
                m_blkSize[cuProc][0].data()[nblocks[cuProc]] = block_size; 
                m_compressSize[cuProc][0].data()[nblocks[cuProc]] = compressed_size;
 
                // If compressed size is less than original block size
                if (compressed_size < block_size){
                    h_compressSize[cuProc][0].data()[bufblocks[cuProc]]  = compressed_size;
                    h_blksize[cuProc][0].data()[bufblocks[cuProc]] = block_size;
                    std::memcpy(&(h_buf_in[cuProc][0].data()[buf_size[cuProc]]),&in[inIdx],compressed_size);
                    inIdx +=compressed_size;
                    buf_size[cuProc] +=block_size_in_bytes;
                    bufblocks[cuProc]++;
                }else if (compressed_size == block_size){
                    no_compress_case++;
                    //No compression block
                    std::memcpy(&(out[outIdx + cuProc * HOST_BUFFER_SIZE + cIdx]),&in[inIdx],block_size);
                    inIdx += block_size;
                }else{
                    assert(0);
                }
                block_cntr++;
                done_block_cntr++;
            }
            assert(total_size[cuProc]<=original_size);
            
            if (nblocks[cuProc] == 1 && compressed_size == block_size)
                break;    
        } // Cu process done
        
        for (int bufC = 0; bufC < compute_cu; bufC++) { 
            // Device buffer allocation
            buffer_input[bufC][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  buf_size[bufC], &inExt[bufC]);

            buffer_output[bufC][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX,  buf_size[bufC], &outExt[bufC]);

            buffer_block_size[bufC][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  sizeof(uint32_t) * bufblocks[bufC], &bsExt[bufC]);
            
            buffer_compressed_size[bufC][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  sizeof(uint32_t) * bufblocks[bufC], &csExt[bufC]);
        }

        // Set kernel arguments
        for (int sArg = 0; sArg < compute_cu; sArg++) {
            uint32_t narg = 0;
            decompress_kernel_lz4[sArg]->setArg(narg++, *(buffer_input[sArg][0]));
            decompress_kernel_lz4[sArg]->setArg(narg++, *(buffer_output[sArg][0]));
            decompress_kernel_lz4[sArg]->setArg(narg++, *(buffer_block_size[sArg][0]));
            decompress_kernel_lz4[sArg]->setArg(narg++, *(buffer_compressed_size[sArg][0]));
            decompress_kernel_lz4[sArg]->setArg(narg++, m_block_size_in_kb);
            decompress_kernel_lz4[sArg]->setArg(narg++, bufblocks[sArg]);
        }

        std::vector<cl::Memory> inBufVec;
        for (int inVec = 0; inVec < compute_cu; inVec++) {
            inBufVec.push_back(*(buffer_input[inVec][0]));
            inBufVec.push_back(*(buffer_block_size[inVec][0]));
            inBufVec.push_back(*(buffer_compressed_size[inVec][0]));
        }
        
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
        m_q->finish();
        
        auto kernel_start = std::chrono::high_resolution_clock::now(); 
        // Kernel invocation
        for (int task = 0; task < compute_cu; task++) {
            m_q->enqueueTask(*decompress_kernel_lz4[task]);
        }
        m_q->finish();

        auto kernel_end = std::chrono::high_resolution_clock::now();    
        auto duration   = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
        kernel_time_ns_1 += duration;
        
        std::vector<cl::Memory> outBufVec;
        for (int oVec = 0; oVec < compute_cu; oVec++) 
            outBufVec.push_back(*(buffer_output[oVec][0]));

        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
        m_q->finish();
        
        for (int cuRead = 0; cuRead < compute_cu; cuRead++) {
            uint32_t bufIdx=0;
            for (uint32_t bIdx = 0, idx=0 ; bIdx < nblocks[cuRead]; bIdx++, idx +=block_size_in_bytes){
                uint32_t block_size         = m_blkSize[cuRead][0].data()[bIdx];
                uint32_t compressed_size    = m_compressSize[cuRead][0].data()[bIdx];
                if (compressed_size < block_size){
                    std::memcpy(&out[output_idx], &h_buf_out[cuRead][0].data()[bufIdx], block_size);
                    output_idx += block_size;
                    bufIdx += block_size;
                    total_decomression_size += block_size;
                } else if (compressed_size == block_size) {
                    output_idx += block_size;
                }
            }
        } // CU processing ends
        
        // Delete device buffers   
        for (int dBuf = 0; dBuf < compute_cu; dBuf++) {
            delete (buffer_input[dBuf][0]);
            delete (buffer_output[dBuf][0]);
            delete (buffer_block_size[dBuf][0]);
            delete (buffer_compressed_size[dBuf][0]);
        }
    } // Top - Main loop ends here
    
    float throughput_in_mbps_1 = (float)total_decomression_size* 1000 / kernel_time_ns_1.count();
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1;
    return original_size;

} // End of decompress

// This version of compression does overlapped execution between
// Kernel and Host. I/O operations between Host and Device are
// overlapped with Kernel execution between multiple compute units
uint32_t xil_lz4::compress(uint8_t *in,
                           uint8_t *out,
                           uint32_t input_size,
                           uint32_t host_buffer_size
                         ) {

    uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;
    uint32_t overlap_buf_count = OVERLAP_BUF_COUNT;
    uint64_t total_kernel_time = 0;

    // Read, Write and Kernel events
    cl::Event kernel_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event read_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];
    cl::Event write_events[MAX_COMPUTE_UNITS][OVERLAP_BUF_COUNT];

    //Assignment to the buffer extensions
    buffer_extension_assignments(1);

    // Total chunks in input file
    // For example: Input file size is 12MB and Host buffer size is 2MB
    // Then we have 12/2 = 6 chunks exists 
    // Calculate the count of total chunks based on input size
    // This count is used to overlap the execution between chunks and file
    // operations
    
    uint32_t total_chunks = (input_size - 1) / host_buffer_size + 1;

    if(total_chunks < 2) overlap_buf_count = 1;

    // Find out the size of each chunk spanning entire file
    // For eaxmple: As mentioned in previous example there are 6 chunks
    // Code below finds out the size of chunk, in general all the chunks holds
    // HOST_BUFFER_SIZE except for the last chunk
    uint32_t sizeOfChunk[total_chunks];
    uint32_t blocksPerChunk[total_chunks];
    uint32_t idx = 0;
    for (uint32_t i = 0; i < input_size; i += host_buffer_size, idx++) {
        uint32_t chunk_size = host_buffer_size;
        if (chunk_size + i > input_size) {
            chunk_size = input_size - i;
        }
        // Update size of each chunk buffer
        sizeOfChunk[idx] = chunk_size;
        // Calculate sub blocks of size BLOCK_SIZE_IN_KB for each chunk
        // 2MB(example)
        // Figure out blocks per chunk
        uint32_t nblocks = (chunk_size - 1) / block_size_in_bytes + 1;
        blocksPerChunk[idx] = nblocks;
    }

    uint32_t temp_nblocks = (host_buffer_size - 1)/block_size_in_bytes + 1;
    host_buffer_size = ((host_buffer_size-1)/64 + 1) * 64;

    // Device buffer allocation
    for (int cu = 0; cu < C_COMPUTE_UNIT;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            // Input:- This buffer contains input chunk data
            buffer_input[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(inExt[cu][flag]));

            // Output:- This buffer contains compressed data written by device
            buffer_output[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(outExt[cu][flag]));

            // Ouput:- This buffer contains compressed block sizes
            buffer_compressed_size[cu][flag] = new cl::Buffer(*m_context, 
                                                        CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                        temp_nblocks * sizeof(uint32_t),
                                                        &(csExt[cu][flag]));

            // Input:- This buffer contains origianl input block sizes
            buffer_block_size[cu][flag] = new cl::Buffer(*m_context, 
                                                     CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,
                                                     temp_nblocks * sizeof(uint32_t),
                                                     &(bsExt[cu][flag]));
        }
    }

    // Counter which helps in tracking
    // Output buffer index    
    uint32_t outIdx = 0;

    // Track the lags of respective chunks for left over handling
    int chunk_flags[total_chunks];
    int cu_order[total_chunks];
    
    // Finished bricks
    int completed_bricks = 0;

    int flag = 0; 
    int lcl_cu = 0;

    // Main loop of overlap execution
    // Loop below runs over total bricks i.e., host buffer size chunks
    auto total_start = std::chrono::high_resolution_clock::now();
    for (uint32_t brick = 0, itr = 0; brick < total_chunks; brick+=C_COMPUTE_UNIT, itr++, flag =!flag) {
   
        lcl_cu = C_COMPUTE_UNIT;
        if (brick + lcl_cu > total_chunks) 
            lcl_cu = total_chunks - brick;
        // Loop below runs over number of compute units
        for (int cu = 0; cu < lcl_cu; cu++) {
            
            chunk_flags[brick + cu] = flag;      
            cu_order[brick + cu] = cu;
            // Wait on read events
            if (itr >= 2) {
                // Wait on current flag previous operation to finish
                read_events[cu][flag].wait();
                
                // Completed bricks counter
                completed_bricks++;

                // Accumulate Kernel time
                total_kernel_time += get_event_duration_ns(kernel_events[cu][flag]);
#ifdef EVENT_PROFILE
                // Accumulate Write time
                total_write_time += get_event_duration_ns(write_events[cu][flag]);
                // Accumulate Read time
                total_read_time += get_event_duration_ns(read_events[cu][flag]);
#endif
                // Run over each block of the within brick
                uint32_t index = 0;
                int brick_flag_idx = brick - (C_COMPUTE_UNIT * overlap_buf_count - cu);
                for (uint32_t bIdx = 0; bIdx < blocksPerChunk[brick_flag_idx]; bIdx++, index += block_size_in_bytes) {
                    uint32_t block_size = block_size_in_bytes;
                    if (index + block_size > sizeOfChunk[brick_flag_idx]) {
                        block_size = sizeOfChunk[brick_flag_idx] - index;
                    }
                    // Figure out the compressed size 
                    uint32_t compressed_size = (h_compressSize[cu][flag]).data()[bIdx];
                    assert(compressed_size != 0);

                    int orig_chunk_size = sizeOfChunk[brick_flag_idx];
                    int perc_cal = orig_chunk_size * 10;
                    perc_cal = perc_cal / block_size;

                    // If compressed size is less than original block size
                    // It means better to dump encoded bytes
                    if (compressed_size < block_size && perc_cal >= 10) {
                        std::memcpy(&out[outIdx], &compressed_size, 4); outIdx += 4;
                        std::memcpy(&out[outIdx], (h_buf_out[cu][flag]).data() + bIdx * block_size_in_bytes, compressed_size);
                        outIdx += compressed_size;
                    } else {
                        if (block_size == block_size_in_bytes) {
                            out[outIdx++] = 0;
                            out[outIdx++] = 0;

                            if (block_size == MAX_BSIZE_64KB)
                                out[outIdx++] = BSIZE_NCOMP_64;
                            else if (block_size == MAX_BSIZE_256KB)
                                out[outIdx++] = BSIZE_NCOMP_256;
                            else if (block_size == MAX_BSIZE_1024KB)
                                out[outIdx++] = BSIZE_NCOMP_1024;
                            else if (block_size == MAX_BSIZE_4096KB)
                                out[outIdx++] = BSIZE_NCOMP_4096;

                            out[outIdx++] = NO_COMPRESS_BIT;
                        } else {
                            uint8_t temp = 0;
                            temp = block_size;
                            out[outIdx++] = temp;
                            temp = block_size >> 8;
                            out[outIdx++] = temp;
                            temp = block_size >> 16;
                            out[outIdx++] = temp;
                            out[outIdx++] = NO_COMPRESS_BIT;
                        }
                        std::memcpy(&out[outIdx], &in[brick_flag_idx * host_buffer_size + index], block_size);
                        outIdx += block_size;
                    } // End of else - uncompressed stream update
                }        
            } 
            // Figure out block sizes per brick
            uint32_t bIdx = 0; 
            for (uint32_t i = 0; i < sizeOfChunk[brick + cu]; i+=block_size_in_bytes) {
                uint32_t block_size = block_size_in_bytes;
                if (i+block_size > sizeOfChunk[brick + cu]){
                    block_size = sizeOfChunk[brick + cu] - i;
                }
                (h_blksize[cu][flag]).data()[bIdx++] = block_size;
            } 
            
            // Copy data from input buffer to host
            std::memcpy(h_buf_in[cu][flag].data(), &in[(brick + cu) * host_buffer_size], sizeOfChunk[brick + cu]);
      
            // Set kernel arguments
            int narg = 0;
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_input[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_output[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_compressed_size[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, *(buffer_block_size[cu][flag]));
            compress_kernel_lz4[cu]->setArg(narg++, m_block_size_in_kb);
            compress_kernel_lz4[cu]->setArg(narg++, sizeOfChunk[brick + cu]);
            
            // Transfer data from host to device
            m_q->enqueueMigrateMemObjects({*(buffer_input[cu][flag]), *(buffer_block_size[cu][flag])}, 0, NULL, &(write_events[cu][flag]));
            
            // Kernel wait events for writing & compute
            std::vector<cl::Event> kernelWriteWait;
            std::vector<cl::Event> kernelComputeWait;

            // Kernel Write events update
            kernelWriteWait.push_back(write_events[cu][flag]);
            
            // Fire the kernel
            m_q->enqueueTask(*compress_kernel_lz4[cu], &kernelWriteWait, &(kernel_events[cu][flag]));
            
            // Update kernel events flag on computation
            kernelComputeWait.push_back(kernel_events[cu][flag]);
            
            // Transfer data from device to host
            m_q->enqueueMigrateMemObjects({*(buffer_output[cu][flag]), *(buffer_compressed_size[cu][flag])}, 
                                            CL_MIGRATE_MEM_OBJECT_HOST, &kernelComputeWait, &(read_events[cu][flag]));
        } // Compute unit loop ends here

    } // Main loop ends here       
    m_q->flush();
    m_q->finish();
    
    int leftover = total_chunks - completed_bricks;
    int stride = 0;
    
    if ((total_chunks < overlap_buf_count * C_COMPUTE_UNIT))
        stride = overlap_buf_count * C_COMPUTE_UNIT;
    else 
        stride = total_chunks;

    // Handle leftover bricks
    for (int ovr_itr = 0, brick = stride - overlap_buf_count * C_COMPUTE_UNIT; ovr_itr < leftover; ovr_itr+=C_COMPUTE_UNIT, brick+=C_COMPUTE_UNIT) {
        
        lcl_cu = C_COMPUTE_UNIT;
        if (ovr_itr + lcl_cu > leftover) 
            lcl_cu = leftover - ovr_itr;
            
        // Handlue multiple bricks with multiple CUs
        for (int j = 0; j < lcl_cu; j++) {  
            int cu = cu_order[brick + j];
            int flag = chunk_flags[brick + j];
            // Run over each block of the within brick
            uint32_t index = 0;
            int brick_flag_idx = brick+j;

            // Accumulate Kernel time
            total_kernel_time += get_event_duration_ns(kernel_events[cu][flag]);
#ifdef EVENT_PROFILE
            // Accumulate Write time
            total_write_time += get_event_duration_ns(write_events[cu][flag]);
            // Accumulate Read time
            total_read_time += get_event_duration_ns(read_events[cu][flag]);
#endif
            for (uint32_t bIdx = 0; bIdx < blocksPerChunk[brick_flag_idx]; bIdx++, index += block_size_in_bytes) {
                uint32_t block_size = block_size_in_bytes;
                if (index + block_size > sizeOfChunk[brick_flag_idx]){
                    block_size = sizeOfChunk[brick_flag_idx] - index;
                }
                
                // Figure out the compressed size 
                uint32_t compressed_size = (h_compressSize[cu][flag]).data()[bIdx];
                assert(compressed_size != 0);

                int orig_chunk_size = sizeOfChunk[brick_flag_idx];
                int perc_cal = orig_chunk_size * 10;
                perc_cal = perc_cal / block_size;

                // If compressed size is less than original block size
                // It means better to dump encoded bytes
                if (compressed_size < block_size && perc_cal >= 10) {
                    std::memcpy(&out[outIdx], &compressed_size, 4); outIdx += 4;
                    std::memcpy(&out[outIdx], &h_buf_out[cu][flag].data()[bIdx * block_size_in_bytes], compressed_size);
                    outIdx += compressed_size;
                } else {
                    if (block_size == block_size_in_bytes) {
                        out[outIdx++] = 0;
                        out[outIdx++] = 0;
                        
                        if (block_size == MAX_BSIZE_64KB)
                            out[outIdx++] = BSIZE_NCOMP_64;
                        else if (block_size == MAX_BSIZE_256KB)
                            out[outIdx++] = BSIZE_NCOMP_256;
                        else if (block_size == MAX_BSIZE_1024KB)
                            out[outIdx++] = BSIZE_NCOMP_1024;
                        else if (block_size == MAX_BSIZE_4096KB)
                            out[outIdx++] = BSIZE_NCOMP_4096;
                   
                        out[outIdx++] = NO_COMPRESS_BIT;
                    } else {
                        uint8_t temp = 0;
                        temp = block_size;
                        out[outIdx++] = temp;
                        temp = block_size >> 8;
                        out[outIdx++] = temp;
                        temp = block_size >> 16;
                        out[outIdx++] = temp;
                        out[outIdx++] = NO_COMPRESS_BIT;
                    }
                    std::memcpy(&out[outIdx], &in[brick_flag_idx * host_buffer_size + index], block_size);
                    outIdx += block_size;
                } // End of else - uncompressed stream update
                
            } // For loop ends
        } // cu loop ends here
    } // Main loop ends here
    
    auto total_end = std::chrono::high_resolution_clock::now();   
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    float throughput_in_mbps_1 = (float)input_size * 1000 / total_time_ns.count();
    float kernel_throughput_in_mbps_1 = (float)input_size * 1000 / total_kernel_time;
#ifdef EVENT_PROFILE
    std::cout << "Total Kernel Time " << total_kernel_time << std::endl;
    std::cout << "Total Write Time " << total_write_time << std::endl;
    std::cout << "Total Read Time " << total_read_time << std::endl;
#endif    
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1 << "\t\t";
    std::cout << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
            
    for (int cu = 0; cu < C_COMPUTE_UNIT;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            delete (buffer_input[cu][flag]);
            delete (buffer_output[cu][flag]);
            delete (buffer_compressed_size[cu][flag]);
            delete (buffer_block_size[cu][flag]);
        }
    }

    return outIdx; 
} // Overlap end

// Note: Various block sizes supported by LZ4 standard are not applicable to
// this function. It just supports Block Size 64KB
uint32_t xil_lz4::compress_sequential(uint8_t *in, 
                                      uint8_t *out, 
                                      uint32_t input_size
                                    ) {
    uint32_t block_size_in_kb = BLOCK_SIZE_IN_KB;
    uint32_t block_size_in_bytes = block_size_in_kb * 1024;

    uint32_t no_compress_case=0;
   
    std::chrono::duration<double, std::nano> kernel_time_ns_1(0);
    
    // Keeps track of output buffer index
    uint32_t outIdx = 0;
        
    // Given a input file, we process it as multiple chunks
    // Each compute unit is assigned with a chunk of data
    // In this example HOST_BUFFER_SIZE is the chunk size.
    // For example: Input file = 12 MB
    //              HOST_BUFFER_SIZE = 2MB
    // Each compute unit processes 2MB data per kernel invocation
    uint32_t hostChunk_cu[C_COMPUTE_UNIT];

    // This buffer contains total number of BLOCK_SIZE_IN_KB blocks per CU
    // For Example: HOST_BUFFER_SIZE = 2MB/BLOCK_SIZE_IN_KB = 32block (Block
    // size 64 by default)
    uint32_t total_blocks_cu[C_COMPUTE_UNIT];

    // This buffer holds exact size of the chunk in bytes for all the CUs
    uint32_t bufSize_in_bytes_cu[C_COMPUTE_UNIT];

    // Holds value of total compute units to be 
    // used per iteration
    int compute_cu = 0;    

    for (uint32_t inIdx = 0 ; inIdx < input_size ; inIdx+= HOST_BUFFER_SIZE * C_COMPUTE_UNIT){
        
        // Needs to reset this variable
        // As this drives compute unit launch per iteration
        compute_cu = 0;
        
        // Pick buffer size as predefined one
        // If yet to be consumed input is lesser
        // the reset to required size
        uint32_t buf_size  = HOST_BUFFER_SIZE;

        // This loop traverses through each compute based current inIdx
        // It tries to calculate chunk size and total compute units need to be
        // launched (based on the input_size) 
       for (int bufCalc = 0; bufCalc < C_COMPUTE_UNIT; bufCalc++) {
            hostChunk_cu[bufCalc] = 0;
            // If amount of data to be consumed is less than HOST_BUFFER_SIZE
            // Then choose to send is what is needed instead of full buffer size
            // based on host buffer macro
            if (inIdx + (buf_size * (bufCalc + 1)) > input_size) {
                hostChunk_cu[bufCalc] = input_size - (inIdx + HOST_BUFFER_SIZE * bufCalc);
                compute_cu++;
                break;
            } else  {
                hostChunk_cu[bufCalc] = buf_size;
                compute_cu++;
            }
        }
        // Figure out total number of blocks need per each chunk
        // Copy input data from in to host buffer based on the inIdx and cu
        for (int blkCalc = 0; blkCalc < compute_cu; blkCalc++) {
            uint32_t nblocks = (hostChunk_cu[blkCalc] - 1) / block_size_in_bytes + 1;
            total_blocks_cu[blkCalc] = nblocks;
            std::memcpy(h_buf_in[blkCalc][0].data(), &in[inIdx + blkCalc * HOST_BUFFER_SIZE], hostChunk_cu[blkCalc]);
        }
       
        // Fill the host block size buffer with various block sizes per chunk/cu
        for (int cuBsize = 0; cuBsize < compute_cu; cuBsize++) {
        
            uint32_t bIdx = 0; 
            uint32_t chunkSize_curr_cu = hostChunk_cu[cuBsize];
        
            for (uint32_t bs = 0; bs < chunkSize_curr_cu; bs +=block_size_in_bytes) {
                uint32_t block_size = block_size_in_bytes;
                if (bs + block_size > chunkSize_curr_cu){
                    block_size = chunkSize_curr_cu - bs;
                }
                h_blksize[cuBsize][0].data()[bIdx++] = block_size;
            }

            // Calculate chunks size in bytes for device buffer creation
            bufSize_in_bytes_cu[cuBsize] = ((hostChunk_cu[cuBsize] - 1) / BLOCK_SIZE_IN_KB + 1) * BLOCK_SIZE_IN_KB;
        }
        for (int devbuf = 0; devbuf < compute_cu; devbuf++) {
            // Device buffer allocation
            buffer_input[devbuf][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  bufSize_in_bytes_cu[devbuf], &inExt[devbuf]);

            buffer_output[devbuf][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, bufSize_in_bytes_cu[devbuf], &outExt[devbuf]);

            buffer_compressed_size[devbuf][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(uint32_t) * total_blocks_cu[devbuf], 
                    &csExt[devbuf]);

            buffer_block_size[devbuf][0] = new cl::Buffer(*m_context, 
                    CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX, sizeof(uint32_t) * total_blocks_cu[devbuf], 
                    &bsExt[devbuf]);
        }
        
        // Set kernel arguments
        int narg = 0;
        for (int argset = 0; argset < compute_cu; argset++) {
            narg = 0;
            compress_kernel_lz4[argset]->setArg(narg++, *(buffer_input[argset][0]));
            compress_kernel_lz4[argset]->setArg(narg++, *(buffer_output[argset][0]));
            compress_kernel_lz4[argset]->setArg(narg++, *(buffer_compressed_size[argset][0]));
            compress_kernel_lz4[argset]->setArg(narg++, *(buffer_block_size[argset][0]));
            compress_kernel_lz4[argset]->setArg(narg++, block_size_in_kb);
            compress_kernel_lz4[argset]->setArg(narg++, hostChunk_cu[argset]);
        }
        std::vector<cl::Memory> inBufVec;
    
        for (int inbvec = 0; inbvec < compute_cu; inbvec++) {
            inBufVec.push_back(*(buffer_input[inbvec][0]));
            inBufVec.push_back(*(buffer_block_size[inbvec][0]));
        }
        
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
        m_q->finish();
        
        // Measure kernel execution time
        auto kernel_start = std::chrono::high_resolution_clock::now(); 

        // Fire kernel execution
        for (int ker = 0; ker < compute_cu; ker++)
            m_q->enqueueTask(*compress_kernel_lz4[ker]);
        // Wait till kernels complete
        m_q->finish();


        auto kernel_end = std::chrono::high_resolution_clock::now();    
        auto duration   = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
        kernel_time_ns_1 += duration;
        
        // Setup output buffer vectors
        std::vector<cl::Memory> outBufVec;
        for (int outbvec = 0; outbvec < compute_cu; outbvec++) {
            outBufVec.push_back(*(buffer_output[outbvec][0]));
            outBufVec.push_back(*(buffer_compressed_size[outbvec][0]));
        }

        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
        m_q->finish();

        for (int cuCopy = 0; cuCopy < compute_cu; cuCopy++) {
            // Copy data into out buffer 
            // Include compress and block size data
            // Copy data block by block within a chunk example 2MB (64block size) - 32 blocks data
            // Do the same for all the compute units
            uint32_t idx=0;
            for(uint32_t bIdx = 0; bIdx < total_blocks_cu[cuCopy]; bIdx++, idx += block_size_in_bytes) {
                // Default block size in bytes i.e., 64 * 1024
                uint32_t block_size = block_size_in_bytes;
                if (idx+block_size > hostChunk_cu[cuCopy]){
                    block_size = hostChunk_cu[cuCopy] -idx;
                }
                uint32_t compressed_size = h_compressSize[cuCopy][0].data()[bIdx];
                assert(compressed_size != 0);
                
                int orig_block_size = hostChunk_cu[cuCopy];
                int perc_cal = orig_block_size * 10;
                perc_cal = perc_cal / block_size;
                
                if (compressed_size < block_size && perc_cal >= 10){ 
                    memcpy(&out[outIdx],&compressed_size,4); outIdx +=4;
                    std::memcpy(&out[outIdx], &(h_buf_out[cuCopy][0].data()[bIdx * block_size_in_bytes]), compressed_size); 
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
                    std::memcpy(&out[outIdx], &in[inIdx + cuCopy * HOST_BUFFER_SIZE + idx], block_size);          
                    outIdx += block_size;
                }
            } // End of chunk (block by block) copy to output buffer
        } // End of CU loop - Each CU/chunk block by block copy
        // Buffer deleted
        for (int bdel = 0; bdel < compute_cu; bdel++) {
            delete (buffer_input[bdel][0]);
            delete (buffer_output[bdel][0]);
            delete (buffer_compressed_size[bdel][0]);
            delete (buffer_block_size[bdel][0]);
        }
    }
    float throughput_in_mbps_1 = (float)input_size * 1000 / kernel_time_ns_1.count();
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1;
    return outIdx;
}
