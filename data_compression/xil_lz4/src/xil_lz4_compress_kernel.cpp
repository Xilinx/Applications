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

#define MM2S_IF_NOT_FULL(outStream,bIdx)\
        is_full.range(bIdx,bIdx) = outStream.full();\
            if(!is_full.range(bIdx,bIdx) && (read_idx[bIdx] != write_idx[bIdx])){\
                outStream << local_buffer[bIdx][read_idx[bIdx]];\
                read_idx[bIdx] += 1;\
            }

#define S2MM_READ_SIZE(i,instream,end_of_stream)\
        if (!end_of_stream.range(i,i) && !instream.empty()){ \
            uint16_t tmpValue = instream.read(); \
            input_size[i] += tmpValue; \
            if(tmpValue == 0) end_of_stream.range(i,i) = 1; \
        }

#define S2MM_IF_NOT_EMPTY(i,instream,burst_size,input_size,read_size,write_size,write_idx) \
            burst_size[i] = c_max_burst_size; \
            if(end_of_stream.range(i,i) && ((input_size[i] - write_size[i]) < burst_size[i])){ \
                burst_size[i] = GET_DIFF_IF_BIG(input_size[i],write_size[i]); \
            } \
            if(((read_size[i] - write_size[i]) < burst_size[i]) && (input_size[i] > read_size[i])){ \
                bool is_empty = instream.empty(); \
                if(!is_empty){ \
                    local_buffer[i][write_idx[i]] = instream.read(); \
                    write_idx[i] += 1; \
                    read_size[i] += 64; \
                    is_pending.range(i,i) = true;\
                }else{ \
                    is_pending.range(i,i) = false; \
                }\
            } \
            else{ \
                if(burst_size[i]) done = true; \
                if (read_size[i] >= input_size[i]) is_pending.range(i,i) = false; \
            }


//LZ specific Defines
#define BIT 8
#define LZ_HASH_BIT 12
#define LZ_DICT_SIZE (1 << LZ_HASH_BIT)
#define LZ_MAX_OFFSET_LIMIT 65536
//#define MATCH_LEN (1*VEC)
#define MATCH_LEN 23
#define MIN_MATCH 4
#define MIN_OFFSET 1
typedef ap_uint<VEC * BIT> uintV_t;
typedef ap_uint<MATCH_LEN * BIT> uintMatchV_t;
//#define MATCH_LEVEL 4
#define MATCH_LEVEL 2
#define DICT_ELE_WIDTH (MATCH_LEN*BIT + 32)

typedef ap_uint<DICT_ELE_WIDTH> uintDict_t;
typedef ap_uint< MATCH_LEVEL * DICT_ELE_WIDTH> uintDictV_t;

#define OUT_BYTES (4) 
typedef ap_uint< OUT_BYTES * BIT> uintOut_t;
typedef ap_uint< 2  * OUT_BYTES * BIT> uintDoubleOut_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> compressd_dt;
typedef ap_uint<VEC*32> compressdV_dt;
typedef ap_uint<64> lz4_compressd_dt;

template <int DATAWIDTH, int BURST_SIZE>
void mm2s(const ap_uint<DATAWIDTH> *in,
          const uint32_t input_idx[PARALLEL_BLOCK], 
          hls::stream<ap_uint<DATAWIDTH> > &outStream_0,
#if PARALLEL_BLOCK > 1
          hls::stream<ap_uint<DATAWIDTH> > &outStream_1,
#endif 
#if PARALLEL_BLOCK > 2
          hls::stream<ap_uint<DATAWIDTH> > &outStream_2, 
          hls::stream<ap_uint<DATAWIDTH> > &outStream_3, 
#endif
#if PARALLEL_BLOCK > 4
          hls::stream<ap_uint<DATAWIDTH> > &outStream_4, 
          hls::stream<ap_uint<DATAWIDTH> > &outStream_5, 
          hls::stream<ap_uint<DATAWIDTH> > &outStream_6, 
          hls::stream<ap_uint<DATAWIDTH> > &outStream_7, 
#endif
          const uint32_t input_size[PARALLEL_BLOCK]
        )
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH/c_byte_size;
    ap_uint<DATAWIDTH> local_buffer[PARALLEL_BLOCK][BURST_SIZE];
    #pragma HLS ARRAY_PARTITION variable=local_buffer dim=1 complete
    #pragma HLS RESOURCE variable=local_buffer core=RAM_2P_LUTRAM
    uint32_t read_idx[PARALLEL_BLOCK];
    uint32_t write_idx[PARALLEL_BLOCK];
    uint32_t read_size[PARALLEL_BLOCK];
    #pragma HLS ARRAY_PARTITION variable=read_idx  dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=write_idx dim=0 complete
    #pragma HLS ARRAY_PARTITION variable=read_size dim=0 complete
    ap_uint<PARALLEL_BLOCK> pending;
    ap_uint<PARALLEL_BLOCK> is_full;
    for (uint32_t bIdx  = 0 ; bIdx < PARALLEL_BLOCK ; bIdx++){
        #pragma HLS UNROLL 
        read_idx[bIdx]  = 0;
        write_idx[bIdx] = 0;
        read_size[bIdx] = 0;
        pending.range(bIdx,bIdx) = 1;
    }
    while(pending){
        pending = 0;
        for (uint32_t bIdx = 0; bIdx < PARALLEL_BLOCK ; bIdx++){
            //Global Memory read
            uint32_t pending_bytes = GET_DIFF_IF_BIG(input_size[bIdx],read_size[bIdx]);
            if((pending_bytes) && (read_idx[bIdx] == write_idx[bIdx])){
                uint32_t pending_words = (pending_bytes -1)/c_word_size +1;
                uint32_t burst_size = (pending_words > BURST_SIZE)? BURST_SIZE: pending_words;
                uint32_t mem_read_byte_idx = read_size[bIdx] + input_idx[bIdx];
                uint32_t mem_read_word_idx = (mem_read_byte_idx)?((mem_read_byte_idx-1)/c_word_size + 1):0;
                gmem_rd:for(uint32_t i= 0 ; i < burst_size ; i++){
                #pragma HLS PIPELINE II=1
                    local_buffer[bIdx][i] = in[mem_read_word_idx+i];
                }
                pending.range(bIdx,bIdx) = 1;
                read_idx[bIdx]  = 0;
                write_idx[bIdx] = burst_size;
                read_size[bIdx] += burst_size*c_word_size; 
            }
        }
        ap_uint<PARALLEL_BLOCK> terminate_all;
        terminate_all = 1;
        bool terminate = 0;
        mm2s:for(int i = 0  ; (terminate == 0) && (terminate_all != 0) ; i++){
            #pragma HLS PIPELINE II=1
            MM2S_IF_NOT_FULL(outStream_0,0);
#if PARALLEL_BLOCK > 1
            MM2S_IF_NOT_FULL(outStream_1,1);
#endif
#if PARALLEL_BLOCK > 2
            MM2S_IF_NOT_FULL(outStream_2,2);
            MM2S_IF_NOT_FULL(outStream_3,3);
#endif
#if PARALLEL_BLOCK > 4
            MM2S_IF_NOT_FULL(outStream_4,4);
            MM2S_IF_NOT_FULL(outStream_5,5);
            MM2S_IF_NOT_FULL(outStream_6,6);
            MM2S_IF_NOT_FULL(outStream_7,7);
#endif
            terminate = 0;
            for(uint32_t bIdx = 0 ; bIdx < PARALLEL_BLOCK ; bIdx++){
            #pragma HLS UNROLL 
                if(read_idx[bIdx] == write_idx[bIdx]){
                    terminate_all.range(bIdx,bIdx) = 0;
                    if (read_size[bIdx] < input_size[bIdx]){
                        terminate = 1;
                    }
                }else{
                    terminate_all.range(bIdx,bIdx) = 1;
                    pending.range(bIdx,bIdx) = 1;
                }
            }
        }
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void stream_downsizer(
        hls::stream<ap_uint<IN_WIDTH>  > &inStream,
        hls::stream<ap_uint<OUT_WIDTH> > &outStream,
        SIZE_DT input_size)
{
    if(input_size == 0)
        return;

    const int c_byte_width = 8;
    const int c_input_word = IN_WIDTH/c_byte_width;
    const int c_out_word   = OUT_WIDTH/c_byte_width;
    uint32_t sizeOutputV   = (input_size -1)/c_out_word + 1;
    int factor = c_input_word / c_out_word;
    ap_uint<IN_WIDTH> inBuffer= 0;
    
    conv512toV:for (int i = 0 ; i < sizeOutputV ; i++){
    #pragma HLS PIPELINE II=1
        int idx = i % factor;
        if (idx == 0)  inBuffer= inStream.read();
        ap_uint<OUT_WIDTH> tmpValue = inBuffer.range((idx+1)*VEC - 1, idx*VEC);
        outStream << tmpValue;
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void stream_upsizer(
        hls::stream<ap_uint<IN_WIDTH> >     &inStream,
        hls::stream<SIZE_DT >               &inStreamSize,
        hls::stream<ap_uint<OUT_WIDTH> >    &outStream,
        hls::stream<SIZE_DT >               &outStreamSize
        )
{
    //Constants
    const int c_byte_width = 8 ; // 8bit is each BYTE
    const int c_upsize_factor = OUT_WIDTH/ c_byte_width;
    const int c_in_size = IN_WIDTH / c_byte_width;

    ap_uint<2 * OUT_WIDTH> outBuffer = 0; // Declaring double buffers 
    uint32_t byteIdx = 0; 
    for (SIZE_DT size = inStreamSize.read() ; size != 0 ; size = inStreamSize.read()){
        //rounding off the output size
        uint16_t outSize = ((size+byteIdx)/c_upsize_factor ) * c_upsize_factor;
        if (outSize) {
            outStreamSize << outSize;
        }
        stream_upsizer:for (int i = 0 ; i < size ; i+=c_in_size ){
        #pragma HLS PIPELINE II=1 
            int chunk_size=c_in_size;
            if (chunk_size + i > size) chunk_size = size-i;
            ap_uint<IN_WIDTH> tmpValue = inStream.read();
            outBuffer.range((byteIdx+c_in_size)* c_byte_width-1, byteIdx*c_byte_width) = tmpValue;
            byteIdx +=chunk_size;
            if (byteIdx >= c_upsize_factor) {
                outStream << outBuffer.range(OUT_WIDTH-1,0);
                outBuffer >>= OUT_WIDTH;
                byteIdx -=c_upsize_factor;
            }
        }
    }
    if (byteIdx){
        outStreamSize << byteIdx;
        outStream << outBuffer.range(OUT_WIDTH-1,0);
    }
    //end of block
    outStreamSize << 0;
}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH>
void    s2mm(
        ap_uint<DATAWIDTH>                  *out, 
        const uint32_t                      output_idx[PARALLEL_BLOCK],
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_0,
#if PARALLEL_BLOCK > 1
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_1,
#endif
#if PARALLEL_BLOCK > 2
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_2,
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_3,
#endif
#if PARALLEL_BLOCK > 4 
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_4,
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_5,
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_6,
        hls::stream<ap_uint<DATAWIDTH> >    &inStream_7,
#endif
        hls::stream<uint16_t>               &inStreamSize_0,
#if PARALLEL_BLOCK > 1
        hls::stream<uint16_t>               &inStreamSize_1,
#endif
#if PARALLEL_BLOCK > 2
        hls::stream<uint16_t>               &inStreamSize_2,
        hls::stream<uint16_t>               &inStreamSize_3,
#endif
#if PARALLEL_BLOCK > 4 
        hls::stream<uint16_t>               &inStreamSize_4,
        hls::stream<uint16_t>               &inStreamSize_5,
        hls::stream<uint16_t>               &inStreamSize_6,
        hls::stream<uint16_t>               &inStreamSize_7,
#endif
        STREAM_SIZE_DT                      output_size[PARALLEL_BLOCK]
        )
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH/c_byte_size;
    const int c_max_burst_size = c_word_size * BURST_SIZE;
    uint32_t input_size[PARALLEL_BLOCK];
    uint32_t read_size[PARALLEL_BLOCK];
    uint32_t write_size[PARALLEL_BLOCK];
    uint32_t burst_size[PARALLEL_BLOCK];
    uint32_t write_idx[PARALLEL_BLOCK];
    #pragma HLS ARRAY PARTITION variable=input_size dim=0 complete
    #pragma HLS ARRAY PARTITION variable=read_size  dim=0 complete
    #pragma HLS ARRAY PARTITION variable=write_size dim=0 complete
    #pragma HLS ARRAY PARTITION variable=write_idx dim=0 complete
    #pragma HLS ARRAY PARTITION variable=burst_size dim=0 complete
    ap_uint<PARALLEL_BLOCK> end_of_stream = 0;
    ap_uint<PARALLEL_BLOCK> is_pending = 1;
    ap_uint<DATAWIDTH> local_buffer[PARALLEL_BLOCK][BURST_SIZE];
    #pragma HLS ARRAY_PARTITION variable=local_buffer dim=1 complete
    #pragma HLS RESOURCE variable=local_buffer core=RAM_2P_LUTRAM

    for (int i = 0; i < PARALLEL_BLOCK ; i++){
        #pragma HLS UNROLL
        input_size[i] = 0;
        read_size[i] = 0;
        write_size[i] = 0;
        write_idx[i] = 0;
    }
    bool done = false;
    uint32_t loc=0;
    uint32_t remaining_data = 0;
    while(is_pending != 0){
        S2MM_READ_SIZE(0,inStreamSize_0,end_of_stream); 
#if PARALLEL_BLOCK > 1
        S2MM_READ_SIZE(1,inStreamSize_1,end_of_stream); 
#endif
#if PARALLEL_BLOCK > 2
        S2MM_READ_SIZE(2,inStreamSize_2,end_of_stream); 
        S2MM_READ_SIZE(3,inStreamSize_3,end_of_stream); 
#endif
#if PARALLEL_BLOCK > 4
        S2MM_READ_SIZE(4,inStreamSize_4,end_of_stream); 
        S2MM_READ_SIZE(5,inStreamSize_5,end_of_stream); 
        S2MM_READ_SIZE(6,inStreamSize_6,end_of_stream); 
        S2MM_READ_SIZE(7,inStreamSize_7,end_of_stream); 
#endif
        done = false;
        for (int i = 0 ; (is_pending != 0) && (done == 0) ; i++ ){
            #pragma HLS PIPELINE II=1 
            S2MM_IF_NOT_EMPTY(0,inStream_0,burst_size,input_size,read_size,write_size,write_idx); 
#if PARALLEL_BLOCK > 1
            S2MM_IF_NOT_EMPTY(1,inStream_1,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 2
            S2MM_IF_NOT_EMPTY(2,inStream_2,burst_size,input_size,read_size,write_size,write_idx); 
            S2MM_IF_NOT_EMPTY(3,inStream_3,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 4
            S2MM_IF_NOT_EMPTY(4,inStream_4,burst_size,input_size,read_size,write_size,write_idx); 
            S2MM_IF_NOT_EMPTY(5,inStream_5,burst_size,input_size,read_size,write_size,write_idx); 
            S2MM_IF_NOT_EMPTY(6,inStream_6,burst_size,input_size,read_size,write_size,write_idx); 
            S2MM_IF_NOT_EMPTY(7,inStream_7,burst_size,input_size,read_size,write_size,write_idx); 
#endif
        }
          
        for(int i = 0; i < PARALLEL_BLOCK; i++){
            //Write the data to global memory
            if((read_size[i]> write_size[i]) && (read_size[i] - write_size[i]) >= burst_size[i]){
                uint32_t base_addr = output_idx[i] + write_size[i];
                uint32_t base_idx = base_addr / c_word_size;
                uint32_t burst_size_in_words = (burst_size[i])?((burst_size[i]-1)/c_word_size + 1):0;
                for (int j = 0 ; j < burst_size_in_words ; j++){
                    #pragma HLS PIPELINE II=1
                    out[base_idx + j] = local_buffer[i][j];
                }
                write_size[i] += burst_size[i];
                write_idx[i] = 0;
            }
       }
        for(int i = 0; i < PARALLEL_BLOCK; i++){
            #pragma HLS UNROLL
            if(end_of_stream.range(i,i) && (write_size[i] >= input_size[i])){
                is_pending.range(i,i) = 0;                
            }
            else{
                is_pending.range(i,i) = 1;
            }
       }

    }
    for (int i = 0 ; i < PARALLEL_BLOCK ; i++){
        output_size[i] = input_size[i];
    }

}


void lz_compress(
        hls::stream<ap_uint<BIT> > &inStream,          
        hls::stream<compressd_dt> &outStream,          
        uint32_t input_size,
        uint32_t left_bytes 
        )
{
    if(input_size == 0) return;      
    //Dictionary
    uintDictV_t dict[LZ_DICT_SIZE];
    #pragma HLS RESOURCE variable=dict core=XPM_MEMORY uram

    uintDictV_t resetValue=0;
    for (int i = 0 ; i < MATCH_LEVEL; i++){
        #pragma HLS UNROLL 
        resetValue.range((i+1)*DICT_ELE_WIDTH -1, i*DICT_ELE_WIDTH + MATCH_LEN*BIT) = -1;
    }
    //Initialization of Dictionary
    dict_flush:for (int i  =0 ; i < LZ_DICT_SIZE; i++){
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL FACTOR=2
        dict[i] = resetValue;
   }

    uint8_t present_window[MATCH_LEN];
    #pragma HLS ARRAY_PARTITION variable=present_window complete 
    for (uint8_t i = 1 ; i < MATCH_LEN; i++){
        present_window[i] = inStream.read();
    }
    lz_compress:for (uint32_t i = MATCH_LEN-1; i < input_size -left_bytes; i++)
              {
    #pragma HLS PIPELINE II=1
    #pragma HLS dependence variable=dict inter false
        uint32_t currIdx = i - MATCH_LEN +1;
        //shift present window and load next value
        for(int m = 0 ; m < MATCH_LEN -1; m++){
            #pragma HLS UNROLL
            present_window[m] = present_window[m+1];
        }
        present_window[MATCH_LEN-1] = inStream.read();

        //Calculate Hash Value
        uint32_t hash = (present_window[0] << 4) ^
                        (present_window[1] << 3) ^
                        (present_window[2] << 3) ^
                        (present_window[3]);

        //Dictionary Lookup
        uintDictV_t dictReadValue = dict[hash];
        uintDictV_t dictWriteValue = dictReadValue << DICT_ELE_WIDTH;
        for(int m = 0 ; m < MATCH_LEN ; m++){
            #pragma HLS UNROLL
            dictWriteValue.range((m+1)*BIT-1,m*BIT) = present_window[m];
        }
        dictWriteValue.range(DICT_ELE_WIDTH -1, MATCH_LEN*BIT) = currIdx;
        //Dictionary Update
        dict[hash] = dictWriteValue;

        //Match search and Filtering
        // Comp dict pick
        uint8_t match_length=0;
        uint32_t match_offset=0;
        for (int l = 0 ; l < MATCH_LEVEL ; l++){
            uint8_t len = 0;
            bool done = 0;
            uintDict_t compareWith = dictReadValue.range((l+1)*DICT_ELE_WIDTH-1, l*DICT_ELE_WIDTH);
            uint32_t compareIdx = compareWith.range(DICT_ELE_WIDTH-1, MATCH_LEN*BIT);
            for (int m = 0; m < MATCH_LEN; m++){
                if (present_window[m] == compareWith.range((m+1)*BIT-1,m*BIT) && !done){
                    len++;
                }else{
                    done = 1;
                }
            }
            if ((len >= MIN_MATCH)&& (currIdx > compareIdx) && ((currIdx -compareIdx) < LZ_MAX_OFFSET_LIMIT) && ((currIdx - compareIdx - 1) >= MIN_OFFSET)){
                len = len;
            }else{
                len = 0;
            }
            if (len > match_length){
                match_length = len;
                match_offset = currIdx -compareIdx - 1;
            }
        }
        compressd_dt outValue = 0;
        outValue.range(7,0)     = present_window[0];
        outValue.range(15,8)    = match_length;
        outValue.range(31,16)   = match_offset;
        outStream << outValue;
    }
    lz_compress_leftover:for (int m = 1 ; m < MATCH_LEN ; m++){
        #pragma HLS PIPELINE
        compressd_dt outValue = 0;
        outValue.range(7,0)     = present_window[m];
        outStream << outValue;
    }
    lz_left_bytes:for (int l = 0 ; l < left_bytes ; l++){
        #pragma HLS PIPELINE
        compressd_dt outValue = 0;
        outValue.range(7,0)     = inStream.read();
        outStream << outValue;
    }
}
template<int MAX_MATCH_LEN, int OFFSET_WINDOW>
void lz_booster(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,       
        uint32_t input_size, uint32_t left_bytes
)
{

    if(input_size == 0) return;
    uint8_t local_mem[OFFSET_WINDOW];
    uint32_t match_loc = 0;
    uint32_t match_len =0;
    compressd_dt outValue;
    bool matchFlag=false;
    lz_booster:for (uint32_t i = 0 ; i < input_size-left_bytes; i++){
        #pragma HLS PIPELINE II=1 
        #pragma HLS dependence variable=local_mem inter false
        compressd_dt inValue = inStream.read();
        uint8_t tCh      = inValue.range(7,0);
        uint8_t tLen     = inValue.range(15,8);
        uint16_t tOffset = inValue.range(31,16);
        uint8_t match_ch = local_mem[match_loc%OFFSET_WINDOW];
        local_mem[i%OFFSET_WINDOW] = tCh;
        if (    matchFlag 
                && (match_len< MAX_MATCH_LEN) 
                && (tCh == match_ch) 
           ){
                match_len++;
                match_loc++;
                outValue.range(15,8) = match_len;
        }else{
            match_len = 1;
            match_loc = i - tOffset;
            if (i) outStream << outValue;
            outValue = inValue;
            if(tLen){
                matchFlag = true;
            }else{
                matchFlag =false;
            }
        }
    }
    outStream << outValue;
    lz_booster_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        outStream << inStream.read();
    }
}
void lz_filter(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,       
        uint32_t input_size, uint32_t left_bytes
)
{
    if(input_size == 0) return;
    uint32_t skip_len =0;
    lz_filter:for (uint32_t i = 0 ; i < input_size-left_bytes; i++){
        #pragma HLS PIPELINE II=1 
        compressd_dt inValue = inStream.read();
        uint8_t   tLen     = inValue.range(15,8);
        if (skip_len){
            skip_len--;
        }else{
            outStream << inValue;
            if(tLen)skip_len = tLen-1;
        }
    }
    lz_filter_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        outStream << inStream.read();
    }
}


void lz4_compress(hls::stream<uint8_t> &in_lit_inStream,
                  hls::stream<lz4_compressd_dt> &in_lenOffset_Stream,
                  hls::stream<ap_uint<8> > &outStream,
                  hls::stream<uint16_t> &outStreamSize,
                  uint32_t input_size
                 ) {

    uint32_t lit_len = 0;
    uint16_t outCntr=0;
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
        outStream << outValue;
        outCntr++;
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


    lz4_divide:for (uint32_t i = 0; i < input_size; i++) {
    #pragma HLS PIPELINE II=1 
        compressd_dt tmpEncodedValue = inStream.read();
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
    hls::stream<ap_uint<BIT> >  inStream_0("inStream_0") ;
    hls::stream<compressd_dt>     compressdStream_0("compressdStream_0");
    hls::stream<compressd_dt>     filteredStream_0("filteredStream_0");
    hls::stream<uint16_t >  outStream512Size_0("outStream512Size_0");
    hls::stream<uint512_t>   outStream512_0("outStream512_0");
    hls::stream<uint8_t> litOut_0("litOut_0");
    hls::stream<lz4_compressd_dt> lenOffsetOut_0("lenOffsetOut_0");
    hls::stream<ap_uint<8> > lz4Out_0("lz4Out_0");
    hls::stream<uint16_t> lz4OutSize_0("lz4OutSize_0");
    #pragma HLS STREAM variable=outStream512Size_0      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_0              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_0           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_0         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_0        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_0          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_0                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_0          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_0                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_0            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_0      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_0              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_0           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_0         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_0        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_0          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_0          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_0                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_0            core=FIFO_SRL

#if PARALLEL_BLOCK > 1
    hls::stream<uint512_t>   inStream512_1("inStream512_1");
    hls::stream<ap_uint<BIT> >  inStream_1("inStream_1") ;
    hls::stream<compressd_dt>     compressdStream_1("compressdStream_1");
    hls::stream<compressd_dt>     filteredStream_1("filteredStream_1");
    hls::stream<uint16_t >  outStream512Size_1("outStream512Size_1");
    hls::stream<uint512_t>   outStream512_1("outStream512_1");
    hls::stream<uint8_t> litOut_1("litOut_1");
    hls::stream<lz4_compressd_dt> lenOffsetOut_1("lenOffsetOut_1");
    hls::stream<ap_uint<8> > lz4Out_1("lz4Out_1");
    hls::stream<uint16_t> lz4OutSize_1("lz4OutSize_1");
    #pragma HLS STREAM variable=outStream512Size_1      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_1              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_1           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_1         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_1        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_1          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_1                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_1          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_1                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_1            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_1      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_1              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_1           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_1         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_1        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_1          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_1          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_1                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_1            core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 2
    hls::stream<uint512_t>   inStream512_2("inStream512_2");
    hls::stream<ap_uint<BIT> >  inStream_2("inStream_2") ;
    hls::stream<compressd_dt>     compressdStream_2("compressdStream_2");
    hls::stream<compressd_dt>     filteredStream_2("filteredStream_2");
    hls::stream<uint16_t >  outStream512Size_2("outStream512Size_2");
    hls::stream<uint512_t>   outStream512_2("outStream512_2");
    hls::stream<uint8_t> litOut_2("litOut_2");
    hls::stream<lz4_compressd_dt> lenOffsetOut_2("lenOffsetOut_2");
    hls::stream<ap_uint<8> > lz4Out_2("lz4Out_2");
    hls::stream<uint16_t> lz4OutSize_2("lz4OutSize_2");
    #pragma HLS STREAM variable=outStream512Size_2      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_2              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_2           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_2         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_2        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_2          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_2                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_2          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_2                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_2            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_2      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_2              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_2           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_2         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_2        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_2          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_2          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_2                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_2            core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_3("inStream512_3");
    hls::stream<ap_uint<BIT> >  inStream_3("inStream_3") ;
    hls::stream<compressd_dt>     compressdStream_3("compressdStream_3");
    hls::stream<compressd_dt>     filteredStream_3("filteredStream_3");
    hls::stream<uint16_t >  outStream512Size_3("outStream512Size_3");
    hls::stream<uint512_t>   outStream512_3("outStream512_3");
    hls::stream<uint8_t> litOut_3("litOut_3");
    hls::stream<lz4_compressd_dt> lenOffsetOut_3("lenOffsetOut_3");
    hls::stream<ap_uint<8> > lz4Out_3("lz4Out_3");
    hls::stream<uint16_t> lz4OutSize_3("lz4OutSize_3");
    #pragma HLS STREAM variable=outStream512Size_3      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_3              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_3           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_3         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_3        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_3          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_3                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_3          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_3                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_3            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_3      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_3              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_3           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_3         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_3        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_3          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_3          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_3                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_3            core=FIFO_SRL
#endif
#if PARALLEL_BLOCK > 4
    hls::stream<uint512_t>   inStream512_4("inStream512_4");
    hls::stream<ap_uint<BIT> >  inStream_4("inStream_4") ;
    hls::stream<compressd_dt>     compressdStream_4("compressdStream_4");
    hls::stream<compressd_dt>     filteredStream_4("filteredStream_4");
    hls::stream<uint16_t >  outStream512Size_4("outStream512Size_4");
    hls::stream<uint512_t>   outStream512_4("outStream512_4");
    hls::stream<uint8_t> litOut_4("litOut_4");
    hls::stream<lz4_compressd_dt> lenOffsetOut_4("lenOffsetOut_4");
    hls::stream<ap_uint<8> > lz4Out_4("lz4Out_4");
    hls::stream<uint16_t> lz4OutSize_4("lz4OutSize_4");
    #pragma HLS STREAM variable=outStream512Size_4      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_4              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_4           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_4         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_4        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_4          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_4                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_4          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_4                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_4            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_4      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_4              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_4           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_4         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_4        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_4          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_4          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_4                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_4            core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_5("inStream512_5");
    hls::stream<ap_uint<BIT> >  inStream_5("inStream_5") ;
    hls::stream<compressd_dt>     compressdStream_5("compressdStream_5");
    hls::stream<compressd_dt>     filteredStream_5("filteredStream_5");
    hls::stream<uint16_t >  outStream512Size_5("outStream512Size_5");
    hls::stream<uint512_t>   outStream512_5("outStream512_5");
    hls::stream<uint8_t> litOut_5("litOut_5");
    hls::stream<lz4_compressd_dt> lenOffsetOut_5("lenOffsetOut_5");
    hls::stream<ap_uint<8> > lz4Out_5("lz4Out_5");
    hls::stream<uint16_t> lz4OutSize_5("lz4OutSize_5");
    #pragma HLS STREAM variable=outStream512Size_5      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_5              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_5           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_5         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_5        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_5          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_5                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_5          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_5                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_5            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_5      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_5              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_5           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_5         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_5        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_5          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_5          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_5                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_5            core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_6("inStream512_6");
    hls::stream<ap_uint<BIT> >  inStream_6("inStream_6") ;
    hls::stream<compressd_dt>     compressdStream_6("compressdStream_6");
    hls::stream<compressd_dt>     filteredStream_6("filteredStream_6");
    hls::stream<uint16_t >  outStream512Size_6("outStream512Size_6");
    hls::stream<uint512_t>   outStream512_6("outStream512_6");
    hls::stream<uint8_t> litOut_6("litOut_6");
    hls::stream<lz4_compressd_dt> lenOffsetOut_6("lenOffsetOut_6");
    hls::stream<ap_uint<8> > lz4Out_6("lz4Out_6");
    hls::stream<uint16_t> lz4OutSize_6("lz4OutSize_6");
    #pragma HLS STREAM variable=outStream512Size_6      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_6              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_6           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_6         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_6        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_6          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_6                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_6          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_6                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_6            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_6      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_6              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_6           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_6         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_6        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_6          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_6          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_6                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_6            core=FIFO_SRL

    hls::stream<uint512_t>   inStream512_7("inStream512_7");
    hls::stream<ap_uint<BIT> >  inStream_7("inStream_7") ;
    hls::stream<compressd_dt>     compressdStream_7("compressdStream_7");
    hls::stream<compressd_dt>     filteredStream_7("filteredStream_7");
    hls::stream<uint16_t >  outStream512Size_7("outStream512Size_7");
    hls::stream<uint512_t>   outStream512_7("outStream512_7");
    hls::stream<uint8_t> litOut_7("litOut_7");
    hls::stream<lz4_compressd_dt> lenOffsetOut_7("lenOffsetOut_7");
    hls::stream<ap_uint<8> > lz4Out_7("lz4Out_7");
    hls::stream<uint16_t> lz4OutSize_7("lz4OutSize_7");
    #pragma HLS STREAM variable=outStream512Size_7      depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream_7              depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStream512_7           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=compressdStream_7         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=filteredStream_7        depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512_7          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=litOut_7                depth=max_literal_count
    #pragma HLS STREAM variable=lenOffsetOut_7          depth=c_gmem_burst_size
    #pragma HLS STREAM variable=lz4Out_7                depth=1024
    #pragma HLS STREAM variable=lz4OutSize_7            depth=c_gmem_burst_size
    
    #pragma HLS RESOURCE variable=outStream512Size_7      core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream_7              core=FIFO_SRL
    #pragma HLS RESOURCE variable=inStream512_7           core=FIFO_SRL
    #pragma HLS RESOURCE variable=compressdStream_7         core=FIFO_SRL
    #pragma HLS RESOURCE variable=filteredStream_7        core=FIFO_SRL
    #pragma HLS RESOURCE variable=outStream512_7          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lenOffsetOut_7          core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4Out_7                core=FIFO_SRL
    #pragma HLS RESOURCE variable=lz4OutSize_7            core=FIFO_SRL
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
    // Compress Core 0
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_0,inStream_0,input_size[0]);
    lz_compress(inStream_0, compressdStream_0, input_size[0],left_bytes);
    lz_filter(compressdStream_0, filteredStream_0,input_size[0],left_bytes);
    lz4_divide(filteredStream_0, litOut_0, lenOffsetOut_0, input_size[0], max_lit_limit, 0);
    lz4_compress(litOut_0,lenOffsetOut_0, lz4Out_0,lz4OutSize_0,input_size[0]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_0,lz4OutSize_0,outStream512_0,outStream512Size_0);

#if PARALLEL_BLOCK > 1
    // Compress Core 1 
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_1,inStream_1,input_size[1]);
    lz_compress(inStream_1, compressdStream_1, input_size[1],left_bytes);
    lz_filter(compressdStream_1, filteredStream_1,input_size[1],left_bytes);
    lz4_divide(filteredStream_1, litOut_1, lenOffsetOut_1, input_size[1], max_lit_limit, 1);
    lz4_compress(litOut_1,lenOffsetOut_1, lz4Out_1,lz4OutSize_1,input_size[1]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_1,lz4OutSize_1,outStream512_1,outStream512Size_1);
#endif
#if PARALLEL_BLOCK > 2
    // Compress Core 2
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_2,inStream_2,input_size[2]);
    lz_compress(inStream_2, compressdStream_2, input_size[2],left_bytes);
    lz_filter(compressdStream_2, filteredStream_2,input_size[2],left_bytes);
    lz4_divide(filteredStream_2, litOut_2, lenOffsetOut_2, input_size[2], max_lit_limit, 2);
    lz4_compress(litOut_2,lenOffsetOut_2, lz4Out_2,lz4OutSize_2,input_size[2]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_2,lz4OutSize_2, outStream512_2, outStream512Size_2);
    
    // Compress Core 3
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_3, inStream_3, input_size[3]);
    lz_compress(inStream_3, compressdStream_3, input_size[3],left_bytes);
    lz_filter(compressdStream_3, filteredStream_3, input_size[3], left_bytes);
    lz4_divide(filteredStream_3, litOut_3, lenOffsetOut_3, input_size[3], max_lit_limit, 3);
    lz4_compress(litOut_3, lenOffsetOut_3, lz4Out_3, lz4OutSize_3,input_size[3]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_3, lz4OutSize_3, outStream512_3, outStream512Size_3);
#endif
#if PARALLEL_BLOCK > 4
    // Compress Core 4
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_4,inStream_4,input_size[4]);
    lz_compress(inStream_4, compressdStream_4, input_size[4],left_bytes);
    lz_filter(compressdStream_4, filteredStream_4, input_size[4],left_bytes);
    lz4_divide(filteredStream_4, litOut_4, lenOffsetOut_4, input_size[4], max_lit_limit, 4);
    lz4_compress(litOut_4,lenOffsetOut_4, lz4Out_4, lz4OutSize_4, input_size[4]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_4, lz4OutSize_4, outStream512_4, outStream512Size_4);

    // Compress Core 5
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_5, inStream_5,input_size[5]);
    lz_compress(inStream_5, compressdStream_5, input_size[5],left_bytes);
    lz_filter(compressdStream_5, filteredStream_5, input_size[5],left_bytes);
    lz4_divide(filteredStream_5, litOut_5, lenOffsetOut_5, input_size[5], max_lit_limit, 5);
    lz4_compress(litOut_5, lenOffsetOut_5, lz4Out_5, lz4OutSize_5, input_size[5]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_5,lz4OutSize_5,outStream512_5,outStream512Size_5);

    // Compress Core 6
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_6, inStream_6, input_size[6]);
    lz_compress(inStream_6, compressdStream_6, input_size[6],left_bytes);
    lz_filter(compressdStream_6, filteredStream_6, input_size[6],left_bytes);
    lz4_divide(filteredStream_6, litOut_6, lenOffsetOut_6, input_size[6], max_lit_limit, 6);
    lz4_compress(litOut_6, lenOffsetOut_6, lz4Out_6, lz4OutSize_6, input_size[6]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_6, lz4OutSize_6, outStream512_6, outStream512Size_6);

    // Compress Core 7
    stream_downsizer<uint32_t, GMEM_DWIDTH, 8>(inStream512_7, inStream_7, input_size[7]);
    lz_compress(inStream_7, compressdStream_7, input_size[7],left_bytes);
    lz_filter(compressdStream_7, filteredStream_7, input_size[7],left_bytes);
    lz4_divide(filteredStream_7, litOut_7, lenOffsetOut_7, input_size[7], max_lit_limit, 7);
    lz4_compress(litOut_7, lenOffsetOut_7, lz4Out_7, lz4OutSize_7, input_size[7]);
    stream_upsizer<uint16_t, BIT, GMEM_DWIDTH>(lz4Out_7, lz4OutSize_7, outStream512_7, outStream512Size_7);
#endif
    s2mm<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
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


extern "C" {
void xil_lz4_cu1
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
                    output_idx[j] = 2 * (i + j) * max_block_size;
                }
            }else {
                input_block_size[j] = 0;
                input_idx[j] = 0;
            }
            output_block_size[j] = 0;
            max_lit_limit[j] = 0;
        }


        // Call for parallel compression
        lz4(in,
            out,
            input_idx,
            output_idx,
            input_block_size,
            output_block_size,
            max_lit_limit  
           );

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
