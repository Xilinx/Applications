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

typedef ap_uint<32> compressd_dt;

//LZ4 Decompress states
#define READ_TOKEN      0
#define READ_LIT_LEN    1
#define READ_LITERAL    2
#define READ_OFFSET0    3
#define READ_OFFSET1    4
#define READ_MATCH_LEN  5

inline void lz4_decompressr(
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
