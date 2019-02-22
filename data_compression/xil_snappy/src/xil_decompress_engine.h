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
#include "xil_snappy_config.h"

typedef ap_uint<32> compressd_dt;

/////Snappy Decoder states
#define READ_STATE          0
#define MATCH_STATE         1
#define LOW_OFFSET_STATE    2 
#define READ_TOKEN          3
#define READ_LITERAL        4
#define READ_LITLEN_60      5
#define READ_LITLEN_61      6
#define READ_OFFSET         7  
#define READ_OFFSET_C01     8
#define READ_OFFSET_C10     9
#define READ_LITLEN_61_CONT      10
#define READ_OFFSET_C10_CONT     11

#define BIT 8
#define LOW_OFFSET 8
typedef ap_uint<BIT> uintV_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> encoded_dt;

void snappy_decompressr(hls::stream<uintV_t> &inStream,          
                        hls::stream<encoded_dt> &outStream,          
                        uint32_t input_size
                       )
{
    if(input_size == 0)
        return;

    uint8_t next_state = READ_TOKEN;
    ap_uint<8> nextValue;
    ap_uint<16> offset;
    encoded_dt decodedOut=0;
    ap_uint <32> lit_len = 0;
    uint32_t match_len = 0;
    ap_uint <8> inValue = 0;
    bool read_instream = true;

    uint32_t inCntr_idx =0;
    ap_uint <32> inBlkSize = 0;

    inValue = inStream.read();
    inCntr_idx++;
    
    if ((inValue >> 7 ) == 1) {
	    inBlkSize.range(6,0) = inValue.range (6,0);
        inValue = inStream.read();
	    inCntr_idx++;
        inBlkSize.range(13,7) = inValue.range(6,0);
        if ((inValue >> 7) == 1) {
            inValue = inStream.read();
            inCntr_idx++;
	        inBlkSize.range (20,14) = inValue.range(6,0);
	    }
	    
    } else 
	    inBlkSize = inValue;

    snappy_decompress: for ( ; inCntr_idx < input_size ; inCntr_idx++){
    #pragma HLS PIPELINE II=1
	    if (read_instream)
            inValue = inStream.read(); 
        else 
            inCntr_idx--;
	
        read_instream = true;
        if (next_state == READ_TOKEN) {
		
            if( inValue.range(1,0) != 0) {	
                next_state = READ_OFFSET;
                read_instream = false;
			} else {	
            
                lit_len = inValue.range(7,2);
                
                if (lit_len < 60){ 
                    lit_len++;
                    next_state = READ_LITERAL; 
                }
                else if (lit_len == 60) {
                    next_state = READ_LITLEN_60;
                }
                else if (lit_len == 61) {
                    next_state = READ_LITLEN_61; 
                }
            }
        } else if (next_state == READ_LITERAL) {
	        encoded_dt outValue = 0;
            outValue.range(7,0) = inValue;
            outStream << outValue;
            lit_len--;
            if (lit_len == 0)
                next_state = READ_TOKEN; 
	    
        } else if (next_state == READ_OFFSET) {
		    
            offset = 0;
            if (inValue.range(1,0) == 1) {	  
			  match_len  = inValue.range(4,2);
			  offset.range(10,8) = inValue.range(7,5);
			  next_state = READ_OFFSET_C01;
            } else if ( inValue.range(1,0) == 2) {
			    match_len = inValue.range(7,2);
			    next_state = READ_OFFSET_C10;
            } else  {
			    next_state = READ_TOKEN;
			    read_instream = false;
            }
        } else if (next_state == READ_OFFSET_C01) {	
            offset.range(7,0) = inValue;
            encoded_dt outValue = 0;
		    outValue.range(31,16) = match_len + 3;
            outValue.range(15,0)  = offset - 1 ;
            outStream << outValue;
		    next_state = READ_TOKEN;
        } else if (next_state == READ_OFFSET_C10) {	
		    offset.range(7,0) = inValue;
            next_state = READ_OFFSET_C10_CONT;
        } else if (next_state == READ_OFFSET_C10_CONT) {
		    
            offset.range(15,8) = inValue;	
            encoded_dt outValue = 0;	
		
            outValue.range(31,16) = match_len ;
		    outValue.range(15,0)  = offset - 1;
            outStream << outValue;
		    next_state = READ_TOKEN;

        } else if (next_state == READ_LITLEN_60) {	
            lit_len = inValue + 1;
            next_state = READ_LITERAL;
        } else if (next_state == READ_LITLEN_61) {
            lit_len.range(7,0) = inValue;
            next_state = READ_LITLEN_61_CONT;
        } else if (next_state == READ_LITLEN_61_CONT) {
            lit_len.range(15,8) = inValue;
            lit_len = lit_len + 1;
            next_state = READ_LITERAL;
        } 
    } // End of main snappy_decoder for-loop 
}
