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

#define MIN_BLOCK_SIZE 128

//LZ4 Compress STATES
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
#define LZ_MAX_OFFSET_LIMIT 65536
#define LZ_HASH_BIT 12
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT)
#define LZ_MAX_OFFSET_LIMIT 65536
//#define MATCH_LEN (1*VEC)
#define MAX_MATCH_LEN 255
#define OFFSET_WINDOW 65536
#define MATCH_LEN 6 
#define MIN_MATCH 4
typedef ap_uint<VEC * BIT> uintV_t;
typedef ap_uint<MATCH_LEN * BIT> uintMatchV_t;
#define MATCH_LEVEL 6
#define DICT_ELE_WIDTH (MATCH_LEN*BIT + 24)

typedef ap_uint<DICT_ELE_WIDTH> uintDict_t;
typedef ap_uint< MATCH_LEVEL * DICT_ELE_WIDTH> uintDictV_t;

#define OUT_BYTES (4) 
typedef ap_uint< OUT_BYTES * BIT> uintOut_t;
typedef ap_uint< 2  * OUT_BYTES * BIT> uintDoubleOut_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> compressd_dt;
typedef ap_uint<VEC*32> compressdV_dt;
typedef ap_uint<64> lz4_compressd_dt;

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#endif
{
void lz4_compress(hls::stream<uint8_t> &in_lit_inStream,
                  hls::stream<lz4_compressd_dt> &in_lenOffset_Stream,
                  hls::stream<ap_uint<8> > &outStream,
                  hls::stream<uint16_t> &outStreamSize,
                  uint32_t input_size
                 ) {

    uint32_t lit_len = 0;
    uint16_t outCntr=0;
    uint32_t compressedSize=0;
    uint8_t next_state=WRITE_TOKEN;
    uint16_t lit_length     = 0;
    uint16_t match_length   = 0;
    uint16_t write_lit_length = 0;
    ap_uint<16> match_offset   = 0;
    bool lit_ending=false;
    bool extra_match_len=false;
    
    lz4_compress:for (uint32_t inIdx = 0 ; (inIdx < input_size) || (next_state != WRITE_TOKEN);){
        #pragma HLS PIPELINE II=1 
        ap_uint<8> outValue;
        if (next_state == WRITE_TOKEN){
            lz4_compressd_dt tmpValue = in_lenOffset_Stream.read();
            lit_length      = tmpValue.range(63,32);
            match_length    = tmpValue.range(15,0);
            match_offset    = tmpValue.range(31,16);
            inIdx += match_length + lit_length + 4;
 
            if (match_length == 777 && match_offset == 777) {
                inIdx = input_size;
                lit_ending = true;
            }

            lit_len = lit_length;
            write_lit_length = lit_length;
            if (match_offset == 0  && match_length == 0){
                lit_ending= true;
            }
            if (lit_length >= 15){
                outValue.range(7,4) = 15;
                lit_length -=15;
                next_state = WRITE_LIT_LEN;
            }else if (lit_length){
                outValue.range(7,4) = lit_length;
                lit_length = 0;
                next_state = WRITE_LITERAL;
            }else{
                outValue.range(7,4) = 0;
                next_state = WRITE_OFFSET0;
            }
            if (match_length >= 15){
                outValue.range(3,0) = 15;
                match_length -=15;
                extra_match_len=true;
            }else{
                outValue.range(3,0) = match_length;
                match_length = 0;
                extra_match_len = false;
            }
        }else if (next_state == WRITE_LIT_LEN){
            if (lit_length >= 255){
                outValue = 255;
                lit_length -=255;
            }else{
                outValue    = lit_length;
                next_state  = WRITE_LITERAL; 
            }
        }else if (next_state == WRITE_LITERAL){
            outValue = in_lit_inStream.read();
            write_lit_length--;
            if (write_lit_length == 0){
                if (lit_ending){
                    next_state = WRITE_TOKEN;
                }else{
                    next_state = WRITE_OFFSET0;
                }
            }
        }else if (next_state == WRITE_OFFSET0) {
            match_offset++; //LZ4 standard
            outValue = match_offset.range(7,0); 
            next_state = WRITE_OFFSET1;
        }else if (next_state == WRITE_OFFSET1) {
            outValue = match_offset.range(15,8); 
            if (extra_match_len){
                next_state = WRITE_MATCH_LEN;
            }else{
                next_state = WRITE_TOKEN;
            }
        }else if (next_state == WRITE_MATCH_LEN){
            if (match_length >= 255){
                outValue     = 255;
                match_length -= 255;
            }else{
                outValue    = match_length;
                next_state  = WRITE_TOKEN;
            }
        }
        if (compressedSize < input_size){
            //Limiting compression size not more than input size.
            //Host code should ignore such blocks
            outStream << outValue;
            outCntr++;
            compressedSize++;
        }
        if (outCntr >= 512){
            outStreamSize << outCntr;
            outCntr = 0 ;
        }
    }
   if (outCntr) outStreamSize << outCntr;
    outStreamSize << 0;
}

void lz4_divide(
                hls::stream<compressd_dt> &inStream,   
                hls::stream<uint8_t> &lit_outStream,
                hls::stream<lz4_compressd_dt> &lenOffset_Stream,
                uint32_t input_size,
				uint32_t max_lit_limit[PARALLEL_BLOCK],
				uint32_t index       
              )
{
    if(input_size == 0) return;
    
    uint8_t marker = MARKER;
    uint32_t out_idx = 0;
    uint8_t match_len= 0;
    uint32_t loc_idx = 0;
    uint32_t lit_count = 0;
    uint32_t lit_count_flag = 0;


    compressd_dt nextEncodedValue = inStream.read();
    lz4_divide:for (uint32_t i = 0; i < input_size; i++) {
    #pragma HLS PIPELINE II=1 
        compressd_dt tmpEncodedValue = nextEncodedValue;
        if(i < (input_size - 1)) nextEncodedValue = inStream.read();
        uint8_t tCh  = tmpEncodedValue.range(7,0);
        uint8_t tLen = tmpEncodedValue.range(15,8);
        uint16_t tOffset = tmpEncodedValue.range(31,16);
        uint32_t match_offset = tOffset;

        if (lit_count == max_literal_count) {
            if(tLen > 0) {
                uint8_t match_len = tLen - 1;
                i += match_len;
            }
            continue;
        }        
 
        if (tLen > 0) {
            uint8_t match_len= tLen - 4; // LZ4 standard 
            lz4_compressd_dt tmpValue;
            tmpValue.range(63,32)   = lit_count; 
            tmpValue.range(15,0)    = match_len;
            tmpValue.range(31,16)   = match_offset;
            lenOffset_Stream << tmpValue; 
            match_len= tLen - 1;
            i +=match_len;
            lit_count = 0;
        }
        else {
            lit_outStream << tCh; 
            lit_count++;
        }
    } 
 
    if (lit_count) {
        lz4_compressd_dt tmpValue;
        tmpValue.range(63,32)   = lit_count; 

        if (lit_count == max_literal_count) {
            lit_count_flag = 1;
            tmpValue.range(15,0)    = 777;
            tmpValue.range(31,16)   = 777;
        } else {
            tmpValue.range(15,0)    = 0;
            tmpValue.range(31,16)   = 0;
        }

        lenOffset_Stream << tmpValue;
    }
	max_lit_limit[index] = lit_count_flag;	
}

void lz4_core(
        hls::stream<uint512_t> &inStream512,
        hls::stream<uint512_t> &outStream512,
        hls::stream<uint16_t> &outStream512Size,
        uint32_t max_lit_limit[PARALLEL_BLOCK],
        uint32_t input_size,
        uint32_t core_idx
        )
{
    uint32_t left_bytes = 64;
    hls::stream<ap_uint<BIT> >       inStream("inStream");
    hls::stream<compressd_dt>      compressdStream("compressdStream");
    hls::stream<compressd_dt>      boosterStream("boosterStream");
    hls::stream<uint8_t>            litOut("litOut");
    hls::stream<lz4_compressd_dt>  lenOffsetOut("lenOffsetOut");
    hls::stream<ap_uint<8> >         lz4Out("lz4Out");
    hls::stream<uint16_t>           lz4OutSize("lz4OutSize");
    #pragma HLS STREAM variable=inStream            depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream    depth=c_gmem_burst_size
    #pragma HLS STREAM variable=boosterStream       depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut              depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out              depth=1024
    #pragma HLS STREAM variable=lz4OutSize          depth=c_gmem_burst_size

    #pragma HLS RESOURCE variable=inStream            core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream     core=FIFO_SRL
    #pragma HLS RESOURCE variable=boosterStream       core=FIFO_SRL
    #pragma HLS RESOURCE variable=litOut              core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut        core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out              core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize          core=FIFO_SRL

#pragma HLS dataflow
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512,inStream,input_size);
    lz_compress<MATCH_LEN,MATCH_LEVEL,LZ_DICT_SIZE,BIT,MIN_OFFSET,MIN_MATCH,LZ_MAX_OFFSET_LIMIT>(inStream, compressdStream, input_size,left_bytes);
    lz_booster<MAX_MATCH_LEN, OFFSET_WINDOW>(compressdStream, boosterStream,input_size,left_bytes);
    lz4_divide(boosterStream, litOut, lenOffsetOut, input_size, max_lit_limit, core_idx);
    lz4_compress(litOut,lenOffsetOut, lz4Out,lz4OutSize,input_size);
    upsizer_sizestream<uint16_t, BIT, GMEM_DWIDTH>(lz4Out,lz4OutSize,outStream512,outStream512Size);
}

void lz4(
                const uint512_t *in,      
                uint512_t       *out,          
                const uint32_t  input_idx[PARALLEL_BLOCK],                      
                const uint32_t  output_idx[PARALLEL_BLOCK],                      
                const uint32_t  input_size[PARALLEL_BLOCK],
                uint32_t        output_size[PARALLEL_BLOCK],
                uint32_t        max_lit_limit[PARALLEL_BLOCK]
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
    lz4_core(inStream512_0,outStream512_0,outStream512Size_0,max_lit_limit,input_size[0],0);
#if PARALLEL_BLOCK > 1
    lz4_core(inStream512_1,outStream512_1,outStream512Size_1,max_lit_limit,input_size[1],1);
#endif
#if PARALLEL_BLOCK > 2
    lz4_core(inStream512_2,outStream512_2,outStream512Size_2,max_lit_limit,input_size[2],2);
    lz4_core(inStream512_3,outStream512_3,outStream512Size_3,max_lit_limit,input_size[3],3);
#endif
#if PARALLEL_BLOCK > 4
    lz4_core(inStream512_4,outStream512_4,outStream512Size_4,max_lit_limit,input_size[4],4);
    lz4_core(inStream512_5,outStream512_5,outStream512Size_5,max_lit_limit,input_size[5],5);
    lz4_core(inStream512_6,outStream512_6,outStream512Size_6,max_lit_limit,input_size[6],6);
    lz4_core(inStream512_7,outStream512_7,outStream512Size_7,max_lit_limit,input_size[7],7);
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
void xil_lz4_cu1
#elif (C_COMPUTE_UNIT == 2)
void xil_lz4_cu2
#endif
(
                const uint512_t *in,      
                uint512_t       *out,          
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
