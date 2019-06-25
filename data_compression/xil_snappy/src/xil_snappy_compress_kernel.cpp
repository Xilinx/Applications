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
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include <ap_int.h>
#include "xil_compress_engine.h"

#define MIN_BLOCK_SIZE 16

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

//LZ specific Defines
#define BIT 8
#define MIN_OFFSET 1
#define MIN_MATCH 4
#define LZ_MAX_OFFSET_LIMIT 65536
#define LZ_HASH_BIT 12
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT)
#define MAX_MATCH_LEN 64
#define OFFSET_WINDOW 65536
#define MATCH_LEN 6 
#define MATCH_LEVEL 6

typedef ap_uint<GMEM_DWIDTH> uintMemWidth_t;
typedef ap_uint<32> compressd_dt;
typedef ap_uint<64> snappy_compressd_dt;

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#endif
{
void snappy_core(
        hls::stream<uintMemWidth_t> &inStreamMemWidth,
        hls::stream<uintMemWidth_t> &outStreamMemWidth,
        hls::stream<bool> &outStreamMemWidthSize,
        hls::stream<uint32_t> &compressedSize,
        uint32_t max_lit_limit[PARALLEL_BLOCK],
        uint32_t input_size,
        uint32_t core_idx
        )
{
    uint32_t left_bytes = 64;
    hls::stream<ap_uint<BIT> >       inStream("inStream");
    hls::stream<compressd_dt>      compressdStream("compressdStream");
    hls::stream<compressd_dt>      bestMatchStream("bestMatchStream"); 
    hls::stream<compressd_dt>      boosterStream("boosterStream");
    hls::stream<uint8_t>            litOut("litOut");
    hls::stream<snappy_compressd_dt>  lenOffsetOut("lenOffsetOut");
    hls::stream<ap_uint<8> >         snappyOut("snappyOut");
    hls::stream<bool>           snappyOut_eos("snappyOut_eos");
    #pragma HLS STREAM variable=inStream            depth=2
    #pragma HLS STREAM variable=compressdStream    depth=2
    #pragma HLS STREAM variable=bestMatchStream     depth=2
    #pragma HLS STREAM variable=boosterStream       depth=2
    #pragma HLS STREAM variable=litOut              depth=max_literal_stream
    #pragma HLS STREAM variable=lenOffsetOut        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=snappyOut              depth=8
    #pragma HLS STREAM variable=snappyOut_eos          depth=8

    #pragma HLS RESOURCE variable=inStream            core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream     core=FIFO_SRL
    #pragma HLS RESOURCE variable=boosterStream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut        core=FIFO_SRL
    #pragma HLS RESOURCE variable=snappyOut              core=FIFO_SRL
    #pragma HLS RESOURCE variable=snappyOut_eos          core=FIFO_SRL

#pragma HLS dataflow
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStreamMemWidth,inStream,input_size);
    lz_compress<MATCH_LEN,MATCH_LEVEL,LZ_DICT_SIZE,BIT,MIN_OFFSET,MIN_MATCH,LZ_MAX_OFFSET_LIMIT>(inStream, compressdStream, input_size,left_bytes);
    lz_bestMatchFilter<MATCH_LEN, OFFSET_WINDOW>(compressdStream, bestMatchStream, input_size, left_bytes);
    lz_booster<MAX_MATCH_LEN,OFFSET_WINDOW>(bestMatchStream, boosterStream, input_size,left_bytes);
    snappy_divide(boosterStream, litOut, lenOffsetOut, input_size, max_lit_limit, core_idx);
    snappy_compress(litOut,lenOffsetOut, snappyOut,snappyOut_eos,compressedSize,input_size);
    upsizer_eos<uint16_t, BIT, GMEM_DWIDTH>(snappyOut,snappyOut_eos,outStreamMemWidth,outStreamMemWidthSize);
}

void snappy(
                const uintMemWidth_t *in,      
                uintMemWidth_t       *out,          
                const uint32_t  input_idx[PARALLEL_BLOCK],                      
                const uint32_t  output_idx[PARALLEL_BLOCK],                      
                const uint32_t  input_size[PARALLEL_BLOCK],
                uint32_t        output_size[PARALLEL_BLOCK],
                uint32_t        max_lit_limit[PARALLEL_BLOCK]
                )
{
    hls::stream<uintMemWidth_t>   inStreamMemWidth[PARALLEL_BLOCK];
    hls::stream<bool>             outStreamMemWidthSize[PARALLEL_BLOCK];
    hls::stream<uintMemWidth_t>   outStreamMemWidth[PARALLEL_BLOCK];
    #pragma HLS STREAM variable=outStreamMemWidthSize      depth=2
    #pragma HLS STREAM variable=inStreamMemWidth           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth          core=FIFO_SRL

    hls::stream<uint32_t> compressedSize[PARALLEL_BLOCK];
    uint32_t left_bytes = 64;

    #pragma HLS dataflow
    mm2s_nb<GMEM_DWIDTH, GMEM_BURST_SIZE, PARALLEL_BLOCK>(in,
                                       input_idx,
                                       inStreamMemWidth, 
                                       input_size
                                      );
    for (uint8_t i = 0; i < PARALLEL_BLOCK; i++){
    #pragma HLS UNROLL
        snappy_core(inStreamMemWidth[i],outStreamMemWidth[i],outStreamMemWidthSize[i],compressedSize[i],max_lit_limit,input_size[i],i);
    }

    s2mm_eos_nb<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH, PARALLEL_BLOCK>(out,
                                                 output_idx,
                                                 outStreamMemWidth,
                                                 outStreamMemWidthSize,
                                                 compressedSize,
                                                 output_size
                                                );
}
} // End of namespace

extern "C" {
#if (C_COMPUTE_UNIT == 1)
void xil_snappy_cu1
#elif (C_COMPUTE_UNIT == 2)
void xil_snappy_cu2
#endif
(
                const uintMemWidth_t *in,      
                uintMemWidth_t       *out,          
                uint32_t        *compressd_size,
                uint32_t        *in_block_size,
                uint32_t block_size_in_kb,
                uint32_t input_size                      
                )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=compressd_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=in_block_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=compressd_size bundle=control
    #pragma HLS INTERFACE s_axilite port=in_block_size bundle=control
    #pragma HLS INTERFACE s_axilite port=block_size_in_kb bundle=control
    #pragma HLS INTERFACE s_axilite port=input_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS data_pack variable=in
    #pragma HLS data_pack variable=out

    int block_idx = 0;
    int block_length = block_size_in_kb * 1024;
    int no_blocks = (input_size - 1) / block_length + 1;
    uint32_t max_block_size = block_size_in_kb * 1024;
    
    bool small_block[PARALLEL_BLOCK];
    uint32_t input_block_size[PARALLEL_BLOCK];
    uint32_t input_idx[PARALLEL_BLOCK];
    uint32_t output_idx[PARALLEL_BLOCK];
    uint32_t output_block_size[PARALLEL_BLOCK];
    uint32_t max_lit_limit[PARALLEL_BLOCK];
    uint32_t small_block_inSize[PARALLEL_BLOCK];
    #pragma HLS ARRAY_PARTITION variable=input_block_size dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=input_idx dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=output_idx dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=output_block_size dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=max_lit_limit dim=0 complete

    // Figure out total blocks & block sizes
    for (int i = 0; i < no_blocks; i+=PARALLEL_BLOCK) {
        int nblocks = PARALLEL_BLOCK;
        if((i + PARALLEL_BLOCK) > no_blocks) {
            nblocks = no_blocks - i;
        }

        for (int j = 0; j < PARALLEL_BLOCK; j++) {
            if (j < nblocks) {
                uint32_t inBlockSize = in_block_size[i + j];
                if (inBlockSize < MIN_BLOCK_SIZE) {
                    small_block[j] = 1;
                    small_block_inSize[j] = inBlockSize;
                    input_block_size[j] = 0;
                    input_idx[j] = 0;
                } else {
                    small_block[j] = 0;
                    input_block_size[j] = inBlockSize; 
                    input_idx[j] = (i + j) * max_block_size;    
                    output_idx[j] = (i + j) * max_block_size;
                }
            }else {
                input_block_size[j] = 0;
                input_idx[j] = 0;
            }
            output_block_size[j] = 0;
            max_lit_limit[j] = 0;
        }

        // Call for parallel compression
#if (C_COMPUTE_UNIT == 1)
        cu1::snappy(in,out,input_idx,output_idx,input_block_size,output_block_size,max_lit_limit);
#elif (C_COMPUTE_UNIT == 2)
        cu2::snappy(in,out,input_idx,output_idx,input_block_size,output_block_size,max_lit_limit);
#endif

        for(int k = 0; k < nblocks; k++) {
            if (max_lit_limit[k]) {
                compressd_size[block_idx] = input_block_size[k];   
            } else { 
                compressd_size[block_idx] = output_block_size[k]; 
            }
    
            if (small_block[k] == 1) {
                compressd_size[block_idx] = small_block_inSize[k];       
            }
            block_idx++;
        }

    }
}
}
