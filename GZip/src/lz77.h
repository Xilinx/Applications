#pragma once
#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <chrono>
#include "xcl2.hpp"
#include <vector>
using namespace std;

#ifdef VEC_8
    #define VEC 8 
#else
    #define VEC 16
#endif
#ifndef COMPUTE_UNITS
#define COMPUTE_UNITS 1
#endif

// Macro timers
#define TIMING

#define MAX_INPUT_SIZE 1024*1024*100 // 100MB

#ifdef TIMING
#define INIT_TIMER auto start = std::chrono::high_resolution_clock::now();
#define TOTAL_TIMER auto total = std::chrono::high_resolution_clock::now();
#define START_TIMER  start = std::chrono::high_resolution_clock::now();
#define STOP_TIMER(name)  std::cout << "Execution time " << name << ": " << \
    std::chrono::duration_cast<std::chrono::milliseconds>( \
            std::chrono::high_resolution_clock::now()-start \
    ).count() << " ms \n" << std::endl; 
#define TOP_TIMER  total = std::chrono::high_resolution_clock::now();
#define BOTTOM_TIMER(name)  std::cout << "Execution time " << name << ": " << \
    std::chrono::duration_cast<std::chrono::milliseconds>( \
            std::chrono::high_resolution_clock::now()-total \
    ).count() << " ms \n" << std::endl; 
#else
#define INIT_TIMER
#define TOTAL_TIMER
#define START_TIMER
#define STOP_TIMER(name)
#define TOP_TIMER
#define BOTTOM_TIMER
#endif
uint32_t lz77_decode(uint8_t *in, uint8_t  *out,uint32_t  size);

class xil_lz77 {
    public:
        int init(const std::string& binaryFile);
        int release();
        uint32_t encode(uint8_t *in, uint8_t *out, long actual_size);
    private:
        cl::Program *m_program;
        cl::Context *m_context;
        cl::CommandQueue *m_q;
        std::vector<uint8_t,aligned_allocator<uint8_t>> *local_out;
        std::vector<uint32_t,aligned_allocator<uint32_t>> *sizeOut[COMPUTE_UNITS];
        cl_mem_ext_ptr_t get_buffer_extension(int ddr_no);
};
