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
#define FORMAT_0 31
#define FORMAT_1 139
#define VARIANT 8
#define REAL_CODE 8 
#define OPCODE 3

extern unsigned long crc;
#define CHUNK_16K 16384

int batch_buf_release = 0;

void zip(std::string & inFile_name, std::ofstream & outFile, uint8_t *zip_out, uint32_t enbytes) {

    // 2 bytes of magic header
    outFile.put(FORMAT_0);
    outFile.put(FORMAT_1);
   
    // 1 byte Compression method
    outFile.put(VARIANT);
    
    // 1 byte flags
    uint8_t flags = 0;
    flags |= REAL_CODE;
    outFile.put(flags);

    // 4 bytes file modification time in unit format
    unsigned long time_stamp = 0;
    struct stat istat;
    stat(inFile_name.c_str(), &istat);
    time_stamp = istat.st_mtime;
    //put_long(time_stamp, outFile);
    uint8_t time_byte = 0;
    time_byte = time_stamp;
    outFile.put(time_byte);
    time_byte = time_stamp >> 8;
    outFile.put(time_byte);
    time_byte = time_stamp >> 16;
    outFile.put(time_byte);
    time_byte = time_stamp >> 24;
    outFile.put(time_byte);

    // 1 byte extra flag (depend on compression method)
    uint8_t deflate_flags = 0;
    outFile.put(deflate_flags);

    // 1 byte OPCODE - 0x03 for Unix
    outFile.put(OPCODE);
    
    // Dump file name
    for(int i = 0; inFile_name[i] != '\0'; i++){
        outFile.put(inFile_name[i]); 
    }
    outFile.put(0);

    outFile.write((char *)zip_out, enbytes);    
    
    unsigned long ifile_size = istat.st_size;
    uint8_t crc_byte = 0;
    long crc_val = 0;
    crc_byte = crc_val;
    outFile.put(crc_byte);
    crc_byte = crc_val >> 8;
    outFile.put(crc_byte);
    crc_byte = crc_val >> 16;
    outFile.put(crc_byte);
    crc_byte = crc_val >> 24;
    outFile.put(crc_byte);
    
    uint8_t len_byte = 0;
    len_byte = ifile_size;
    outFile.put(len_byte);
    len_byte = ifile_size >> 8;
    outFile.put(len_byte);
    len_byte = ifile_size >> 16;
    outFile.put(len_byte);
    len_byte = ifile_size >> 24;
    outFile.put(len_byte);
}

void xil_gzip::compress_file(std::vector<std::string> & inFile_batch,
                             std::vector<std::string> & outFile_batch,  
                             std::vector<int> & enbytes,
                             int chunk_size
                            ) {
    // Input file size holder
    std::vector<int> input_size_cntr; 

    // Allocate buffer pointers based on chunk size
    // These buffer pointers hold data related to 
    // Each input file 
    uint8_t *gzip_in[chunk_size];
    uint8_t *gzip_out[chunk_size];

    // Allocate actual aligned input and output buffers
    std::vector<uint8_t, aligned_allocator<uint8_t>> input_vectors[chunk_size]; 
    std::vector<uint8_t, aligned_allocator<uint8_t>> output_vectors[chunk_size]; 

    uint32_t local_enbytes[chunk_size];
    
    // Find out input sizes  
    for(int i = 0; i < chunk_size; i++) {
        
        std::ifstream inFile(inFile_batch[i].c_str(), std::ifstream::binary);
        if(!inFile) {
            std::cout << "Unable to open file" << std::endl;
            exit(1);
        }
        
        inFile.seekg(0, inFile.end);
        long input_size = inFile.tellg();
        inFile.seekg(0, inFile.beg);
        input_size_cntr.push_back(input_size);
   
        input_vectors[i].reserve(input_size_cntr[i]);
        output_vectors[i].reserve(2 * input_size_cntr[i]);
        inFile.read((char *)input_vectors[i].data(), input_size_cntr[i]);

        // Assign data to buffer pointers
        gzip_in[i]  = input_vectors[i].data();
        gzip_out[i] = output_vectors[i].data();
        inFile.close();
    }

    // GZip batch mode compress
    compress(gzip_in, gzip_out, input_size_cntr, local_enbytes);

    
    // Fill the .gz buffer with encoded stream
    for(int i = 0; i < chunk_size; i++) {
        
        enbytes.push_back(local_enbytes[i]);

        std::ofstream outFile(outFile_batch[i].c_str(), std::ofstream::binary);
    
        // Dump compressed bytes to .gz file
        zip(inFile_batch[i], outFile, output_vectors[i].data(), local_enbytes[i]);        
        outFile.close();
    }   
}


uint32_t xil_gzip::compress_file(std::string & inFile_name, std::string & outFile_name) 
{
    std::ifstream inFile(inFile_name.c_str(), std::ifstream::binary);
    std::ofstream outFile(outFile_name.c_str(), std::ofstream::binary);
    
    if(!inFile) {
        std::cout << "Unable to open file";
        exit(1);
    }

    inFile.seekg(0, inFile.end);
    long input_size = inFile.tellg();
    inFile.seekg(0, inFile.beg);   
    
    std::vector<uint8_t,aligned_allocator<uint8_t>> gzip_in (input_size);
    std::vector<uint8_t,aligned_allocator<uint8_t>> gzip_out(input_size * 2);
    
    inFile.read((char *)gzip_in.data(), input_size); 

    // GZip Compress 
    uint32_t enbytes = compress(gzip_in.data(), gzip_out.data(), input_size);

    // Pack GZip encoded stream .gz file
    zip(inFile_name, outFile, gzip_out.data(), enbytes);

    // Close file 
    inFile.close();
    outFile.close();
    return enbytes;
}

int validate(std::string & inFile_name, std::string & outFile_name) {


#ifdef VERBOSE 
   orig< "Validation     -- ";
#endif   

    std::string command = "cmp " + inFile_name + " " + outFile_name;
    int ret = system(command.c_str());
    return ret;
}


int xil_gzip::init(const std::string& binaryFileName)
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
    local_out = new std::vector<uint8_t,aligned_allocator<uint8_t>>(2*MAX_INPUT_SIZE);
    long round_off = MAX_INPUT_SIZE/ VEC;
    round_off = round_off * VEC;
    long size_for_each_unit = round_off;
    int inputSizeInKB = (size_for_each_unit-1) / BLOCK_PARITION + 1;
    sizeOut = new std::vector<uint32_t,aligned_allocator<uint32_t>>(2*inputSizeInKB);

    // Batch mode buffer allocate
    for(int i = 0; i < 2; i++) {
        batch_sizeOut[i] = new std::vector<uint32_t, aligned_allocator<uint32_t>>(2 * inputSizeInKB);
        batch_local_out[i] = new std::vector<uint8_t, aligned_allocator<uint8_t>>(2 * MAX_INPUT_SIZE);
    }
    
    // Various Kernel Names
    std::string kernel_names = "gZip_cu1";
    
    // Create kernels based on compute unit count
    kernel_gzip = new cl::Kernel(*m_program, (kernel_names.c_str()));    

    return 0;
}

int xil_gzip::release()
{
    delete(m_q);
    delete(m_program);
    delete(m_context);
    delete(local_out);
    delete(sizeOut);
    
    delete(kernel_gzip);    

    // Release local and sizeOut buffers
    for(int i = 0; i < batch_buf_release; i++) {
        delete (batch_sizeOut[i]);
        delete (batch_local_out[i]);
    }

    // Release cl buffers
    for(int i = 0; i < batch_buf_release; i++) {
        delete (batch_buffer_input[i]);
        delete (batch_buffer_output[i]);
        delete (batch_buffer_size[i]); 
    }

    return 0;
}

cl_mem_ext_ptr_t xil_gzip::get_buffer_extension(int ddr_no)
{
    cl_mem_ext_ptr_t ext;
    switch(ddr_no){
        case 0: 
            ext.flags = XCL_MEM_DDR_BANK0; break;
        case 1:
            ext.flags = XCL_MEM_DDR_BANK1; break;
        case 2:
            ext.flags = XCL_MEM_DDR_BANK2; break;
        default:
            ext.flags = XCL_MEM_DDR_BANK3; 
    };
    return ext;
}

// GZip Compress (Batch of Files) using -l <file.list> -b <size of batch>
void xil_gzip::compress(uint8_t *in[],
                        uint8_t *out[],
                        std::vector<int> & input_size,
                        uint32_t *file_encode_bytes 
                       ) {
    auto total_start = std::chrono::high_resolution_clock::now();

    // Figure out the total files sent in batch mode
    int total_file_count = input_size.size();

    cl_mem_ext_ptr_t in_ext[2];
    cl_mem_ext_ptr_t out_ext[2];
    cl_mem_ext_ptr_t sizeExt[2];

    // Read, Write, Kernel Events
    cl::Event kernel_events[2];
    cl::Event read_events[2];
    cl::Event write_events[2];

    std::vector<cl::Event> kernelWriteWait;
    std::vector<cl::Event> kernelComputeWait;

    bool file_flags[total_file_count];

    // Main loop for overlap computation
    for(int file = 0; file < total_file_count; file++) {

        long insize = (long) input_size[file];

        int flag = file % 2;
        
        file_flags[file] = flag;

        if(file >= 2) {
        
            read_events[flag].wait();
            uint8_t *dst = out[file - 2];
            size_t out_cntr = (batch_sizeOut[flag]->data())[0];
            uint8_t *src = batch_local_out[flag]->data();
            file_encode_bytes[file - 2] = out_cntr;
            std::memcpy(dst, src, out_cntr);

            delete (batch_buffer_input[flag]);
            delete (batch_buffer_output[flag]);
            delete (batch_buffer_size[flag]);
        }

        long size_per_unit = (input_size[file] - 1) + 1;
        long size_of_each_unit_4k = ((size_per_unit - 1) / 4096 + 1) * 4096;
        int inputSizeInKB = (size_of_each_unit_4k - 1) / BLOCK_PARITION + 1;
        long inbuf_size = input_size[file] * sizeof(uint8_t);
        long outbuf_size = 2 * input_size[file] * sizeof(uint8_t);

        uint8_t *in_ptr = in[file];
        in_ext[flag].flags = XCL_MEM_DDR_BANK0;
        in_ext[flag].obj = &in_ptr[0];
        in_ext[flag].param = 0;

        out_ext[flag].flags = XCL_MEM_DDR_BANK1;
        out_ext[flag].obj   = batch_local_out[flag]->data();
        out_ext[flag].param = 0;

        sizeExt[flag].flags = XCL_MEM_DDR_BANK1;
        sizeExt[flag].obj = batch_sizeOut[flag]->data();

        // Device buffer allocation
        batch_buffer_input[flag]     = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  
                inbuf_size, &in_ext[flag]);
        
        batch_buffer_output[flag]    = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
                outbuf_size,&out_ext[flag]);
        
        batch_buffer_size[flag]      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
                sizeof(uint32_t) * inputSizeInKB , &sizeExt[flag]);

        // Vector of memory objects
        std::vector<cl::Memory> inBufVec;
        inBufVec.push_back(*(batch_buffer_input[flag]));

        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,
                                      0,/* 0 means from host*/
                                      NULL,
                                      &write_events[flag]
                                      );
        
        // Set kernel arguments
        int narg = 0;
        (kernel_gzip)->setArg(narg++, *(batch_buffer_input[flag]));
        (kernel_gzip)->setArg(narg++, *(batch_buffer_output[flag]));
        (kernel_gzip)->setArg(narg++, *(batch_buffer_size[flag]));
        (kernel_gzip)->setArg(narg++, insize);
        
        kernelWriteWait.push_back(write_events[flag]);        
        
        // Kernel invocation
        m_q->enqueueTask(*kernel_gzip, &kernelWriteWait, &kernel_events[flag]);
        
        kernelComputeWait.push_back(kernel_events[flag]); 
        
        // Migrate Memeory - Map device to host buffers
        std::vector<cl::Memory> outBufVec;
        outBufVec.push_back(*(batch_buffer_output[flag]));
        outBufVec.push_back(*(batch_buffer_size[flag]));
        
        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec, CL_MIGRATE_MEM_OBJECT_HOST, &kernelComputeWait, &read_events[flag]);

    } // End of mail loop - Batch mode
    m_q->flush();
    m_q->finish();

    int temp = total_file_count;
    if(total_file_count == 1) {
        temp = 2;
        batch_buf_release = 1;
    }
    else {
        batch_buf_release = 2;
    }

    int cntr = 0;
    for(int i = temp - 2; i < total_file_count; i++) {
        cntr = file_flags[i];
        uint8_t *dst = out[i];
        size_t out_cntr = (batch_sizeOut[cntr]->data())[0];
        uint8_t *src = batch_local_out[cntr]->data();
        file_encode_bytes[i] = out_cntr;
        std::memcpy(dst, src, out_cntr);
    }
    auto total_end = std::chrono::high_resolution_clock::now(); 
    
    long sum_size = 0;
    for(unsigned int i = 0; i < input_size.size(); i++) 
        sum_size += input_size[i];
    
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    float total_in_mbps = (float)sum_size * 1000 / total_time_ns.count();
    printf("%d", total_file_count);
    printf("\t\t");
    printf("%.2f",total_in_mbps);
    printf("\n");
}

// GZip compress (Single File) using -i option
uint32_t xil_gzip::compress(uint8_t *in, 
                            uint8_t *out, 
                            long input_size
                          ) {

    auto total_start = std::chrono::high_resolution_clock::now(); 
    long size_per_unit = (input_size - 1)+ 1;
    long size_of_each_unit_4k = ((size_per_unit -1)/4096 + 1) * 4096;
    int inputSizeInKB = (size_of_each_unit_4k-1) / BLOCK_PARITION+ 1;
    
    // Device buffers
    cl::Buffer* buffer_input;
    cl::Buffer* buffer_output;
    cl::Buffer* buffer_size;
    
    auto alloc_end = std::chrono::high_resolution_clock::now(); 
    
    long size_for_each_unit =  size_of_each_unit_4k;
    if (size_for_each_unit > input_size){
        size_for_each_unit = input_size;
    }
   
    long inbuf_start    = 0;
    long outbuf_start   = 0;
    long inbuf_size     = size_for_each_unit * sizeof(uint8_t);
    long outbuf_size    = 2*size_for_each_unit * sizeof(uint8_t);

    // OpenCL device buffer extension mapping   
    cl_mem_ext_ptr_t    inExt,outExt,sizeExt;
    inExt   = get_buffer_extension(0); inExt.obj    = &in[inbuf_start];
    outExt  = get_buffer_extension(1); outExt.obj   = local_out->data() + outbuf_start;
    sizeExt = get_buffer_extension(1); sizeExt.obj  = sizeOut->data();
    
    // Device buffer allocation
    buffer_input     = new cl::Buffer(*m_context, 
        CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  
            inbuf_size, &inExt);

    buffer_output    = new cl::Buffer(*m_context, 
            CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
            outbuf_size,&outExt);

    buffer_size      = new cl::Buffer(*m_context, 
            CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
            sizeof(uint32_t) * inputSizeInKB , &sizeExt);
    auto dw_s = std::chrono::high_resolution_clock::now(); 
    
    
    size_for_each_unit =  size_of_each_unit_4k;
    if (size_for_each_unit > input_size){
        size_for_each_unit = input_size;
    }
    
    // Set kernel arguments
    int narg = 0;
    (kernel_gzip)->setArg(narg++, *(buffer_input));
    (kernel_gzip)->setArg(narg++, *(buffer_output));
    (kernel_gzip)->setArg(narg++, *(buffer_size));
    (kernel_gzip)->setArg(narg++, size_for_each_unit);
    std::vector<cl::Memory> inBufVec;
    inBufVec.push_back(*(buffer_input));

    // Migrate memory - Map host to device buffers   
    m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
    m_q->finish();
    
    auto kernel_start = std::chrono::high_resolution_clock::now(); 
    
    // Kernel invocation
    m_q->enqueueTask(*kernel_gzip);
    m_q->finish();
    auto kernel_end = std::chrono::high_resolution_clock::now();    
    
    std::vector<cl::Memory> outBufVec;
    outBufVec.push_back(*(buffer_output));
    outBufVec.push_back(*(buffer_size));
    
    // Migrate memory - Map device to host buffers
    m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    m_q->finish();
    
    auto dr_end = std::chrono::high_resolution_clock::now();    
    uint32_t out_cntr = (sizeOut->data())[0];
    std::memcpy(&out[0],local_out->data(),out_cntr);
    auto mcpy_end = std::chrono::high_resolution_clock::now();    

    auto left_over_end = std::chrono::high_resolution_clock::now();    

    delete (buffer_input);
    delete (buffer_output);
    delete (buffer_size);
    
    auto total_end = std::chrono::high_resolution_clock::now(); 

    auto alloc_time = std::chrono::duration<double, std::nano>(alloc_end- total_start);
    auto buffer_create_time = std::chrono::duration<double, std::nano>(dw_s -alloc_end);
    auto buffer_write_time = std::chrono::duration<double, std::nano>(kernel_start - dw_s);
    auto kernel_time_ns_1 = std::chrono::duration<double, std::nano>(kernel_end - kernel_start);
    auto buffer_read_time = std::chrono::duration<double, std::nano>(dr_end - kernel_end);
    auto memcpy_time = std::chrono::duration<double, std::nano>(mcpy_end - dr_end );
    auto leftover_time = std::chrono::duration<double, std::nano>(left_over_end - mcpy_end);
    auto buffer_delete_time = std::chrono::duration<double, std::nano>(total_end - left_over_end);
    auto total_time_ns = std::chrono::duration<double, std::nano>(total_end - total_start);
    auto accumulate_time = alloc_time + buffer_create_time + buffer_write_time + kernel_time_ns_1 + buffer_read_time + memcpy_time + leftover_time + buffer_delete_time;
#if 0
    float alloc_througput   = (float)input_size * 1000 / alloc_time.count();
    float buffer_througput  = (float)input_size * 1000 / buffer_create_time.count();
    float buffer_write      = (float)input_size * 1000 / buffer_write_time.count();
    float kernel            = (float)input_size * 1000 / kernel_time_ns_1.count();
    float buffer_read       = (float)input_size * 1000 / buffer_read_time.count();
    float memcpy_throughput = (float)input_size * 1000 / memcpy_time.count();
    float leftover          = (float)input_size * 1000 / leftover_time.count();
    float buffer_delete     = (float)input_size * 1000 / buffer_delete_time.count();
    float total             = (float)input_size * 1000 / total_time_ns.count();
    float accumulate        = (float)input_size * 1000 / accumulate_time.count();
    std::cout 
        << "allocation="   << alloc_througput
        << "buffer_create="   << buffer_througput
        << " buffer_write="         << buffer_write
        << " kernel="               << kernel
        << " buffer_read="          << buffer_read
        << " memcpy="               << memcpy_throughput
        << " leftover="             << leftover 
        << " buffer_delete="        << buffer_delete
        << " accumulate="           << accumulate 
        << " total="                << total 
        << std::endl; 
#endif
    float throughput_in_mbps_1 = (float)input_size * 1000 / kernel_time_ns_1.count();
    //float total_in_mbps = (float)input_size * 1000 / total_time_ns.count();
    //printf("%.2f",total_in_mbps);
    //printf("%.2f",throughput_in_mbps_1 );
    std::cout << std::fixed << std::setprecision(2) << throughput_in_mbps_1;
    return out_cntr;
}

