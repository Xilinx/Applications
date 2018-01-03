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
ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE,EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#include "lz77.h"

int xil_lz77::init(const std::string& binaryFileName)
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
    long round_off = MAX_INPUT_SIZE/ (VEC * COMPUTE_UNITS);
    round_off = round_off * (VEC * COMPUTE_UNITS);
    long size_for_each_unit = round_off /COMPUTE_UNITS;
    int inputSizeInKB = (size_for_each_unit-1) / 1024 + 1;
    for(int i = 0 ; i < COMPUTE_UNITS ; i++){
        sizeOut[i] = new std::vector<uint32_t,aligned_allocator<uint32_t>>(2*inputSizeInKB);
    }
    return 0;
}

int xil_lz77::release()
{
    delete(m_q);
    delete(m_program);
    delete(m_context);
    delete(local_out);
    for(int i = 0 ; i < COMPUTE_UNITS ; i++){
        delete(sizeOut[i]);
    }
    return 0;
}

cl_mem_ext_ptr_t xil_lz77::get_buffer_extension(int ddr_no)
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
uint32_t xil_lz77::encode(uint8_t *in, 
                     uint8_t *out, 
                     long input_size
                    ) {

    auto total_start = std::chrono::high_resolution_clock::now(); 
    long size_per_unit = (input_size - 1) /COMPUTE_UNITS + 1;
    long size_of_each_unit_4k = ((size_per_unit -1)/4096 + 1) * 4096;
    int inputSizeInKB = (size_of_each_unit_4k-1) / 1024 + 1;
    
    // Device buffers
    cl::Buffer* buffer_input[COMPUTE_UNITS];
    cl::Buffer* buffer_output[COMPUTE_UNITS];
    cl::Buffer* buffer_size[COMPUTE_UNITS];
    
    auto alloc_end = std::chrono::high_resolution_clock::now(); 
    
    for(long size = 0 , i =0; size < input_size ; size+=size_of_each_unit_4k, i++){
        long size_for_each_unit =  size_of_each_unit_4k;
        if (size+size_for_each_unit > input_size){
            size_for_each_unit = input_size - size;
            size_for_each_unit = size_for_each_unit- input_size% VEC;
        }
       
        long inbuf_start    = i*size_of_each_unit_4k;
        long outbuf_start   = i*size_of_each_unit_4k*2;
        long inbuf_size     = size_for_each_unit * sizeof(uint8_t);
        long outbuf_size    = 2*size_for_each_unit * sizeof(uint8_t);
    
        // OpenCL device buffer extension mapping   
        cl_mem_ext_ptr_t    inExt,outExt,sizeExt;
        inExt   = get_buffer_extension(i); inExt.obj    = &in[inbuf_start];
        outExt  = get_buffer_extension(i); outExt.obj   = local_out->data() + outbuf_start;
        sizeExt = get_buffer_extension(i); sizeExt.obj  = sizeOut[i]->data();
        
        // Device buffer allocation
        buffer_input[i]     = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX,  
                inbuf_size, &inExt);

        buffer_output[i]    = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
                outbuf_size,&outExt);

        buffer_size[i]      = new cl::Buffer(*m_context, 
                CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX, 
                sizeof(uint32_t) * inputSizeInKB , &sizeExt);
    }
    auto dw_s = std::chrono::high_resolution_clock::now(); 
    
    // Kernel declaration based on compute units
    cl::Kernel* kernel_lz77[COMPUTE_UNITS];
    std::vector<std::string> kernel_names = {"gZip_cu1", "gZip_cu2", "gZip_cu3", "gZip_cu4"};
    
    for(long size = 0, i=0 ; size < input_size ; size+=size_of_each_unit_4k, i++){
        long size_for_each_unit =  size_of_each_unit_4k;
        if (size+size_for_each_unit > input_size){
            size_for_each_unit = input_size - size;
            size_for_each_unit = size_for_each_unit- input_size % VEC;
        }
        
        // Create kernels based on compute unit count
        kernel_lz77[i] = new  cl::Kernel(*m_program,(kernel_names[i]).c_str());
        
        // Set kernel arguments
        int narg = 0;
        (kernel_lz77[i])->setArg(narg++, *(buffer_input[i]));
        (kernel_lz77[i])->setArg(narg++, *(buffer_output[i]));
        (kernel_lz77[i])->setArg(narg++, *(buffer_size[i]));
        (kernel_lz77[i])->setArg(narg++, size_for_each_unit);
        std::vector<cl::Memory> inBufVec;
        inBufVec.push_back(*(buffer_input[i]));
    
        // Migrate memory - Map host to device buffers   
        m_q->enqueueMigrateMemObjects(inBufVec,0/* 0 means from host*/);
    }
    m_q->finish();
    auto kernel_start = std::chrono::high_resolution_clock::now(); 
    for(long size=0, i = 0 ; size < input_size ; size+=size_of_each_unit_4k, i++){
        // Kernel invocation
        m_q->enqueueTask(*kernel_lz77[i]);
    }
    m_q->finish();
    auto kernel_end = std::chrono::high_resolution_clock::now();    
    
    for(long size=0, i = 0 ; size < input_size ; size+=size_of_each_unit_4k, i++){
        std::vector<cl::Memory> outBufVec;
        outBufVec.push_back(*(buffer_output[i]));
        outBufVec.push_back(*(buffer_size[i]));
        // Migrate memory - Map device to host buffers
        m_q->enqueueMigrateMemObjects(outBufVec,CL_MIGRATE_MEM_OBJECT_HOST);
    }
    m_q->finish();
    auto dr_end = std::chrono::high_resolution_clock::now();    

    // Memory copy of multiple 1KB chunks into output buffer
    uint32_t out_cntr = 0;
    for(long size=0, i = 0 ; size < input_size ; size+=size_of_each_unit_4k, i++){
        long size_for_each_unit =  size_of_each_unit_4k;
        if (size+size_for_each_unit > input_size){
            size_for_each_unit = input_size - size;
            size_for_each_unit = size_for_each_unit- input_size%VEC;
        }
        for (int j = 0, idx=0 ; j < size_for_each_unit ; j+=1024,idx++){
            uint32_t next_cntr = (sizeOut[i]->data())[idx];
            std::memcpy(&out[out_cntr],local_out->data() + 2*i*size_of_each_unit_4k + 2*j,next_cntr);
            out_cntr += next_cntr;
        }
    }
    auto mcpy_end = std::chrono::high_resolution_clock::now();    
    
    // Copy left over data if input file to LZ77 fpga encoded buffer
    int left_over = input_size%VEC;
    for (long i = input_size-left_over; i < input_size ; i++){
        uint8_t ch = in[i];
        if (ch == 255){
            out[out_cntr++] = ch;
            out[out_cntr++] = 0;
            out[out_cntr++] = 0;
        }else{
            out[out_cntr++] = ch;
        }
    }
    auto left_over_end = std::chrono::high_resolution_clock::now();    
    for(long size=0, i = 0 ; size < input_size ; size+=size_of_each_unit_4k, i++){
        delete (buffer_input[i]);
        delete (buffer_output[i]);
        delete (buffer_size[i]);
        delete (kernel_lz77[i]);
    }
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
    float total_in_mbps = (float)input_size * 1000 / total_time_ns.count();
    printf("%.2f",total_in_mbps);
    printf("\t\t%.2f",throughput_in_mbps_1 );
    return out_cntr;
}

