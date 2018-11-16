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

//LZ4 Compress STATES
#define WRITE_TOKEN         0
#define WRITE_LIT_LEN       1
#define WRITE_MATCH_LEN     2
#define WRITE_LITERAL       3
#define WRITE_OFFSET0       4
#define WRITE_OFFSET1       5

typedef ap_uint<32> compressd_dt;
typedef ap_uint<64> lz4_compressd_dt;

inline void lz4_compress(hls::stream<uint8_t> &in_lit_inStream,
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

inline void lz4_divide(
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
