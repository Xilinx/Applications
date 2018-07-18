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
#pragma once
#include <ap_int.h>

#ifndef PARALLEL_BLOCK
#define PARALLEL_BLOCK 8
#endif

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

#define STREAM_UTILS_MM2S_IF_NOT_FULL(bIdx,outStream,is_full,read_idx,write_idx,local_buffer)\
        is_full.range(bIdx,bIdx) = outStream.full();\
            if(!is_full.range(bIdx,bIdx) && (read_idx[bIdx] != write_idx[bIdx])){\
                outStream << local_buffer[bIdx][read_idx[bIdx]];\
                read_idx[bIdx] += 1;\
            }

#define STREAM_UTILS_S2MM_READ_SIZE(i,instream,end_of_stream)\
        if (!end_of_stream.range(i,i) && !instream.empty()){ \
            uint16_t tmpValue = instream.read(); \
            input_size[i] += tmpValue; \
            if(tmpValue == 0) end_of_stream.range(i,i) = 1; \
        }

#define STREAM_UTILS_S2MM_IF_NOT_EMPTY(i,instream,burst_size,input_size,read_size,write_size,write_idx) \
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

#define STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(i,instream,burst_size,input_size,read_size,write_size,write_idx) \
            burst_size[i] = c_max_burst_size; \
            if(((input_size[i] - write_size[i]) < burst_size[i])){ \
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
            }

template <int DATAWIDTH, int BURST_SIZE>
void mm2s(const ap_uint<DATAWIDTH> *in,
          const uint32_t _input_idx[PARALLEL_BLOCK], 
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
          const uint32_t _input_size[PARALLEL_BLOCK]
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
    uint32_t input_idx[PARALLEL_BLOCK]; 
    uint32_t input_size[PARALLEL_BLOCK];
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
        input_idx[bIdx] = _input_idx[bIdx];
        input_size[bIdx] = _input_size[bIdx];
        pending.range(bIdx,bIdx) = 1;
    }
    while(pending){
        pending = 0;
        for (uint32_t bIdx = 0; bIdx < PARALLEL_BLOCK ; bIdx++){
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
            STREAM_UTILS_MM2S_IF_NOT_FULL(0,outStream_0,is_full,read_idx,write_idx,local_buffer);
#if PARALLEL_BLOCK > 1
            STREAM_UTILS_MM2S_IF_NOT_FULL(1,outStream_1,is_full,read_idx,write_idx,local_buffer);
#endif
#if PARALLEL_BLOCK > 2
            STREAM_UTILS_MM2S_IF_NOT_FULL(2,outStream_2,is_full,read_idx,write_idx,local_buffer);
            STREAM_UTILS_MM2S_IF_NOT_FULL(3,outStream_3,is_full,read_idx,write_idx,local_buffer);
#endif
#if PARALLEL_BLOCK > 4
            STREAM_UTILS_MM2S_IF_NOT_FULL(4,outStream_4,is_full,read_idx,write_idx,local_buffer);
            STREAM_UTILS_MM2S_IF_NOT_FULL(5,outStream_5,is_full,read_idx,write_idx,local_buffer);
            STREAM_UTILS_MM2S_IF_NOT_FULL(6,outStream_6,is_full,read_idx,write_idx,local_buffer);
            STREAM_UTILS_MM2S_IF_NOT_FULL(7,outStream_7,is_full,read_idx,write_idx,local_buffer);
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
        ap_uint<OUT_WIDTH> tmpValue = inBuffer.range((idx+1)*OUT_WIDTH- 1, idx*OUT_WIDTH);
        outStream << tmpValue;
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void stream_upsizer(
        hls::stream<ap_uint<IN_WIDTH> >     &inStream,
        hls::stream<ap_uint<OUT_WIDTH> >    &outStream,
        SIZE_DT original_size
        )
{
    int pack_size = OUT_WIDTH/IN_WIDTH;
    ap_uint<OUT_WIDTH> outBuffer;
    for(int i = 0; i < original_size; i += pack_size) {
        int chunk_size = pack_size;
        
        if(i + pack_size > original_size) 
            chunk_size = original_size - i;
        
        for(int j = 0; j < chunk_size; j++){
        #pragma HLS PIPELINE II=1        
            uint8_t c = inStream.read();
            outBuffer.range((j+1)*IN_WIDTH - 1, j*IN_WIDTH) = c;
        }

        outStream << outBuffer;
    }
}
template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void upsizer_sizestream(
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
    //printme("%s: factor=%d\n",__FUNCTION__,c_upsize_factor);
    for (SIZE_DT size = inStreamSize.read() ; size != 0 ; size = inStreamSize.read()){
        //rounding off the output size
        uint16_t outSize = ((size+byteIdx)/c_upsize_factor ) * c_upsize_factor;
        if (outSize) {
            outStreamSize << outSize;
        }
        ////printme("%s: reading next data=%d outSize=%d c_in_size=%d\n ",__FUNCTION__, size,outSize,c_in_size);
        stream_upsizer:for (int i = 0 ; i < size ; i+=c_in_size ){
        #pragma HLS PIPELINE II=1 
            int chunk_size=c_in_size;
            if (chunk_size + i > size) chunk_size = size-i;
            ap_uint<IN_WIDTH> tmpValue = inStream.read();
            outBuffer.range((byteIdx+c_in_size)* c_byte_width-1, byteIdx*c_byte_width) = tmpValue;
            byteIdx +=chunk_size;
            ////printme("%s: value=%c, chunk_size = %d and byteIdx=%d\n",__FUNCTION__,(char)tmpValue, chunk_size,byteIdx);
            if (byteIdx >= c_upsize_factor) {
                outStream << outBuffer.range(OUT_WIDTH-1,0);
                outBuffer >>= OUT_WIDTH;
                byteIdx -=c_upsize_factor;
            }
        }
    }
    if (byteIdx){
        outStreamSize << byteIdx;
        ////printme("sent outSize %d \n", byteIdx);
        outStream << outBuffer.range(OUT_WIDTH-1,0);
    }
    //end of block
    outStreamSize << 0;
    //printme("%s:Ended \n",__FUNCTION__);
}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH>
void    s2mm_compress(
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

    //printme("%s:Started\n", __FUNCTION__);
    for (int i = 0; i < PARALLEL_BLOCK ; i++){
        #pragma HLS UNROLL
        input_size[i] = 0;
        read_size[i] = 0;
        write_size[i] = 0;
        write_idx[i] = 0;
        //printme("%s:Indx=%d out_idx=%d\n",__FUNCTION__,i , output_idx[i]);
    }
    bool done = false;
    uint32_t loc=0;
    uint32_t remaining_data = 0;
    while(is_pending != 0){
        STREAM_UTILS_S2MM_READ_SIZE(0,inStreamSize_0,end_of_stream); 
#if PARALLEL_BLOCK > 1
        STREAM_UTILS_S2MM_READ_SIZE(1,inStreamSize_1,end_of_stream); 
#endif
#if PARALLEL_BLOCK > 2
        STREAM_UTILS_S2MM_READ_SIZE(2,inStreamSize_2,end_of_stream); 
        STREAM_UTILS_S2MM_READ_SIZE(3,inStreamSize_3,end_of_stream); 
#endif
#if PARALLEL_BLOCK > 4
        STREAM_UTILS_S2MM_READ_SIZE(4,inStreamSize_4,end_of_stream); 
        STREAM_UTILS_S2MM_READ_SIZE(5,inStreamSize_5,end_of_stream); 
        STREAM_UTILS_S2MM_READ_SIZE(6,inStreamSize_6,end_of_stream); 
        STREAM_UTILS_S2MM_READ_SIZE(7,inStreamSize_7,end_of_stream); 
#endif
        done = false;
        for (int i = 0 ; (is_pending != 0) && (done == 0) ; i++ ){
            #pragma HLS PIPELINE II=1 
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(0,inStream_0,burst_size,input_size,read_size,write_size,write_idx); 
#if PARALLEL_BLOCK > 1
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(1,inStream_1,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 2
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(2,inStream_2,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(3,inStream_3,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 4
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(4,inStream_4,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(5,inStream_5,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(6,inStream_6,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_IF_NOT_EMPTY(7,inStream_7,burst_size,input_size,read_size,write_size,write_idx); 
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
    //printme("%s:Ended \n", __FUNCTION__);
    for (int i = 0 ; i < PARALLEL_BLOCK ; i++){
        output_size[i] = input_size[i];
    }

}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH>
void    s2mm_decompress(
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
        const STREAM_SIZE_DT                      input_size[PARALLEL_BLOCK]
        )
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH/c_byte_size;
    const int c_max_burst_size = c_word_size * BURST_SIZE;
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

    //printme("%s:Started\n", __FUNCTION__);
    for (int i = 0; i < PARALLEL_BLOCK ; i++){
        #pragma HLS UNROLL
        read_size[i] = 0;
        write_size[i] = 0;
        write_idx[i] = 0;
        //printme("%s:Indx=%d out_idx=%d\n",__FUNCTION__,i , output_idx[i]);
    }
    bool done = false;
    uint32_t loc=0;
    uint32_t remaining_data = 0;
    while(is_pending != 0){
        done = false;
        for (int i = 0 ; (is_pending != 0) && (done == 0) ; i++ ){
            #pragma HLS PIPELINE II=1 
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(0,inStream_0,burst_size,input_size,read_size,write_size,write_idx); 
#if PARALLEL_BLOCK > 1
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(1,inStream_1,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 2
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(2,inStream_2,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(3,inStream_3,burst_size,input_size,read_size,write_size,write_idx); 
#endif
#if PARALLEL_BLOCK > 4
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(4,inStream_4,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(5,inStream_5,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(6,inStream_6,burst_size,input_size,read_size,write_size,write_idx); 
            STREAM_UTILS_S2MM_DEC_IF_NOT_EMPTY(7,inStream_7,burst_size,input_size,read_size,write_size,write_idx); 
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
            if(done==true && (write_size[i] >= input_size[i])){
                is_pending.range(i,i) = 0;                
            }
            else{
                is_pending.range(i,i) = 1;
            }
       }

    }
}
