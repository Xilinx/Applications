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
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include "stream_utils.h"
#include "lz_compress.h"
#include <ap_int.h>
#include "xil_lzma_config.h"
#include "xil_lzma_rc_kernel.h"
#include "xil_lzma_stream_utils.h"

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
#define LZ_HASH_BIT 26
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT)
#define LZ_MAX_OFFSET_LIMIT 0x40000000
#define MAX_MATCH_LEN 255
#define OFFSET_WINDOW 0x40000000
#define MATCH_LEN 12 
#define MATCH_LEVEL 4 

typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<64> compressd_dt;

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#elif (C_COMPUTE_UNIT == 3)
namespace cu3
#elif (C_COMPUTE_UNIT == 4)
namespace cu4
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
    hls::stream<compressd_dt>      crImprover959to999Stream("crImprover959to999Stream");
    hls::stream<compressd_dt>      crImprover965to987Stream("crImprover965to987Stream");

    hls::stream<compressd_dt>      boosterStream("boosterStream");
    hls::stream<compressd_dt>      filterStream("filterStream");

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
    #pragma HLS STREAM variable=crImprover959to999Stream    depth=c_gmem_burst_size
    #pragma HLS STREAM variable=crImprover965to987Stream    depth=c_gmem_burst_size
    #pragma HLS STREAM variable=boosterStream        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filterStream        depth=c_gmem_burst_size
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
    #pragma HLS RESOURCE variable=filterrStream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=crImprover959to999Stream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=crImprover965to987Stream       core=FIFO_SRL
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
    lz_cr_improve_959to999<MATCH_LEN, OFFSET_WINDOW>(compressdStream, crImprover959to999Stream, input_size, left_bytes);
    lz_cr_improve_965to987<MATCH_LEN, OFFSET_WINDOW>(crImprover959to999Stream, crImprover965to987Stream, input_size, left_bytes);
    lz_booster_999to11109<MAX_MATCH_LEN, OFFSET_WINDOW, MATCH_LEN>(crImprover965to987Stream, boosterStream,input_size,left_bytes);
    //lz_booster_999to11<MAX_MATCH_LEN, OFFSET_WINDOW, MATCH_LEN>(crImprover965to987Stream, boosterStream,input_size,left_bytes,compOutSize);
    lz_filter(boosterStream, filterStream,input_size,left_bytes,compOutSize);

    lzma_rc_1(filterStream,compOutSize, symStream64,probsStream64,outStreamSize64, input_size,last_index);
    //lzma_rc_1_1(boosterStream,compOutSize, packStream,packsizeStream, input_size,last_index);
    //lzma_rc_1_2(packStream,packsizeStream,symStream64,probsStream64,outStreamSize64);
    lzma_rc_converter(symStream64,probsStream64,outStreamSize64,symStream,probsStream,outStreamSize);
    lzma_rc_2(symStream,probsStream,outStreamSize,rangeStream,lowStream,outStreamSize2);
    lzma_rc_3(rangeStream,lowStream,outStreamSize2,rcStream,rcOutSize,input_size);
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
    hls::stream<uint512_t>   inStream512[PARALLEL_BLOCK];
    hls::stream<uint16_t >  outStream512Size[PARALLEL_BLOCK];
    hls::stream<uint512_t>   outStream512[PARALLEL_BLOCK];
    #pragma HLS STREAM variable=outStream512Size      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512          depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512           core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512          core=FIFO_SRL

    uint32_t left_bytes = 64;

    #pragma HLS dataflow
    mm2s_nb<GMEM_DWIDTH, GMEM_BURST_SIZE,PARALLEL_BLOCK>(in,
                                       input_idx,
                                       inStream512, 
                                       input_size
                                      );
    for(uint8_t i = 0; i < PARALLEL_BLOCK; i++){
        #pragma HLS UNROLL
        lzma_core(inStream512[i],dict_buff1,/*dict_buff2,dict_buff3,*/max_lit_limit,input_size[i],i,outStream512[i],outStream512Size[i],last_index);
    }

    s2mm_compress<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
                                                   output_idx,
                                                   outStream512,
                                                   outStream512Size,
                                                   output_size
                                                  );

}
} // End of namespace

extern "C" {
#if (C_COMPUTE_UNIT == 1)
void xil_lzma_cu1
#elif (C_COMPUTE_UNIT == 2)
void xil_lzma_cu2
#elif (C_COMPUTE_UNIT == 3)
void xil_lzma_cu3
#elif (C_COMPUTE_UNIT == 4)
void xil_lzma_cu4
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
	
	if(dataprocessed == 0) {
		for(uint32_t a1 = 0;a1<LZ_DICT_SIZE;++a1) {
				dict_buff1[a1] = -1;
			}
	}
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
        last_index = i*max_block_size;
        last_index += dataprocessed;
#if (C_COMPUTE_UNIT == 1)
        cu1::lzma(in,out,dict_buff1,/*dict_buff2,dict_buff3,*/input_idx,output_idx,input_block_size,output_block_size,max_lit_limit,last_index);
#elif (C_COMPUTE_UNIT == 2)
        cu2::lzma(in,out,dict_buff1,/*dict_buff2,dict_buff3,*/input_idx,output_idx,input_block_size,output_block_size,max_lit_limit,last_index);
#elif (C_COMPUTE_UNIT == 3)
        cu3::lzma(in,out,dict_buff1,/*dict_buff2,dict_buff3,*/input_idx,output_idx,input_block_size,output_block_size,max_lit_limit,last_index);
#elif (C_COMPUTE_UNIT == 4)
        cu4::lzma(in,out,dict_buff1,/*dict_buff2,dict_buff3,*/input_idx,output_idx,input_block_size,output_block_size,max_lit_limit,last_index);
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
