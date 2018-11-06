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
#include "stream_utils.h"
#include "lz_compress.h"
#include <ap_int.h>
#include "xil_lz4_config.h"

#define MARKER 255
#define MAX_OFFSET 65536 
#define HISTORY_SIZE MAX_OFFSET

#define BIT 8
#define READ_STATE 0
#define MATCH_STATE 1
#define LOW_OFFSET_STATE 2 
#define LOW_OFFSET 8 // This should be bigger than Pipeline Depth to handle inter dependency false case

//LZ4 Decompress states
#define READ_TOKEN      0
#define READ_LIT_LEN    1
#define READ_LITERAL    2
#define READ_OFFSET0    3
#define READ_OFFSET1    4
#define READ_MATCH_LEN  5

typedef ap_uint<BIT> uintV_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> compressd_dt;

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

#if (D_COMPUTE_UNIT == 1)
namespace  dec_cu1 
#elif (D_COMPUTE_UNIT == 2)
namespace  dec_cu2 
#endif
{
void lz4_decompressr(
        hls::stream<ap_uint<8> > &inStream,          
        hls::stream<compressd_dt> &outStream,          
        uint32_t input_size
    )
{

    uint8_t next_state = READ_TOKEN;
    ap_uint<8> nextValue;
    ap_uint<16> offset;
    compressd_dt decompressdOut=0;
    uint32_t lit_len = 0;
    uint32_t match_len = 0;
    lz4_decompressr: for (uint32_t i = 0 ; i < input_size ; i++){
    #pragma HLS PIPELINE II=1
        ap_uint<8> inValue= inStream.read();
        if (next_state == READ_TOKEN){
            lit_len     = inValue.range(7,4);
            match_len   = inValue.range(3,0); 
            if (lit_len == 0xF){
                next_state = READ_LIT_LEN; 
            }else if (lit_len){
                next_state = READ_LITERAL;
            }else{
                next_state = READ_OFFSET0;
            }
        }else if ( next_state == READ_LIT_LEN){
            lit_len += inValue;
            if (inValue != 0xFF){
                next_state = READ_LITERAL;
            }
        }else if (next_state  == READ_LITERAL){
            compressd_dt outValue = 0;
            outValue.range(7,0) = inValue;
            outStream << outValue;
            lit_len--;
            if (lit_len == 0){
                next_state = READ_OFFSET0;
            }
        }else if (next_state == READ_OFFSET0){
            offset.range(7,0) = inValue;
            next_state = READ_OFFSET1;
        }else if (next_state == READ_OFFSET1){
            offset.range(15,8) = inValue;
            if (match_len == 0xF){
                next_state = READ_MATCH_LEN;
            }else{
                next_state = READ_TOKEN;
                compressd_dt outValue = 0;
                outValue.range(31,16)    = (match_len+3); //+3 for LZ4 standard
                outValue.range(15,0)   = (offset-1); //-1 for LZ4 standard
                outStream << outValue;
            }
        }else if (next_state == READ_MATCH_LEN){
            match_len += inValue;
            if (inValue!=0xFF){
                compressd_dt outValue = 0;
                outValue.range(31,16)    = (match_len+3); //+3 for LZ4 standard
                outValue.range(15,0)   = (offset-1); //-1 for LZ4 standard
                outStream << outValue;
                next_state = READ_TOKEN;
            }
        }
    }
}
void lz4_core(
        hls::stream<ap_uint<GMEM_DWIDTH> > &instream_512, 
        hls::stream<ap_uint<GMEM_DWIDTH> > &outstream_512, 
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
    #pragma HLS STREAM variable=instreamV               depth=c_gmem_burst_size
    #pragma HLS STREAM variable=decompressd_stream          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=decompressed_stream     depth=c_gmem_burst_size
    #pragma HLS RESOURCE variable=instreamV             core=FIFO_SRL
    #pragma HLS RESOURCE variable=decompressd_stream        core=FIFO_SRL
    #pragma HLS RESOURCE variable=decompressed_stream   core=FIFO_SRL

    #pragma HLS dataflow 
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(instream_512,instreamV,input_size); 
    lz4_decompressr(instreamV, decompressd_stream, input_size1); 
    lz_decompress<HISTORY_SIZE,READ_STATE,MATCH_STATE,LOW_OFFSET_STATE,LOW_OFFSET>(decompressd_stream, decompressed_stream, output_size); 
    stream_upsizer<uint32_t, 8, GMEM_DWIDTH>(decompressed_stream, outstream_512, output_size1);
}

void lz4_dec(
                const uint512_t *in,      
                uint512_t       *out,          
                const uint32_t  input_idx[PARALLEL_BLOCK],
                const uint32_t  input_size[PARALLEL_BLOCK],
                const uint32_t  output_size[PARALLEL_BLOCK],
                const uint32_t  input_size1[PARALLEL_BLOCK],
                const uint32_t  output_size1[PARALLEL_BLOCK]
                )
{
    hls::stream<uint512_t> inStream512_0("inStream512_0"); 
    hls::stream<uint512_t> outStream512_0("outStream512_0"); 
    #pragma HLS STREAM variable=inStream512_0 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_0 depth=c_gmem_burst_size
    #pragma HLS RESOURCE variable=inStream512_0  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_0 core=FIFO_SRL
#if PARALLEL_BLOCK > 1
    hls::stream<uint512_t> inStream512_1("inStream512_1");
    hls::stream<uint512_t> outStream512_1("outStream512_1"); 
    #pragma HLS STREAM variable=inStream512_1 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_1 depth=c_gmem_burst_size
	#pragma HLS RESOURCE variable=inStream512_1  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_1 core=FIFO_SRL
#endif 
#if PARALLEL_BLOCK > 2
    hls::stream<uint512_t> inStream512_2("inStream512_2"); 
    hls::stream<uint512_t> inStream512_3("inStream512_3"); 
    hls::stream<uint512_t> outStream512_2("outStream512_2"); 
    hls::stream<uint512_t> outStream512_3("outStream512_3"); 
    #pragma HLS STREAM variable=inStream512_2 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_3 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_2 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_3 depth=c_gmem_burst_size
	#pragma HLS RESOURCE variable=inStream512_2  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_2 core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_3  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_3 core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 4
    hls::stream<uint512_t> inStream512_4("inStream512_4"); 
    hls::stream<uint512_t> inStream512_5("inStream512_5"); 
    hls::stream<uint512_t> inStream512_6("inStream512_6"); 
    hls::stream<uint512_t> inStream512_7("inStream512_7"); 
    hls::stream<uint512_t> outStream512_4("outStream512_4"); 
    hls::stream<uint512_t> outStream512_5("outStream512_5"); 
    hls::stream<uint512_t> outStream512_6("outStream512_6"); 
    hls::stream<uint512_t> outStream512_7("outStream512_7"); 
    #pragma HLS STREAM variable=inStream512_4 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_5 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_6 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_7 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_4 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_5 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_6 depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_7 depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStream512_4  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_4 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStream512_5  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_5 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStream512_6  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_6 core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=inStream512_7  core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_7 core=FIFO_SRL
#endif
    #pragma HLS dataflow    
    // Transfer data from global memory to kernel
    mm2s<GMEM_DWIDTH, GMEM_BURST_SIZE>(in,
                                       input_idx,
                                       inStream512_0,
#if PARALLEL_BLOCK > 1
                                       inStream512_1,
#endif
#if PARALLEL_BLOCK > 2
                                       inStream512_2,
                                       inStream512_3,
#endif
#if PARALLEL_BLOCK > 4
                                       inStream512_4,
                                       inStream512_5,
                                       inStream512_6,
                                       inStream512_7,
#endif
                                       input_size
                                      );
   lz4_core(inStream512_0,outStream512_0, input_size1[0],output_size1[0]); 
#if PARALLEL_BLOCK > 1
   lz4_core(inStream512_1,outStream512_1, input_size1[1],output_size1[1]); 
#endif
#if PARALLEL_BLOCK > 2
   lz4_core(inStream512_2,outStream512_2, input_size1[2],output_size1[2]); 
   lz4_core(inStream512_3,outStream512_3, input_size1[3],output_size1[3]); 
#endif
#if PARALLEL_BLOCK > 4
   lz4_core(inStream512_4,outStream512_4, input_size1[4],output_size1[4]); 
   lz4_core(inStream512_5,outStream512_5, input_size1[5],output_size1[5]); 
   lz4_core(inStream512_6,outStream512_6, input_size1[6],output_size1[6]); 
   lz4_core(inStream512_7,outStream512_7, input_size1[7],output_size1[7]); 
#endif
    // Transfer data from kernel to global memory
   s2mm_decompress<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
                                                 input_idx,
                                                 outStream512_0,
#if PARALLEL_BLOCK > 1
                                                 outStream512_1,
#endif
#if PARALLEL_BLOCK > 2
                                                 outStream512_2,
                                                 outStream512_3,
#endif
#if PARALLEL_BLOCK > 4
                                                 outStream512_4,
                                                 outStream512_5,
                                                 outStream512_6,
                                                 outStream512_7,
#endif
                                                 output_size
                                                );

}

}//end of namepsace

extern "C" {
#if (D_COMPUTE_UNIT == 1)
void xil_lz4_dec_cu1
#elif (D_COMPUTE_UNIT == 2)
void xil_lz4_dec_cu2
#endif
(
                const uint512_t *in,      
                uint512_t       *out,          
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

    //printf ("In decode compute unit %d no_blocks %d\n", D_COMPUTE_UNIT, no_blocks);   
 
    for (int i = 0; i < no_blocks; i+=PARALLEL_BLOCK) {
        
        int nblocks = PARALLEL_BLOCK;
        if((i + PARALLEL_BLOCK) > no_blocks) {
            nblocks = no_blocks - i;
        }

        for (int j = 0; j < PARALLEL_BLOCK; j++) {
            if(j < nblocks) {
                uint32_t iSize = in_compress_size[i + j];
                uint32_t oSize = in_block_size[i + j];
                //printf("iSize %d oSize %d \n", iSize, oSize);
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
        dec_cu1::lz4_dec(in, out, input_idx, compress_size, block_size, compress_size1, block_size1);
#elif (D_COMPUTE_UNIT == 2)
        dec_cu2::lz4_dec(in, out, input_idx, compress_size, block_size, compress_size1, block_size1);
#endif
    }

}
}
