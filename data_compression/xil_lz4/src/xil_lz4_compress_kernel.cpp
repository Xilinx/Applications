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
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include <ap_int.h>
#include "xil_compress_engine.h"

#define MIN_BLOCK_SIZE 128

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

//LZ specific Defines
#define BIT 8
#define MIN_OFFSET 1
#define MIN_MATCH 4
#define LZ_MAX_OFFSET_LIMIT 65536
#define LZ_HASH_BIT 12
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT)
#define MAX_MATCH_LEN 255
#define OFFSET_WINDOW 65536
#define MATCH_LEN 6 
#define MATCH_LEVEL 6

typedef ap_uint<GMEM_DWIDTH> uintMemWidth_t;
typedef ap_uint<32> compressd_dt;
typedef ap_uint<64> lz4_compressd_dt;

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#endif
{
void lz4_core(
        hls::stream<uintMemWidth_t> &inStreamMemWidth,
        hls::stream<uintMemWidth_t> &outStreamMemWidth,
        hls::stream<uint16_t> &outStreamMemWidthSize,
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
    hls::stream<lz4_compressd_dt>  lenOffsetOut("lenOffsetOut");
    hls::stream<ap_uint<8> >         lz4Out("lz4Out");
    hls::stream<uint16_t>           lz4OutSize("lz4OutSize");
    #pragma HLS STREAM variable=inStream            depth=2
    #pragma HLS STREAM variable=compressdStream    depth=2
    #pragma HLS STREAM variable=bestMatchStream     depth=2
    #pragma HLS STREAM variable=boosterStream       depth=2
    #pragma HLS STREAM variable=litOut              depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out              depth=1024
    #pragma HLS STREAM variable=lz4OutSize          depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStream            core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream     core=FIFO_SRL
    #pragma HLS RESOURCE variable=boosterStream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut        core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out              core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize          core=FIFO_SRL

#pragma HLS dataflow
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStreamMemWidth,inStream,input_size);
    lz_compress<MATCH_LEN,MATCH_LEVEL,LZ_DICT_SIZE,BIT,MIN_OFFSET,MIN_MATCH,LZ_MAX_OFFSET_LIMIT>(inStream, compressdStream, input_size,left_bytes);
    lz_bestMatchFilter<MATCH_LEN, OFFSET_WINDOW>(compressdStream, bestMatchStream, input_size, left_bytes);
    lz_booster<MAX_MATCH_LEN, OFFSET_WINDOW>(bestMatchStream, boosterStream,input_size,left_bytes);
    lz4_divide(boosterStream, litOut, lenOffsetOut, input_size, max_lit_limit, core_idx);
    lz4_compress(litOut,lenOffsetOut, lz4Out,lz4OutSize,input_size);
    upsizer_sizestream<uint16_t, BIT, GMEM_DWIDTH>(lz4Out,lz4OutSize,outStreamMemWidth,outStreamMemWidthSize);
}

void lz4(
                const uintMemWidth_t *in,      
                uintMemWidth_t       *out,          
                const uint32_t  input_idx[PARALLEL_BLOCK],                      
                const uint32_t  output_idx[PARALLEL_BLOCK],                      
                const uint32_t  input_size[PARALLEL_BLOCK],
                uint32_t        output_size[PARALLEL_BLOCK],
                uint32_t        max_lit_limit[PARALLEL_BLOCK]
                )
{
    hls::stream<uintMemWidth_t>   inStreamMemWidth_0("inStreamMemWidth_0");
    hls::stream<uint16_t >  outStreamMemWidthSize_0("outStreamMemWidthSize_0");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_0("outStreamMemWidth_0");
    #pragma HLS STREAM variable=outStreamMemWidthSize_0      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_0           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_0          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_0      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_0           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_0          core=FIFO_SRL

#if PARALLEL_BLOCK > 1
    hls::stream<uintMemWidth_t>   inStreamMemWidth_1("inStreamMemWidth_1");
    hls::stream<uint16_t >  outStreamMemWidthSize_1("outStreamMemWidthSize_1");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_1("outStreamMemWidth_1");
    #pragma HLS STREAM variable=outStreamMemWidthSize_1      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_1           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_1          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_1      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_1           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_1          core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 2
    hls::stream<uintMemWidth_t>   inStreamMemWidth_2("inStreamMemWidth_2");
    hls::stream<uint16_t >  outStreamMemWidthSize_2("outStreamMemWidthSize_2");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_2("outStreamMemWidth_2");
    #pragma HLS STREAM variable=outStreamMemWidthSize_2      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_2           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_2          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_2      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_2           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_2          core=FIFO_SRL

    hls::stream<uintMemWidth_t>   inStreamMemWidth_3("inStreamMemWidth_3");
    hls::stream<uint16_t >  outStreamMemWidthSize_3("outStreamMemWidthSize_3");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_3("outStreamMemWidth_3");
    #pragma HLS STREAM variable=outStreamMemWidthSize_3      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_3           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_3          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_3      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_3           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_3          core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 4
    hls::stream<uintMemWidth_t>   inStreamMemWidth_4("inStreamMemWidth_4");
    hls::stream<uint16_t >  outStreamMemWidthSize_4("outStreamMemWidthSize_4");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_4("outStreamMemWidth_4");
    #pragma HLS STREAM variable=outStreamMemWidthSize_4      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_4           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_4          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_4      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_4           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_4          core=FIFO_SRL

    hls::stream<uintMemWidth_t>   inStreamMemWidth_5("inStreamMemWidth_5");
    hls::stream<uint16_t >  outStreamMemWidthSize_5("outStreamMemWidthSize_5");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_5("outStreamMemWidth_5");
    #pragma HLS STREAM variable=outStreamMemWidthSize_5      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_5           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_5          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_5      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_5           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_5          core=FIFO_SRL

    hls::stream<uintMemWidth_t>   inStreamMemWidth_6("inStreamMemWidth_6");
    hls::stream<uint16_t >  outStreamMemWidthSize_6("outStreamMemWidthSize_6");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_6("outStreamMemWidth_6");
    #pragma HLS STREAM variable=outStreamMemWidthSize_6      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_6           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_6          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_6      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_6           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_6          core=FIFO_SRL

    hls::stream<uintMemWidth_t>   inStreamMemWidth_7("inStreamMemWidth_7");
    hls::stream<uint16_t >  outStreamMemWidthSize_7("outStreamMemWidthSize_7");
    hls::stream<uintMemWidth_t>   outStreamMemWidth_7("outStreamMemWidth_7");
    #pragma HLS STREAM variable=outStreamMemWidthSize_7      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_7           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_7          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStreamMemWidthSize_7      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_7           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_7          core=FIFO_SRL
#endif

    uint32_t left_bytes = 64;

    #pragma HLS dataflow
    mm2s<GMEM_DWIDTH, GMEM_BURST_SIZE>(in,
                                       input_idx,
                                       inStreamMemWidth_0, 
#if PARALLEL_BLOCK > 1
                                       inStreamMemWidth_1, 
#endif
#if PARALLEL_BLOCK > 2
                                       inStreamMemWidth_2, 
                                       inStreamMemWidth_3, 
#endif
#if PARALLEL_BLOCK > 4
                                       inStreamMemWidth_4, 
                                       inStreamMemWidth_5, 
                                       inStreamMemWidth_6, 
                                       inStreamMemWidth_7, 
#endif
                                       input_size
                                      );
    lz4_core(inStreamMemWidth_0,outStreamMemWidth_0,outStreamMemWidthSize_0,max_lit_limit,input_size[0],0);
#if PARALLEL_BLOCK > 1
    lz4_core(inStreamMemWidth_1,outStreamMemWidth_1,outStreamMemWidthSize_1,max_lit_limit,input_size[1],1);
#endif
#if PARALLEL_BLOCK > 2
    lz4_core(inStreamMemWidth_2,outStreamMemWidth_2,outStreamMemWidthSize_2,max_lit_limit,input_size[2],2);
    lz4_core(inStreamMemWidth_3,outStreamMemWidth_3,outStreamMemWidthSize_3,max_lit_limit,input_size[3],3);
#endif
#if PARALLEL_BLOCK > 4
    lz4_core(inStreamMemWidth_4,outStreamMemWidth_4,outStreamMemWidthSize_4,max_lit_limit,input_size[4],4);
    lz4_core(inStreamMemWidth_5,outStreamMemWidth_5,outStreamMemWidthSize_5,max_lit_limit,input_size[5],5);
    lz4_core(inStreamMemWidth_6,outStreamMemWidth_6,outStreamMemWidthSize_6,max_lit_limit,input_size[6],6);
    lz4_core(inStreamMemWidth_7,outStreamMemWidth_7,outStreamMemWidthSize_7,max_lit_limit,input_size[7],7);
#endif
    s2mm_compress<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
                                                 output_idx,
                                                 outStreamMemWidth_0,
#if PARALLEL_BLOCK > 1
                                                 outStreamMemWidth_1,
#endif
#if PARALLEL_BLOCK > 2
                                                 outStreamMemWidth_2,
                                                 outStreamMemWidth_3,
#endif
#if PARALLEL_BLOCK > 4
                                                 outStreamMemWidth_4,
                                                 outStreamMemWidth_5,
                                                 outStreamMemWidth_6,
                                                 outStreamMemWidth_7,
#endif
                                                 outStreamMemWidthSize_0,
#if PARALLEL_BLOCK > 1
                                                 outStreamMemWidthSize_1,
#endif
#if PARALLEL_BLOCK > 2
                                                 outStreamMemWidthSize_2,
                                                 outStreamMemWidthSize_3,
#endif
#if PARALLEL_BLOCK > 4
                                                 outStreamMemWidthSize_4,
                                                 outStreamMemWidthSize_5,
                                                 outStreamMemWidthSize_6,
                                                 outStreamMemWidthSize_7,
#endif
                                                 output_size
                                                );
}
} // End of namespace

extern "C" {
#if (C_COMPUTE_UNIT == 1)
void xil_lz4_cu1
#elif (C_COMPUTE_UNIT == 2)
void xil_lz4_cu2
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
        cu1::lz4(in,out,input_idx,output_idx,input_block_size,output_block_size,max_lit_limit);
#elif (C_COMPUTE_UNIT == 2)
        cu2::lz4(in,out,input_idx,output_idx,input_block_size,output_block_size,max_lit_limit);
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
