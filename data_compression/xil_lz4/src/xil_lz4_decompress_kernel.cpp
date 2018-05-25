/**********
 * Copyright (c) 2018, Xilinx, Inc.
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
#define MAX_OFFSET 65536 
#define HISTORY_SIZE MAX_OFFSET
#define MARKER 255

//LZ77 specific Defines
#define BIT 8

//LZ4 Decompress states
#define READ_TOKEN      0
#define READ_LIT_LEN    1
#define READ_LITERAL    2
#define READ_OFFSET0    3
#define READ_OFFSET1    4
#define READ_MATCH_LEN  5

#define READ_STATE 0
#define MATCH_STATE 1
#define LOW_OFFSET_STATE 2 
#define LOW_OFFSET 8 // This should be bigger than Pipeline Depth to handle inter dependency false case
typedef ap_uint<BIT> uintV_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> compressd_dt;

#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

#define MM2S_IF_NOT_FULL(outStream,bIdx)\
        is_full.range(bIdx,bIdx) = outStream.full();\
            if(!is_full.range(bIdx,bIdx) && (read_idx[bIdx] != write_idx[bIdx])){\
                outStream << local_buffer[bIdx][read_idx[bIdx]];\
                read_idx[bIdx] += 1;\
            }

#define S2MM_IF_NOT_EMPTY(i,instream,burst_size,input_size,read_size,write_size,write_idx) \
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


namespace  dec_cu1 
{
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
            outBuffer.range((j+1)*VEC - 1, j*VEC) = c;
        }

        outStream << outBuffer;
    }
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

    for (int i = 0; i < PARALLEL_BLOCK ; i++){
        #pragma HLS UNROLL
        read_size[i] = 0;
        write_size[i] = 0;
        write_idx[i] = 0;
    }
    bool done = false;
    uint32_t loc=0;
    uint32_t remaining_data = 0;
    while(is_pending != 0){
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
            if(done==true && (write_size[i] >= input_size[i])){
                is_pending.range(i,i) = 0;                
            }
            else{
                is_pending.range(i,i) = 1;
            }
       }

    }
}

void lz_decompress(
        hls::stream<compressd_dt> &inStream,          
        hls::stream<ap_uint<8> > &outStream,          
        uint32_t original_size
    )
{
    uint8_t local_buf[HISTORY_SIZE];
    #pragma HLS dependence variable=local_buf inter false

    uint32_t match_len = 0; 
    uint32_t out_len = 0;
    uint32_t match_loc = 0;
    uint32_t length_extract=0;
    uint8_t next_states = READ_STATE;
    uint16_t offset =0;
    compressd_dt nextValue;
    ap_uint<8> outValue = 0;
    ap_uint<8> prevValue[LOW_OFFSET];
    #pragma HLS ARRAY PARTITION variable=prevValue dim=0 complete
    lz_decompress:for(uint32_t i = 0; i < original_size; i++ ) {
    #pragma HLS PIPELINE II=1
        if (next_states == READ_STATE){
            nextValue = inStream.read();
            offset         = nextValue.range(15,0);
            length_extract = nextValue.range(31,16);
            if (length_extract){
                match_loc = i -offset -1;
                match_len = length_extract + 1;
                out_len = 1;
                if (offset>=LOW_OFFSET){
                    next_states = MATCH_STATE;
                    outValue = local_buf[match_loc % HISTORY_SIZE];
                }else{
                    next_states = LOW_OFFSET_STATE;
                    outValue = prevValue[offset];
                }
                match_loc++;
            }else{
                outValue = nextValue.range(7,0);
            }
        }else if (next_states == LOW_OFFSET_STATE){
            outValue = prevValue[offset];
            match_loc++;
            out_len++;
            if (out_len == match_len) next_states = READ_STATE;
        }else{
            outValue = local_buf[match_loc % HISTORY_SIZE];
            match_loc++;
            out_len++;
            if (out_len == match_len) next_states = READ_STATE;
        }
        local_buf[i % HISTORY_SIZE] = outValue;
        outStream << outValue;
        for(uint32_t pIdx= LOW_OFFSET-1 ; pIdx > 0 ; pIdx--){
            #pragma HLS UNROLL
            prevValue[pIdx] = prevValue[pIdx-1];
        }
        prevValue[0] = outValue;
    }
}

// LZ77 compress module
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
        hls::stream<ap_uint<512> > &instream_512, 
        hls::stream<ap_uint<512> > &outstream_512, 
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
    lz_decompress(decompressd_stream, decompressed_stream, output_size); 
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
    s2mm<uint32_t, GMEM_BURST_SIZE, GMEM_DWIDTH>(out,
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
void xil_lz4_dec_cu1
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
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem2
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

    
    for (int i = 0; i < no_blocks; i+=PARALLEL_BLOCK) {
        
        int nblocks = PARALLEL_BLOCK;
        if((i + PARALLEL_BLOCK) > no_blocks) {
            nblocks = no_blocks - i;
        }

        for (int j = 0; j < PARALLEL_BLOCK; j++) {
            if(j < nblocks) {
                uint32_t iSize = in_compress_size[i + j];
                uint32_t oSize = in_block_size[i + j];
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
        dec_cu1::lz4_dec(in,
                         out,
                         input_idx,
                         compress_size,
                         block_size,
                         compress_size1,
                         block_size1
                        );
    }

}
}
