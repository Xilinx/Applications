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
#include "stream_utils.h"
#include "lz_compress.h"
#include <ap_int.h>
#include "xil_lzma_config.h"
#include "xil_lzma_rc_kernel.h"

#define MIN_BLOCK_SIZE 128

//LZMA Compress STATES
#define WRITE_TOKEN         0
#define WRITE_LIT_LEN       1
#define WRITE_MATCH_LEN     2
#define WRITE_LITERAL       3
#define WRITE_OFFSET0       4
#define WRITE_OFFSET1       5

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

//LZ specific Defines
#define BIT 8
#define MIN_OFFSET 1
#define MIN_MATCH 4
//#define LZ_MAX_OFFSET_LIMIT 1048576//65536//1048576//65536//8388608 //65536
#define LZ_HASH_BIT 24//25//27//26//24//12
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT) //(50331648)//
#define LZ_MAX_OFFSET_LIMIT 1073741824//7340032//1048576//65536//65536//1048576//65536
//#define MATCH_LEN (1*VEC)
#define MAX_MATCH_LEN 255
#define OFFSET_WINDOW 1073741824//7340032//1048576//65536//1048576//65536
#define MATCH_LEN 12 
#define MIN_MATCH 2
typedef ap_uint<VEC * BIT> uintV_t;
typedef ap_uint<MATCH_LEN * BIT> uintMatchV_t;
#define MATCH_LEVEL 4 
#define DICT_ELE_WIDTH (MATCH_LEN*BIT + 32)//24)

typedef ap_uint<DICT_ELE_WIDTH> uintDict_t;
typedef ap_uint< MATCH_LEVEL * DICT_ELE_WIDTH> uintDictV_t;

#define OUT_BYTES (4) 
typedef ap_uint< OUT_BYTES * BIT> uintOut_t;
typedef ap_uint< 2  * OUT_BYTES * BIT> uintDoubleOut_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<64> compressd_dt;
typedef ap_uint<VEC*32> compressdV_dt;
typedef ap_uint<64> lzma_compressd_dt;

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#endif
{

void lzma_core(
        hls::stream<uint512_t> &inStream512,
        uint512_t       *dict_buff1,
        //uint512_t       *dict_buff2,
        //uint512_t       *dict_buff3,
        uint32_t max_lit_limit[PARALLEL_BLOCK],
        uint32_t input_size,
        uint32_t core_idx,
        hls::stream<uint512_t> &outStream512,
        hls::stream<uint16_t> &outStream512Size,
        uint32_t last_index
        )
{
    uint32_t left_bytes = 0;
    hls::stream<ap_uint<BIT> >       inStream("inStream");
    hls::stream<compressd_dt>      compressdStream("compressdStream");
    hls::stream<compressd_dt>      boosterStream("boosterStream");
    hls::stream<uint16_t>           compOutSize("compOutSize");
	
    hls::stream<ap_uint<512> >           packStream("packStream");
    hls::stream<uint8_t >           packsizeStream("packsizeStream");

	hls::stream<ap_uint<8> >           symStream("symStream");	
    hls::stream<ap_uint<32> >           probsStream("probsStream");
    hls::stream<uint16_t >           outStreamSize("outStreamSize");

    hls::stream<ap_uint<512> >           symStream64("symStream64");	
    hls::stream<ap_uint<1024> >           probsStream64("probsStream64");
    hls::stream<uint16_t >           outStreamSize64("outStreamSize64");

    hls::stream<ap_uint<32> >           rangeStream("rangeStream");	
    hls::stream<ap_uint<64> >           lowStream("lowStream");
    hls::stream<uint16_t >           outStreamSize2("outStreamSize2");

    hls::stream<ap_uint<8> >           rcStream("rcStream");
    hls::stream<uint16_t>           rcOutSize("rcOutSize");
    #pragma HLS STREAM variable=inStream             depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=boosterStream        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compOutSize          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=rcStream             depth=c_gmem_burst_size
    #pragma HLS STREAM variable=rcOutSize            depth=c_gmem_burst_size
	
    #pragma HLS STREAM variable=packStream            depth=c_gmem_burst_size
    #pragma HLS STREAM variable=packsizeStream            depth=c_gmem_burst_size

    #pragma HLS STREAM variable=symStream            depth=128
    #pragma HLS STREAM variable=probsStream            depth=128
    #pragma HLS STREAM variable=outStreamSize            depth=128

    #pragma HLS STREAM variable=symStream64            depth=128
    #pragma HLS STREAM variable=probsStream64            depth=128
    #pragma HLS STREAM variable=outStreamSize64            depth=128


    #pragma HLS STREAM variable=rangeStream            depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lowStream            depth=c_gmem_burst_size	
    #pragma HLS STREAM variable=outStreamSize2            depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStream            core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream     core=FIFO_SRL
    #pragma HLS RESOURCE variable=boosterStream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=compOutSize         core=FIFO_SRL
    #pragma HLS RESOURCE variable=rcStream            core=FIFO_SRL
    #pragma HLS RESOURCE variable=rcOutSize           core=FIFO_SRL
	
    #pragma HLS RESOURCE variable=packStream         core=FIFO_SRL
    #pragma HLS RESOURCE variable=packsizeStream         core=FIFO_SRL

    #pragma HLS RESOURCE variable=symStream         core=FIFO_SRL
    #pragma HLS RESOURCE variable=probsStream         core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamSize         core=FIFO_SRL

    #pragma HLS RESOURCE variable=symStream64         core=FIFO_SRL
    #pragma HLS RESOURCE variable=probsStream64         core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamSize64         core=FIFO_SRL
    
    #pragma HLS RESOURCE variable=rangeStream         core=FIFO_SRL
    #pragma HLS RESOURCE variable=lowStream         core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStreamSize2         core=FIFO_SRL

#pragma HLS dataflow
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512,inStream,input_size);
    lz_compress<MATCH_LEN,MATCH_LEVEL,LZ_DICT_SIZE,BIT,MIN_OFFSET,MIN_MATCH,LZ_MAX_OFFSET_LIMIT>(inStream,compressdStream,dict_buff1,/*dict_buff2,dict_buff3,*/input_size,left_bytes,last_index);
    lz_filter(compressdStream, boosterStream,input_size,left_bytes,compOutSize);

    lzma_rc_1(boosterStream,compOutSize, symStream64,probsStream64,outStreamSize64, input_size,last_index);
    //lzma_rc_1_1(boosterStream,compOutSize, packStream,packsizeStream, input_size,last_index);
    //lzma_rc_1_2(packStream,packsizeStream,symStream64,probsStream64,outStreamSize64);
    lzma_rc_converter(symStream64,probsStream64,outStreamSize64,symStream,probsStream,outStreamSize);
    lzma_rc_2(symStream,probsStream,outStreamSize,rangeStream,lowStream,outStreamSize2);
    lzma_rc_3(rangeStream,lowStream,outStreamSize2,rcStream,rcOutSize);
    upsizer_sizestream<uint16_t, 8, GMEM_DWIDTH>(rcStream,rcOutSize,outStream512,outStream512Size);
}

void lzma(
                const uint512_t *in,      
                uint512_t       *out,		
                uint512_t       *dict_buff1, 
                //uint512_t       *dict_buff2,
                //uint512_t       *dict_buff3,      
                const uint32_t  input_idx[PARALLEL_BLOCK],                      
                const uint32_t  output_idx[PARALLEL_BLOCK],                      
                const uint32_t  input_size[PARALLEL_BLOCK],
                uint32_t        output_size[PARALLEL_BLOCK],
                uint32_t        max_lit_limit[PARALLEL_BLOCK],
                uint32_t last_index
                )
{
    hls::stream<uint512_t>   inStream512_0("inStream512_0");
    hls::stream<uint16_t >  outStream512Size_0("outStream512Size_0");
    hls::stream<uint512_t>   outStream512_0("outStream512_0");
    #pragma HLS STREAM variable=outStream512Size_0      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_0           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_0          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_0      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_0           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_0          core=FIFO_SRL

#if PARALLEL_BLOCK > 1
    hls::stream<uint512_t>   inStream512_1("inStream512_1");
    hls::stream<uint16_t >  outStream512Size_1("outStream512Size_1");
    hls::stream<uint512_t>   outStream512_1("outStream512_1");
    #pragma HLS STREAM variable=outStream512Size_1      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_1           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_1          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_1      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_1           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_1          core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 2
    hls::stream<uint512_t>   inStream512_2("inStream512_2");
    hls::stream<uint16_t >  outStream512Size_2("outStream512Size_2");
    hls::stream<uint512_t>   outStream512_2("outStream512_2");
    #pragma HLS STREAM variable=outStream512Size_2      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_2           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_2          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_2      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_2           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_2          core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_3("inStream512_3");
    hls::stream<uint16_t >  outStream512Size_3("outStream512Size_3");
    hls::stream<uint512_t>   outStream512_3("outStream512_3");
    #pragma HLS STREAM variable=outStream512Size_3      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_3           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_3          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_3      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_3           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_3          core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 4
    hls::stream<uint512_t>   inStream512_4("inStream512_4");
    hls::stream<uint16_t >  outStream512Size_4("outStream512Size_4");
    hls::stream<uint512_t>   outStream512_4("outStream512_4");
    #pragma HLS STREAM variable=outStream512Size_4      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_4           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_4          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_4      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_4           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_4          core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_5("inStream512_5");
    hls::stream<uint16_t >  outStream512Size_5("outStream512Size_5");
    hls::stream<uint512_t>   outStream512_5("outStream512_5");
    #pragma HLS STREAM variable=outStream512Size_5      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_5           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_5          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_5      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_5           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_5          core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_6("inStream512_6");
    hls::stream<uint16_t >  outStream512Size_6("outStream512Size_6");
    hls::stream<uint512_t>   outStream512_6("outStream512_6");
    #pragma HLS STREAM variable=outStream512Size_6      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_6           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_6          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_6      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_6           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_6          core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_7("inStream512_7");
    hls::stream<uint16_t >  outStream512Size_7("outStream512Size_7");
    hls::stream<uint512_t>   outStream512_7("outStream512_7");
    #pragma HLS STREAM variable=outStream512Size_7      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_7           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_7          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_7      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_7           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_7          core=FIFO_SRL
#endif

    uint32_t left_bytes = 64;

    #pragma HLS dataflow
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
    lzma_core(inStream512_0,dict_buff1,/*dict_buff2,dict_buff3,*/max_lit_limit,input_size[0],0,outStream512_0,outStream512Size_0,last_index);

#if PARALLEL_BLOCK > 1
    lzma_core(inStream512_1,outStream512_1,outStream512Size_1,max_lit_limit,input_size[1],1);
#endif
#if PARALLEL_BLOCK > 2
    lzma_core(inStream512_2,outStream512_2,outStream512Size_2,max_lit_limit,input_size[2],2);
    lzma_core(inStream512_3,outStream512_3,outStream512Size_3,max_lit_limit,input_size[3],3);
#endif
#if PARALLEL_BLOCK > 4
    lzma_core(inStream512_4,outStream512_4,outStream512Size_4,max_lit_limit,input_size[4],4);
    lzma_core(inStream512_5,outStream512_5,outStream512Size_5,max_lit_limit,input_size[5],5);
    lzma_core(inStream512_6,outStream512_6,outStream512Size_6,max_lit_limit,input_size[6],6);
    lzma_core(inStream512_7,outStream512_7,outStream512Size_7,max_lit_limit,input_size[7],7);
#endif

    s2mm_compress<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
                                                 output_idx,
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
                                                 outStream512Size_0,
#if PARALLEL_BLOCK > 1
                                                 outStream512Size_1,
#endif
#if PARALLEL_BLOCK > 2
                                                 outStream512Size_2,
                                                 outStream512Size_3,
#endif
#if PARALLEL_BLOCK > 4
                                                 outStream512Size_4,
                                                 outStream512Size_5,
                                                 outStream512Size_6,
                                                 outStream512Size_7,
#endif
                                                 output_size
                                                );

}
} // End of namespace

extern "C" {
#if (C_COMPUTE_UNIT == 1)
void xil_lzma_cu1
#elif (C_COMPUTE_UNIT == 2)
void xil_lzma_cu2
#endif
(
                const uint512_t *in,      
                uint512_t       *out,
                uint512_t       *dict_buff1,
                //uint512_t       *dict_buff2,
                //uint512_t       *dict_buff3,
                uint32_t        *compressd_size,
                uint32_t        *in_block_size,
                uint32_t block_size_in_kb,
                uint32_t input_size,
                uint32_t dataprocessed          
                )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=dict_buff1 offset=slave bundle=gmem2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16 latency=32
    //#pragma HLS INTERFACE m_axi port=dict_buff2 offset=slave bundle=gmem3 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16 latency=32
    //#pragma HLS INTERFACE m_axi port=dict_buff3 offset=slave bundle=gmem4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16 latency=32
    #pragma HLS INTERFACE m_axi port=compressd_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=in_block_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=dict_buff1 bundle=control
    //#pragma HLS INTERFACE s_axilite port=dict_buff2 bundle=control
    //#pragma HLS INTERFACE s_axilite port=dict_buff3 bundle=control
    #pragma HLS INTERFACE s_axilite port=compressd_size bundle=control
    #pragma HLS INTERFACE s_axilite port=in_block_size bundle=control
    #pragma HLS INTERFACE s_axilite port=block_size_in_kb bundle=control
    #pragma HLS INTERFACE s_axilite port=input_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    #pragma HLS INTERFACE s_axilite port=dataprocessed bundle=control

    #pragma HLS data_pack variable=in
    #pragma HLS data_pack variable=out
    #pragma HLS data_pack variable=dict_buff1
    //#pragma HLS data_pack variable=dict_buff2
    //#pragma HLS data_pack variable=dict_buff3
    int block_idx = 0;
    int block_length = block_size_in_kb * 1024;
    int no_blocks = (input_size - 1) / block_length + 1;
    uint32_t max_block_size =  block_size_in_kb * 1024;
    
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
	//printf("=======================================================================dataprocessed:%u\n",dataprocessed);
/*	
	if(dataprocessed == 0) {
		for(uint32_t a1 = 0;a1<16384;++a1) {
			//#pragma HLS PIPELINE II=2
			for(uint32_t a2 = 0;a2<1024;++a2) {
				//#pragma HLS UNROLL
				dict_buff1[(a1 << 10)+ a2] = -1;
//				dict_buff2[(a1 << 10)+ a2] = -1;
			}
		}
	}
*/
    uint32_t last_index = 0;
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
        last_index = i*max_block_size;
        last_index += dataprocessed;
        cu1::lzma(in,out,dict_buff1,/*dict_buff2,dict_buff3,*/input_idx,output_idx,input_block_size,output_block_size,max_lit_limit,last_index);
#elif (C_COMPUTE_UNIT == 2)
        cu2::lzma(in,out,input_idx,output_idx,input_block_size,output_block_size,max_lit_limit);
#endif
        for(int k = 0; k < nblocks; k++) {
            compressd_size[block_idx] = output_block_size[k]; 
            if (small_block[k] == 1) {
                compressd_size[block_idx] = small_block_inSize[k];       
            }
            block_idx++;
        }
    }

}
}
