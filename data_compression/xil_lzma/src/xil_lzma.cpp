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
#include <exception>
#include "crc.h"
#define BLOCK_SIZE 64
#define KB 1024
#define MAGIC_HEADER_SIZE 4
#define MAGIC_BYTE_1 4
#define MAGIC_BYTE_2 34
#define MAGIC_BYTE_3 77
#define MAGIC_BYTE_4 24
#define FLG_BYTE 104

uint64_t xil_lzma::get_event_duration_ns(const cl::Event &event){

    uint64_t start_time=0, end_time=0;

    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_START, &start_time);
    event.getProfilingInfo<uint64_t>(CL_PROFILING_COMMAND_END, &end_time);
    return (end_time - start_time);
}

uint64_t xil_lzma::compress_file(std::string & inFile_name, 
                                std::string & outFile_name,
                                int cu
                               ) 
{
    if (m_switch_flow == 0) { // Xilinx FPGA compression flow
        std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
        std::ofstream outFile(outFile_name, std::ofstream::out |std::ofstream::binary);
        uint64_t enbytes = 0;
        if(!inFile) {
            std::cout << "Unable to open file";
            exit(1);
        }
        if(!outFile) {
            std::cout << "Unable to open file";
            exit(1);
        }

        std::vector<uint8_t> in(2147483648);
        uint64_t Full_input_size = get_bigfile_size(inFile);
        uint32_t input_size;
		
        for(uint64_t done_size =0; done_size < Full_input_size; done_size+=input_size) {
            if(done_size + 2147483648 < Full_input_size)
                input_size = 2147483648;
            else
                input_size = Full_input_size-done_size;
            std::vector<uint8_t,aligned_allocator<uint8_t>> out;//(input_size * 4 );
            inFile.read((char *)in.data(),input_size); 			
            uint32_t host_buffer_size = HOST_BUFFER_SIZE;
            uint32_t acc_buff_size = m_block_size_in_kb * 1024 * PARALLEL_BLOCK;
            if (acc_buff_size > host_buffer_size){
                host_buffer_size = acc_buff_size;
            }
            if (host_buffer_size > input_size){
                host_buffer_size = input_size;
            }
            enbytes += compress(in.data(), NULL, input_size, host_buffer_size,&outFile,cu);
            //std::cout<<"enbytes:"<<enbytes<<"\n";  
        }
        // Close file 
        inFile.close();
        outFile.close();
        return enbytes;
    }
    return 0;
}

uint64_t xil_lzma::compress_buffer(char* in, 
                                char* out,
                                uint64_t input_size,
                                uint64_t out_size,
                                int cu
                               ) 
{
    if (m_switch_flow == 0) { // Xilinx FPGA compression flow
        uint64_t enbytes = 0;
        uint32_t host_buffer_size = HOST_BUFFER_SIZE;
        uint32_t acc_buff_size = m_block_size_in_kb * 1024 * PARALLEL_BLOCK;
        if (acc_buff_size > host_buffer_size){
            host_buffer_size = acc_buff_size;
        }
        if (host_buffer_size > input_size){
            host_buffer_size = input_size;
        }
        enbytes += compress((uint8_t*)in, (uint8_t*)out, input_size, host_buffer_size,NULL,cu);
        return enbytes;
    }
    return 0;
}

int validate(std::string & inFile_name, std::string & outFile_name) {

    std::string command = "cmp " + inFile_name + " " + outFile_name;
    int ret = system(command.c_str());
    return ret;
}

void xil_lzma::buffer_extension_assignments(int cu_run){
    for (int i = 0; i < MAX_COMPUTE_UNITS; i++) {
        for (int j = 0; j < OVERLAP_BUF_COUNT; j++){
                inExt[i][j].flags = comp_ddr_nums[cu_run];
                inExt[i][j].obj   = h_buf_in[i][j].data();
                inExt[i][j].param   = 0;

                outExt[i][j].flags = comp_ddr_nums[cu_run];
                outExt[i][j].obj   = h_buf_out[i][j].data();
                outExt[i][j].param   = 0;

                csExt[i][j].flags = comp_ddr_nums[cu_run];
                csExt[i][j].obj   = h_compressSize[i][j].data();
                csExt[i][j].param   = 0;

                bsExt[i][j].flags = comp_ddr_nums[cu_run];
                bsExt[i][j].obj   = h_blksize[i][j].data();
                bsExt[i][j].param   = 0;
        }

        dbuf[i][0].flags = dict_ddr_nums[cu_run];
        dbuf[i][0].obj = Gdict1[i].data();
        dbuf[i][0].param = 0;

        dbuf[i][1].flags = dict_ddr_nums[cu_run];
        dbuf[i][1].obj = Gdict2[i].data();
        dbuf[i][1].param = 0;

        dbuf[i][2].flags = dict_ddr_nums[cu_run];
        dbuf[i][2].obj = Gdict3[i].data();
        dbuf[i][2].param = 0;
    }
}

// Constructor
xil_lzma::xil_lzma(){

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

        Gdict1[i].resize(MEM_ALLOC_CPU,255);
        Gdict2[i].resize(1*1024*1024,255);
        Gdict3[i].resize(1*1024*1024,255);
    }
}   

// Destructor
xil_lzma::~xil_lzma(){
}

int xil_lzma::init(const std::string& binaryFileName)
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
   
    for (int i = 0; i < C_COMPUTE_UNIT; i++)
        compress_kernel_lzma[i] = new cl::Kernel(*m_program, compress_kernel_names[i].c_str());

    return 0;
}

int xil_lzma::release()
{
    for(int i = 0; i < C_COMPUTE_UNIT; i++)
        delete(compress_kernel_lzma[i]);

    delete(m_program);
    delete(m_q);
    delete(m_context);

    return 0;
}

// This version of compression does overlapped execution between
// Kernel and Host. I/O operations between Host and Device are
// overlapped with Kernel execution between multiple compute units
uint32_t xil_lzma::compress(uint8_t *in,
                           uint8_t *out,
                           uint32_t input_size,
                           uint32_t host_buffer_size,
                           std::ofstream *ofs,
                           int cu_run
                          ) {
    uint32_t block_size_in_bytes = m_block_size_in_kb * 1024;
    uint32_t overlap_buf_count = OVERLAP_BUF_COUNT;
    uint64_t total_kernel_time = 0;
#ifdef EVENT_PROFILE
    uint64_t total_write_time = 0;
    uint64_t total_read_time = 0;
#endif
    //Assignment to the buffer extensions
    buffer_extension_assignments(cu_run);
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
    for (int cu = 0; cu < MAX_COMPUTE_UNITS;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            // Input:- This buffer contains input chunk data
            buffer_input[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                host_buffer_size,
                                                &(inExt[cu][flag]));
            // Output:- This buffer contains compressed data written by device
            buffer_output[cu][flag] = new cl::Buffer(*m_context, 
                                                CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                               	(uint32_t)HOST_BUFFER_SIZE,//host_buffer_size,
                                                &(outExt[cu][flag]));
            // Ouput:- This buffer contains compressed block sizes
            buffer_compressed_size[cu][flag] = new cl::Buffer(*m_context, 
                                                        CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                        temp_nblocks * sizeof(uint32_t),
                                                        &(csExt[cu][flag]));
            // Input:- This buffer contains origianl input block sizes
            buffer_block_size[cu][flag] = new cl::Buffer(*m_context, 
                                                     CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                     temp_nblocks * sizeof(uint32_t),
                                                     &(bsExt[cu][flag]));
        }

        dict_buffer1[cu] = new cl::Buffer(*m_context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX, MEM_ALLOC_CPU*sizeof(uint8_t),&(dbuf[cu][0]));
        dict_buffer2[cu] = new cl::Buffer(*m_context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX, 1*1024*1024*sizeof(uint8_t),&(dbuf[cu][1]));
        dict_buffer3[cu] = new cl::Buffer(*m_context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR | CL_MEM_EXT_PTR_XILINX, 1*1024*1024*sizeof(uint8_t),&(dbuf[cu][2]));
    }

    //m_q->enqueueMigrateMemObjects({*(dict_buffer1[0])}, 0, NULL, NULL);  // migrate only for 1CU
    m_q->flush();
    m_q->finish();
    // Counter which helps in tracking
    // Output buffer index    
    uint64_t outIdx = 0;
    uint64_t outvalue = 0;

    // Track the lags of respective chunks for left over handling
    int chunk_flags[total_chunks];
    int cu_order[total_chunks];
    
    // Finished bricks
    int completed_bricks = 0;

    int flag = 0; 
    int lcl_cu = 0;

    uint32_t value32 = 0;
    uint64_t value64 = 0;
    CRC crc;
    initcrc();
//-------START------------
    RECORD_LIST *record_list = new RECORD_LIST[((input_size-1)/block_size_in_bytes) +1];
    size_t uncompressed_size;
    uint8_t control;
    size_t h_compressed_size;
    uint8_t block_header[] = {0x02, 0x00, 0x21, 0x01, 0x24, 0x00, 0x00, 0x00};
    uint8_t block_header_size = 8;
    uint8_t *str;
    int lzblock = 0;
    int unlzblock = 0;

    //int lc = 3;
    //int pb = 2;
    //int lp = 0;
    uint8_t lclppb = 0x5D;

    uint8_t magic[] = {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00};
    uint8_t magic_size = 6;
    fileWrite(ofs,magic,magic_size,out,outvalue);
    outvalue+=magic_size;
    uint8_t stream_flag[] = {0x00, 0x04};
    uint8_t stream_flag_size = 2;
    fileWrite(ofs,stream_flag,stream_flag_size,out,outvalue);
    outvalue+=stream_flag_size;
    value32 = crc32(stream_flag, stream_flag_size, 0);
    crc.buffer.u32[0] = integer_le_32(value32);
    fileWrite(ofs,crc.buffer.u8,4,out,outvalue);
    outvalue+=4;
    fileWrite(ofs,block_header,block_header_size,out,outvalue);
    outvalue+=block_header_size;
    value32 = crc32(block_header, block_header_size, 0);
    crc.buffer.u32[0] = integer_le_32(value32);
    fileWrite(ofs,crc.buffer.u8,4,out,outvalue);
    outvalue+=4;
//-------END------------

    // Main loop of overlap execution
    // Loop below runs over total bricks i.e., host buffer size chunks
    auto total_start = std::chrono::high_resolution_clock::now();
    for (uint32_t brick = 0, itr = 0; brick < total_chunks; brick+=MAX_COMPUTE_UNITS, itr++, flag =!flag) {
        lcl_cu = MAX_COMPUTE_UNITS;
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
                int brick_flag_idx = brick - (MAX_COMPUTE_UNITS * overlap_buf_count - cu);
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
//-------START------------
                    str = &in[brick_flag_idx * host_buffer_size + index];
                    uint8_t *comdata = &(h_buf_out[cu][flag].data()[bIdx * block_size_in_bytes]);
                    uncompressed_size = block_size;
                    if(compressed_size < uncompressed_size) {
                        if((lzblock-unlzblock) == 0)
                            if(lzblock == 0)
                                control = 0xE0;
                            else
                                control = 0xC0;
                        else
                            control = 0xA0;
                        control |= ((uncompressed_size-1) >> 16);
                        fileWrite(ofs, control,out,outvalue);
                        outvalue++;
                        fileWrite(ofs, (uncompressed_size-1)>> 8,out,outvalue);
                        outvalue++;
                        fileWrite(ofs, uncompressed_size-1,out,outvalue);
                        outvalue++;
                        record_list[lzblock].uncompressed_size = uncompressed_size;
                        h_compressed_size = compressed_size;
                        fileWrite(ofs, (h_compressed_size-1)>> 8,out,outvalue);
                        outvalue++;
                        fileWrite(ofs, h_compressed_size-1,out,outvalue);
                        outvalue++;
                        record_list[lzblock].h_compressed_size = h_compressed_size;
                        outIdx += h_compressed_size;
                        lzblock++;
                        if((lzblock-unlzblock) == 1) {
                            fileWrite(ofs, lclppb,out,outvalue);
                            outvalue++;
                        }
                        fileWrite(ofs,comdata,h_compressed_size,out,outvalue);
                        outvalue+=h_compressed_size;
                        value64 = crc64(str, uncompressed_size, value64);
                    } else {
                        compressed_size = uncompressed_size;
                        comdata = str;
                        if(lzblock == 0)
                            control = 0x01;
                        else
                            control = 0x02;
                        fileWrite(ofs, control,out,outvalue);
                        outvalue++;
                        record_list[lzblock].uncompressed_size = uncompressed_size;
                        h_compressed_size = compressed_size;
                        fileWrite(ofs, (h_compressed_size-1)>> 8,out,outvalue);
                        outvalue++;
                        fileWrite(ofs, h_compressed_size-1,out,outvalue);
                        outvalue++;
                        record_list[lzblock].h_compressed_size = h_compressed_size;
                        outIdx += h_compressed_size;
                        lzblock++;
                        unlzblock++;
                        fileWrite(ofs,comdata,h_compressed_size,out,outvalue);
                        outvalue+=h_compressed_size;
                        value64 = crc64(str, uncompressed_size, value64);
                }
//-------END------------
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
            compress_kernel_lzma[cu_run]->setArg(narg++, *(buffer_input[cu][flag]));
            compress_kernel_lzma[cu_run]->setArg(narg++, *(buffer_output[cu][flag]));
            compress_kernel_lzma[cu_run]->setArg(narg++, *(dict_buffer1[cu]));
            //compress_kernel_lzma[cu_run]->setArg(narg++, *(dict_buffer2[cu]));
            //compress_kernel_lzma[cu_run]->setArg(narg++, *(dict_buffer3[cu]));
            compress_kernel_lzma[cu_run]->setArg(narg++, *(buffer_compressed_size[cu][flag]));
            compress_kernel_lzma[cu_run]->setArg(narg++, *(buffer_block_size[cu][flag]));
            compress_kernel_lzma[cu_run]->setArg(narg++, m_block_size_in_kb);
            compress_kernel_lzma[cu_run]->setArg(narg++, sizeOfChunk[brick + cu]);
            compress_kernel_lzma[cu_run]->setArg(narg++, brick*host_buffer_size);
            // Transfer data from host to device
            m_q->enqueueMigrateMemObjects({*(buffer_input[cu][flag]), *(buffer_block_size[cu][flag])}, 0, NULL, &(write_events[cu][flag]));
            // Kernel wait events for writing & compute
            std::vector<cl::Event> kernelWriteWait;
            std::vector<cl::Event> kernelComputeWait;

            // Kernel Write events update
            kernelWriteWait.push_back(write_events[cu][flag]);
            // Fire the kernel
            m_q->enqueueTask(*compress_kernel_lzma[cu_run], &kernelWriteWait, &(kernel_events[cu][flag]));
            // Update kernel events flag on computation
            kernelComputeWait.push_back(kernel_events[cu][flag]);
            // Transfer data from device to host
            m_q->enqueueMigrateMemObjects({*(buffer_output[cu][flag]), *(buffer_compressed_size[cu][flag])}, 
                                            CL_MIGRATE_MEM_OBJECT_HOST, &kernelComputeWait, &(read_events[cu][flag]));
            //cl::Event event_host_read;
        } // Compute unit loop ends here
    } // Main loop ends here
    m_q->flush();
    m_q->finish();
    int leftover = total_chunks - completed_bricks;
    int stride = 0;
    if ((total_chunks < overlap_buf_count * MAX_COMPUTE_UNITS))
        stride = overlap_buf_count * MAX_COMPUTE_UNITS;
    else 
        stride = total_chunks;
    // Handle leftover bricks

    for (int ovr_itr = 0, brick = stride - overlap_buf_count * MAX_COMPUTE_UNITS; ovr_itr < leftover; ovr_itr+=MAX_COMPUTE_UNITS, brick+=MAX_COMPUTE_UNITS) {
        lcl_cu = MAX_COMPUTE_UNITS;
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
                str = &in[brick_flag_idx * host_buffer_size + index];
                uint8_t *comdata = &(h_buf_out[cu][flag].data()[bIdx * block_size_in_bytes]);
                uncompressed_size = block_size;
                if(compressed_size < uncompressed_size) {
                    if((lzblock-unlzblock) == 0)
                        if(lzblock == 0)
                            control = 0xE0;
                        else
                            control = 0xC0;
                    else
                        control = 0xA0;
                    control |= ((uncompressed_size-1) >> 16); // = (control & 0x1F) << 16;
                    fileWrite(ofs, control,out,outvalue);
                    outvalue++;
                    fileWrite(ofs, (uncompressed_size-1)>> 8,out,outvalue);
                    outvalue++;
                    fileWrite(ofs, uncompressed_size-1,out,outvalue);
                    outvalue++;
                    record_list[lzblock].uncompressed_size = uncompressed_size;
                    h_compressed_size = compressed_size;
                    fileWrite(ofs, (h_compressed_size-1)>> 8,out,outvalue);
                    outvalue++;
                    fileWrite(ofs, h_compressed_size-1,out,outvalue);
                    outvalue++;
                    record_list[lzblock].h_compressed_size = h_compressed_size;
                    outIdx += h_compressed_size;
                    lzblock++;
                    if((lzblock-unlzblock) == 1) {
                        fileWrite(ofs, lclppb,out,outvalue);
                        outvalue++;
                    }
                    fileWrite(ofs,comdata,h_compressed_size,out,outvalue);
                    outvalue+=h_compressed_size;
                    value64 = crc64(str, uncompressed_size, value64);
                } else {
                    compressed_size = uncompressed_size;
                    comdata = str;
                    if(lzblock == 0)
                        control = 0x01;
                    else
                        control = 0x02;
                    fileWrite(ofs, control,out,outvalue);
                    outvalue++;
                    record_list[lzblock].uncompressed_size = uncompressed_size;
                    h_compressed_size = compressed_size;
                    fileWrite(ofs, (h_compressed_size-1)>> 8,out,outvalue);
                    outvalue++;
                    fileWrite(ofs, h_compressed_size-1,out,outvalue);
                    outvalue++;
                    record_list[lzblock].h_compressed_size = h_compressed_size;
                    outIdx += h_compressed_size;
                    lzblock++;
                    unlzblock++;
                    fileWrite(ofs,comdata,h_compressed_size,out,outvalue);
                    outvalue+=h_compressed_size;
                    value64 = crc64(str, uncompressed_size, value64);
                }
            } // For loop ends
        } // cu loop ends here
    } // Main loop ends here
    
//---------START-----------------
    fileWrite(ofs, 0x00,out,outvalue); //end of stream
    outvalue++;
    //padding
    uint32_t paddingsize = outIdx + (lzblock-unlzblock)*5 + unlzblock*3 + 1 +  ((lzblock-unlzblock) > 0 ? 1:0);
    while (paddingsize & 3) {
        fileWrite(ofs, 0x00,out,outvalue);
        outvalue++;
        ++paddingsize;
    }
    crc.buffer.u64[0] = integer_le_64(value64);
    fileWrite(ofs,crc.buffer.u8,8,out,outvalue);
    outvalue+=8;

    std::vector<uint8_t> index;
    //uint8_t index_size = 0;
    uint8_t records = 1;
    uint8_t record_buf[9];
    size_t record_size = encode(record_buf,records);
    index.push_back(0x00);	//index_indicator
    for(size_t k =0;k<record_size;++k) {
    	index.push_back(record_buf[k]);
    }	
    uint8_t unpadded_size_buf[9];
    uint8_t uncompressed_size_buf[9];
    //uint8_t block_header_crc_size = 4;
    //uint8_t data_crc_size = 8;
    size_t unpadded_size;	
    size_t uncompressed_size_buf_size;
    uint32_t unpadded = outIdx + (lzblock-unlzblock)*5 + unlzblock*3 + 1 + ((lzblock-unlzblock) > 0 ? 1:0) + 12 + 8;
   
    unpadded_size = encode(unpadded_size_buf,unpadded);
    uncompressed_size_buf_size = encode(uncompressed_size_buf,input_size);

    for(size_t k =0;k<unpadded_size;++k) 
        index.push_back(unpadded_size_buf[k]);

    for(size_t k =0;k<uncompressed_size_buf_size;++k)
        index.push_back(uncompressed_size_buf[k]);

    paddingsize = index.size();
    while (paddingsize & 3) {
        index.push_back(0x00);
        ++paddingsize;
    }

    fileWrite(ofs,index.data(),index.size(),out,outvalue);
    outvalue+=index.size();
    value32 = crc32((uint8_t*)index.data(), index.size(), 0);
    crc.buffer.u32[0] = integer_le_32(value32);
    fileWrite(ofs,crc.buffer.u8,4,out,outvalue);
    outvalue+=4;
    uint32_t backward_size = index.size() + 4 ; //index+crc
    backward_size = backward_size/4;
    backward_size = backward_size - 1;
    uint8_t footer[6];
    footer[0] = backward_size;
    footer[1] = backward_size>>8;
    footer[2] = backward_size>>16;
    footer[3] = backward_size>>24;
    footer[4] = stream_flag[0];
    footer[5] = stream_flag[1];

    uint8_t footer_size = 6;
    value32 = crc32(footer, footer_size, 0);
    crc.buffer.u32[0] = integer_le_32(value32);
    fileWrite(ofs,crc.buffer.u8,4,out,outvalue);
    outvalue+=4;
    fileWrite(ofs,backward_size,out,outvalue);
    outvalue++;
    fileWrite(ofs,backward_size>>8,out,outvalue);
    outvalue++;
    fileWrite(ofs,backward_size>>16,out,outvalue);
    outvalue++;
    fileWrite(ofs,backward_size>>24,out,outvalue);
    outvalue++;
    fileWrite(ofs,stream_flag,stream_flag_size,out,outvalue);
    outvalue+=stream_flag_size;
    fileWrite(ofs,0x59,out,outvalue);
    outvalue++;
    fileWrite(ofs,0x5A,out,outvalue);
    outvalue++;
//---------END-----------------------
    auto total_end = std::chrono::high_resolution_clock::now();   
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    float throughput_in_mbps_1 = (float)input_size * 1000 / total_time_ns.count();
    float kernel_throughput_in_mbps_1 = (float)input_size * 1000 / total_kernel_time;
#ifdef EVENT_PROFILE

    std::cout << "total_time_ms: " << std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count() << std::endl;	
    std::cout << "input_size " << input_size << std::endl;
    std::cout << "Total Kernel Time " << total_kernel_time << std::endl;
    std::cout << "Total Write Time " << total_write_time << std::endl;
    std::cout << "Total Read Time " << total_read_time << std::endl;
#endif    
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1 << "\t";
    std::cout << std::fixed << std::setprecision(2) << kernel_throughput_in_mbps_1;
      
	//delete[] record_list;     
    for (int cu = 0; cu < MAX_COMPUTE_UNITS;cu++) {
        for (uint32_t flag = 0; flag < overlap_buf_count;flag++) {
            delete (buffer_input[cu][flag]);
            delete (buffer_output[cu][flag]);
            delete (buffer_compressed_size[cu][flag]);
            delete (buffer_block_size[cu][flag]);
        }
        delete (dict_buffer1[cu]);
        delete (dict_buffer2[cu]);
        delete (dict_buffer3[cu]);
    }
    delete[] record_list;
    return outvalue; 
} // Overlap end
/*
// Note: Various block sizes supported by LZMA standard are not applicable to
// this function. It just supports Block Size 64KB
uint32_t xil_lzma::compress_sequential(uint8_t *in, 
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
            compress_kernel_lzma[argset]->setArg(narg++, *(buffer_input[argset][0]));
            compress_kernel_lzma[argset]->setArg(narg++, *(buffer_output[argset][0]));
            compress_kernel_lzma[argset]->setArg(narg++, *(buffer_compressed_size[argset][0]));
            compress_kernel_lzma[argset]->setArg(narg++, *(buffer_block_size[argset][0]));
            compress_kernel_lzma[argset]->setArg(narg++, block_size_in_kb);
            compress_kernel_lzma[argset]->setArg(narg++, hostChunk_cu[argset]);
        }
        std::vector<cl::Memory> inBufVec;
    
        for (int inbvec = 0; inbvec < compute_cu; inbvec++) {
            inBufVec.push_back(*(buffer_input[inbvec][0]));
            inBufVec.push_back(*(buffer_block_size[inbvec][0]));
        }
        
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0);// 0 means from host
        m_q->finish();
        
        // Measure kernel execution time
        auto kernel_start = std::chrono::high_resolution_clock::now(); 

        // Fire kernel execution
        for (int ker = 0; ker < compute_cu; ker++)
            m_q->enqueueTask(*compress_kernel_lzma[ker]);
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
*/
