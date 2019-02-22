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
#include "xil_decompress_engine.h"

#define MAX_OFFSET 65536 
#define HISTORY_SIZE MAX_OFFSET

#define BIT 8
#define READ_STATE 0
#define MATCH_STATE 1
#define LOW_OFFSET_STATE 2 
#define LOW_OFFSET 8 // This should be bigger than Pipeline Depth to handle inter dependency false case

typedef ap_uint<BIT> uintV_t;
typedef ap_uint<GMEM_DWIDTH> uintMemWidth_t;
typedef ap_uint<32> compressd_dt;

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

#if (D_COMPUTE_UNIT == 1)
namespace  dec_cu1 
#elif (D_COMPUTE_UNIT == 2)
namespace  dec_cu2 
#endif
{
void snappy_core(
        hls::stream<uintMemWidth_t> &inStreamMemWidth, 
        hls::stream<uintMemWidth_t> &outStreamMemWidth, 
        const uint32_t _input_size,
        const uint32_t _output_size
        )
{
    uint32_t input_size = _input_size;
    uint32_t output_size = _output_size;
    uint32_t input_size1  = input_size;
    uint32_t output_size1 = output_size;
    hls::stream<uintV_t>    instreamV("instreamV"); 
    hls::stream<compressd_dt> decompressd_stream("decompressd_stream"); 
    hls::stream<uintV_t>    decompressed_stream("decompressed_stream");
    #pragma HLS STREAM variable=instreamV               depth=2
    #pragma HLS STREAM variable=decompressd_stream          depth=2
    #pragma HLS STREAM variable=decompressed_stream     depth=2
    #pragma HLS RESOURCE variable=instreamV             core=FIFO_SRL
    #pragma HLS RESOURCE variable=decompressd_stream        core=FIFO_SRL
    #pragma HLS RESOURCE variable=decompressed_stream   core=FIFO_SRL

    #pragma HLS dataflow 
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStreamMemWidth,instreamV,input_size);
    snappy_decompressr(instreamV, decompressd_stream, input_size1); 
    lz_decompress<HISTORY_SIZE,READ_STATE,MATCH_STATE,LOW_OFFSET_STATE,LOW_OFFSET>(decompressd_stream, decompressed_stream, output_size); 
    stream_upsizer<uint32_t, 8, GMEM_DWIDTH>(decompressed_stream, outStreamMemWidth, output_size1);
}

void snappy_dec(
                const uintMemWidth_t *in,      
                uintMemWidth_t       *out,          
                const uint32_t  input_idx[PARALLEL_BLOCK],
                const uint32_t  input_size[PARALLEL_BLOCK],
                const uint32_t  output_size[PARALLEL_BLOCK],
                const uint32_t  input_size1[PARALLEL_BLOCK],
                const uint32_t  output_size1[PARALLEL_BLOCK]
                )
{
    hls::stream<uintMemWidth_t> inStreamMemWidth_0("inStreamMemWidth_0"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_0("outStreamMemWidth_0"); 
    #pragma HLS STREAM variable=inStreamMemWidth_0 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_0 depth=c_gmem_burst_size
    #pragma HLS RESOURCE variable=inStreamMemWidth_0  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_0 core=FIFO_SRL
#if PARALLEL_BLOCK > 1
    hls::stream<uintMemWidth_t> inStreamMemWidth_1("inStreamMemWidth_1");
    hls::stream<uintMemWidth_t> outStreamMemWidth_1("outStreamMemWidth_1"); 
    #pragma HLS STREAM variable=inStreamMemWidth_1 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_1 depth=c_gmem_burst_size
	#pragma HLS RESOURCE variable=inStreamMemWidth_1  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_1 core=FIFO_SRL
#endif 
#if PARALLEL_BLOCK > 2
    hls::stream<uintMemWidth_t> inStreamMemWidth_2("inStreamMemWidth_2"); 
    hls::stream<uintMemWidth_t> inStreamMemWidth_3("inStreamMemWidth_3"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_2("outStreamMemWidth_2"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_3("outStreamMemWidth_3"); 
    #pragma HLS STREAM variable=inStreamMemWidth_2 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_3 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_2 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_3 depth=c_gmem_burst_size
	#pragma HLS RESOURCE variable=inStreamMemWidth_2  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_2 core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStreamMemWidth_3  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_3 core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 4
    hls::stream<uintMemWidth_t> inStreamMemWidth_4("inStreamMemWidth_4"); 
    hls::stream<uintMemWidth_t> inStreamMemWidth_5("inStreamMemWidth_5"); 
    hls::stream<uintMemWidth_t> inStreamMemWidth_6("inStreamMemWidth_6"); 
    hls::stream<uintMemWidth_t> inStreamMemWidth_7("inStreamMemWidth_7"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_4("outStreamMemWidth_4"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_5("outStreamMemWidth_5"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_6("outStreamMemWidth_6"); 
    hls::stream<uintMemWidth_t> outStreamMemWidth_7("outStreamMemWidth_7"); 
    #pragma HLS STREAM variable=inStreamMemWidth_4 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_5 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_6 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamMemWidth_7 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_4 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_5 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_6 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStreamMemWidth_7 depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStreamMemWidth_4  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_4 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStreamMemWidth_5  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_5 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStreamMemWidth_6  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_6 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStreamMemWidth_7  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamMemWidth_7 core=FIFO_SRL
#endif
    #pragma HLS dataflow    
    // Transfer data from global memory to kernel
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
   snappy_core(inStreamMemWidth_0,outStreamMemWidth_0, input_size1[0],output_size1[0]); 
#if PARALLEL_BLOCK > 1
   snappy_core(inStreamMemWidth_1,outStreamMemWidth_1, input_size1[1],output_size1[1]); 
#endif
#if PARALLEL_BLOCK > 2
   snappy_core(inStreamMemWidth_2,outStreamMemWidth_2, input_size1[2],output_size1[2]); 
   snappy_core(inStreamMemWidth_3,outStreamMemWidth_3, input_size1[3],output_size1[3]); 
#endif
#if PARALLEL_BLOCK > 4
   snappy_core(inStreamMemWidth_4,outStreamMemWidth_4, input_size1[4],output_size1[4]); 
   snappy_core(inStreamMemWidth_5,outStreamMemWidth_5, input_size1[5],output_size1[5]); 
   snappy_core(inStreamMemWidth_6,outStreamMemWidth_6, input_size1[6],output_size1[6]); 
   snappy_core(inStreamMemWidth_7,outStreamMemWidth_7, input_size1[7],output_size1[7]); 
#endif
    // Transfer data from kernel to global memory
   s2mm_decompress<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
                                                 input_idx,
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
                                                 output_size
                                                );

}

}//end of namepsace

extern "C" {
#if (D_COMPUTE_UNIT == 1)
void xil_snappy_dec_cu1
#elif (D_COMPUTE_UNIT == 2)
void xil_snappy_dec_cu2
#endif
(
                const uintMemWidth_t *in,      
                uintMemWidth_t       *out,          
                uint32_t        *in_block_size,
                uint32_t        *in_compress_size,
                uint32_t        block_size_in_kb,                     
                uint32_t        no_blocks
                )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=in_block_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=in_compress_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=in_block_size bundle=control
    #pragma HLS INTERFACE s_axilite port=in_compress_size bundle=control
    #pragma HLS INTERFACE s_axilite port=block_size_in_kb bundle=control
    #pragma HLS INTERFACE s_axilite port=no_blocks bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS data_pack variable=in
    #pragma HLS data_pack variable=out
    uint32_t max_block_size = block_size_in_kb * 1024;
    uint32_t compress_size[PARALLEL_BLOCK];
    uint32_t compress_size1[PARALLEL_BLOCK];
    uint32_t block_size[PARALLEL_BLOCK];
    uint32_t block_size1[PARALLEL_BLOCK];
    uint32_t input_idx[PARALLEL_BLOCK];
    #pragma HLS ARRAY_PARTITION variable=input_idx dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=compress_size dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=compress_size1 dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=block_size dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=block_size1 dim=0 complete

    for (int i = 0; i < no_blocks; i+=PARALLEL_BLOCK) {
        
        int nblocks = PARALLEL_BLOCK;
        if((i + PARALLEL_BLOCK) > no_blocks) {
            nblocks = no_blocks - i;
        }

        for (int j = 0; j < PARALLEL_BLOCK; j++) {
            if(j < nblocks) {
                uint32_t iSize = in_compress_size[i + j];
                uint32_t oSize = in_block_size[i + j];
                compress_size[j] = iSize;
                block_size[j]  = oSize;
                compress_size1[j] = iSize;
                block_size1[j]  = oSize;
                input_idx[j]   = (i + j) * max_block_size;
            } else  {
                compress_size[j] = 0;
                block_size[j]  = 0;
                compress_size1[j] = 0;
                block_size1[j]  = 0;
                input_idx[j]   = 0;
            }

        }
#if (D_COMPUTE_UNIT == 1)
        dec_cu1::snappy_dec(in, out, input_idx, compress_size, block_size, compress_size1, block_size1);
#elif (D_COMPUTE_UNIT == 2)
        dec_cu2::snappy_dec(in, out, input_idx, compress_size, block_size, compress_size1, block_size1);
#endif
    }

}
}
