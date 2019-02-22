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

//SNAPPY Compress STATES
#define WRITE_TOKEN          0
#define WRITE_OFFSET	     1	     	
#define WRITE_LITERAL        2
#define WRITE_LITLEN_60      3
#define WRITE_LITLEN_61      4
#define WRITE_LITLEN_61_B1   5
#define WRITE_OFFSET_C01     6
#define WRITE_OFFSET_C10     7
#define WRITE_OFFSET_C10_B1  8

#define INSIZE_RANGE_7BIT (1<<7) // 128
#define INSIZE_RANGE_14BIT (1<<14) // 16384
#define INSIZE_RANGE_21BIT (1<<21) // 2097152

typedef ap_uint<32> compressd_dt;
typedef ap_uint<64> snappy_compressd_dt;

void snappy_compress(hls::stream<uint8_t> &in_lit_inStream,
                  hls::stream<snappy_compressd_dt> &in_lenOffset_Stream,
                  hls::stream<ap_uint<8> > &outStream,
                  hls::stream<uint16_t> &outStreamSize,
                  uint32_t input_size
                 ) {
    if(input_size == 0) { outStreamSize << 0; return; }

    ap_uint<10> outCntr =0; // outCntr can max be 512
    uint32_t outSize =0;
    uint8_t next_state = WRITE_TOKEN;
    ap_uint<16> litSecLength = 0;
    uint16_t match_length = 0;
    uint16_t outLitLen = 0;
    ap_uint<16> match_offset = 0;
    ap_uint<8> outInSize = 0; 
    ap_uint<32> blkInputSize = input_size;
    bool lit_ending = false;

    // Step - 1 := Setup Preamble for this block
    // Stream starts with the uncompressed length 
    // This can be of series of bytes, major
    // Criteria to define these bytes is
    // lower 7 bits contains data
    // top 1 bit is set if more bytes needed
    // otherwise set to 0
    // Example: 
    //      a. 64 would be represented as 0x40
    //      b. 2097150 (0x1FFFFE) woud be represented
    //         0xFE 0XFF 0X7F

    assert(input_size < INSIZE_RANGE_21BIT);
    if (input_size>= INSIZE_RANGE_14BIT){
        // If input size range is greater than 14bit (16384) but less than 21bit (2097152)
        // --- 3 bytes needed
        outInSize.range(6,0) = blkInputSize.range(6,0);
        outInSize.range(7,7) = 1;
        outStream << outInSize;
        outCntr++;
        outSize++;
        outInSize.range (6,0) = blkInputSize.range(13,7);
        outInSize.range(7,7) = 1;
        outStream << outInSize;
        outCntr++;
        outSize++;
        outInSize = blkInputSize.range(21,14);
        outStream << outInSize;
        outCntr++;
        outSize++;
    } else if (input_size>=INSIZE_RANGE_7BIT){
        // If input size range is greater than 7bit (128) but less than 14bit (16384)  
        // --- 2 bytes needed
        outInSize.range(6,0) = blkInputSize.range(6,0);
        outInSize.range(7,7) = 1;
        outStream << outInSize;
        outCntr++;
        outSize++;
        outInSize = blkInputSize.range(14,7);
        outStream << outInSize;
        outCntr++;
        outSize++;
    } else {
        // If input size is less than 128 = 7bit 
        // --- 1 byte is enough
        outInSize = blkInputSize.range(7,0);
        outStream << outInSize;
        outCntr++;
        outSize++;
    }

    // Read lenOffset stream to begin processing literal and sequence sections
    snappy_compressd_dt tmpValue = in_lenOffset_Stream.read();

    // Main loop which process the lz_compress data 
    // does the encoding based on compress format specificaiton of Snappy
    snappy_compress:for (uint32_t inIdx = 0 ; (inIdx < input_size) || (next_state != WRITE_TOKEN); ){
    #pragma HLS PIPELINE II=1 
        ap_uint<8> outValue;
        bool flagOutWrite = true;

        if (next_state == WRITE_TOKEN) {
            
            // Find the lieral section length
            // Number of uncompressed literal length
            litSecLength  = tmpValue.range(63,32);
		    
            match_length    = tmpValue.range(15,0);
            match_offset    = tmpValue.range(31,16);

            // Add match length and literal section
            // to reach upto input_size - Loop termination cond.,
            inIdx += match_length + litSecLength;
          
            // This variable tracks literal length
            // Each time we push out literals to outstream
            // this variable is decremented 
            outLitLen = litSecLength;
			
            // If match offset and length are set to 0
            // it means we are in literal section
            if ((match_offset == 0)  && (match_length == 0)) {
                lit_ending= true;
            }	
        
            // If literal section length is 0
            // it means we will process sequence section
            if (litSecLength == 0) {
                next_state = WRITE_OFFSET;
                flagOutWrite = false;
            } else if ((litSecLength > 0) && ( litSecLength <= 60)){
                // If litseclength is <= 60, then represent 
                // lower bit is set to 0 and upper 6 bits are used
                // to represent literal length
                outValue.range(1,0) = 0;
                outValue.range(7,2) = litSecLength - 1;
                // This state shift enables reading literals from stream
                // and writing them into output stream, more details look
                // WRITE_LITERAL state
                next_state = WRITE_LITERAL;
            }
#ifdef LARGE_LIT_RANGE 
            else if ((litSecLength > 60) && (litSecLength < 256)) {
                // if litseclength is more than 60 but less than 256
                // go ahead with 2 byte literal length representation
                // Literal section flag byte starts with 1st bit 00
                // Since literal length is more than 60 use extra byte
                outValue.range(1,0) = 0;
                outValue.range(7,2) = 60;
                litSecLength = litSecLength - 1;
                next_state = WRITE_LITLEN_60;
            } else if ((litSecLength > 255) && ( litSecLength < 65536)) {
                outValue.range(1,0) = 0;
                outValue.range(7,2) = 61;
                litSecLength = litSecLength - 1;
                next_state = WRITE_LITLEN_61;
            }
#endif
            // Send match length and offset details 
        } else if (next_state == WRITE_OFFSET) {	
            // Case 1: 1-byte offset
            // As per spec if the match length <=11 and matc_offsets are <=2047
            // Tag byte:
            // [0,1] bits are set to 1
            // [2,4] bits are set to len-4 [For examples if your match_length = 6,
            // [6,4] = 2 will be set]
            // [5,7] - Offset occupies 11 bits, of which upper three are store
            // in in tag byte at these locations
            // lower eight bits are stored in next byte which succedes tag byte
            if ((match_length <= 11) && (match_offset <= 2047) ) {
                outValue.range(1,0) = 1;
                outValue.range(4,2) = match_length - 4;
                outValue.range(7,5) = match_offset.range(11,8);
                // Move to next state where lower 8 bits of offset 
                // will be put up in a brand new byte following offset tag byte
                // Extra 1 byte: look into WRITE_OFFSET_C01 state
                next_state = WRITE_OFFSET_C01;
               // Case 2: 2 - byte offset
               // As per spec if the match lengths are <= 64
               // offsets are <= 65535
               // Tag byte:
               // [0,1]  bits are set to 2
               // [2,7]  bits are set to match length
               // Next 2 bytes --> 16bit integer little endian following tag
               // byte contains 2 bytes of offset value
            } else if (((match_offset >= 2048) && (match_offset <= 65535 ) &&  (match_length <= 64)) || 
                        ((match_length > 11) && (match_length <= 64) && (match_offset <= 2047)) 
                      ) {
                outValue.range(1,0) = 2;
                outValue.range(7,2) = match_length - 1;
                next_state = WRITE_OFFSET_C10;
            } else { 
                next_state = WRITE_TOKEN;
                if(inIdx < input_size)
	                tmpValue = in_lenOffset_Stream.read();
            }
            
        } else if ( next_state == WRITE_LITERAL) {  
            // Read literals, state continues until
            // all literals are consumed
            // Once all literals are consumed
            // next state would be offset related
            // If it is literal ending then go back to
            // write token state
            outValue = in_lit_inStream.read();
            outLitLen--;
            if (outLitLen == 0) {
                if(lit_ending) { 
                    next_state = WRITE_TOKEN;
                    lit_ending = false;
                    if(inIdx < input_size)
	                    tmpValue = in_lenOffset_Stream.read();
                } else 		
                    next_state = WRITE_OFFSET;
            }
        }
#ifdef LARGE_LIT_RANGE
        else if (next_state == WRITE_LITLEN_60) {
            outValue = litSecLength.range (7,0); 
            next_state = WRITE_LITERAL;
        } else if (next_state == WRITE_LITLEN_61) {
            outValue = litSecLength.range (7,0);
            next_state = WRITE_LITLEN_61_B1;
        } else if(next_state == WRITE_LITLEN_61_B1) {
            outValue = litSecLength.range (15,8) ;
            next_state = WRITE_LITERAL;
        }
#endif
        else if (next_state == WRITE_OFFSET_C01) {
            outValue = match_offset.range(7,0);
            next_state = WRITE_TOKEN;
            if(inIdx < input_size)
	            tmpValue = in_lenOffset_Stream.read();
        } else if( next_state == WRITE_OFFSET_C10) {
            outValue = match_offset.range(7,0);
            next_state = WRITE_OFFSET_C10_B1;
        } else if(next_state == WRITE_OFFSET_C10_B1) {
            outValue = match_offset.range (15,8);
            next_state = WRITE_TOKEN; 
            if(inIdx < input_size)
	            tmpValue = in_lenOffset_Stream.read();
        } 
        
        // Send output put every cycle
        // Skip the 
        if (flagOutWrite && outSize<input_size) {
            outStream << outValue;
            outCntr++;
            outSize++;
        }

        if (outCntr >= 512){
            outStreamSize << outCntr;
            outCntr = 0 ;
        }
    } // Main FOR LOOP ENDS here


    if (outCntr) 
        outStreamSize << outCntr;
    
    outStreamSize << 0; 
}

inline void snappy_divide(
            hls::stream<compressd_dt> &inStream,   
            hls::stream<uint8_t> &lit_outStream,
            hls::stream<snappy_compressd_dt> &lenOffset_Stream,
            uint32_t input_size,
            uint32_t max_lit_limit[PARALLEL_BLOCK],
            uint32_t index       
          )
{
    if(input_size == 0) return;
    assert(max_literal_count < max_literal_stream);
    uint8_t marker = MARKER;
    uint32_t out_idx = 0;
    uint8_t match_len= 0;
    uint32_t loc_idx = 0;
    uint32_t lit_count = 0;
    uint32_t lit_count_flag = 0;

    compressd_dt nextEncodedValue = inStream.read();
    snappy_divide:for (uint32_t i = 0; i < input_size; i++) {
    #pragma HLS PIPELINE II=1 
        compressd_dt tmpEncodedValue = nextEncodedValue;
        if(i < (input_size - 1)) nextEncodedValue = inStream.read();
        uint8_t tCh  = tmpEncodedValue.range(7,0);
        uint8_t tLen = tmpEncodedValue.range(15,8);
        uint16_t tOffset = tmpEncodedValue.range(31,16);
        uint32_t match_offset = tOffset + 1;

        if (tLen > 0) {
            uint8_t match_len= tLen; // Snappy standard 
            snappy_compressd_dt tmpValue;
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
            if(lit_count == max_literal_count) {
                snappy_compressd_dt tmpValue;
                tmpValue.range(63,32)   = lit_count;
                tmpValue.range(15,0)    = 0;
                tmpValue.range(31,16)   = 0;
                lenOffset_Stream << tmpValue;
                lit_count = 0;
            }
        }
    } 

    if (lit_count) {
        snappy_compressd_dt tmpValue;
        tmpValue.range(63,32)   = lit_count; 
        
        if (lit_count == max_literal_count) {
            lit_count_flag = 1;
            tmpValue.range(15,0)    = 0;
            tmpValue.range(31,16)   = 0;
        } else {
            tmpValue.range(15,0)    = 0;
            tmpValue.range(31,16)   = 0;
        }
        lenOffset_Stream << tmpValue;
    }
    
    max_lit_limit[index] = lit_count_flag;	
}
