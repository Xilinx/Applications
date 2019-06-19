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
#pragma once
#include <ap_int.h>

template <int DATAWIDTH, int BURST_SIZE, int NUM_BLOCKS>
void mm2s_nb(const ap_uint<DATAWIDTH> *in,
          const uint32_t _input_idx[PARALLEL_BLOCK], 
          hls::stream<ap_uint<DATAWIDTH> > outStream[PARALLEL_BLOCK],
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
        for (uint32_t bIdx = 0; bIdx < NUM_BLOCKS ; bIdx++){
            uint32_t pending_bytes = (input_size[bIdx]>read_size[bIdx])?(input_size[bIdx]-read_size[bIdx]):0;
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
                 for (uint8_t pb = 0; pb < PARALLEL_BLOCK; pb++){
                 #pragma HLS UNROLL
                    is_full.range(pb,pb) = outStream[pb].full();
                    if(!is_full.range(pb,pb) && (read_idx[pb] != write_idx[pb])){
                        outStream[pb] << local_buffer[pb][read_idx[pb]];
                        read_idx[pb] += 1;
                    }    
                 }
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
    convInWidthtoV:for (int i = 0 ; i < sizeOutputV ; i++){
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
            ap_uint<IN_WIDTH> c = inStream.read();
            outBuffer.range((j+1)*IN_WIDTH - 1, j*IN_WIDTH) = c;
        }

        outStream << outBuffer;
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void upsizer_eos(
        hls::stream<ap_uint<IN_WIDTH> >     &inStream,
        hls::stream<bool>            &inStream_eos,
        hls::stream<ap_uint<OUT_WIDTH> >    &outStream,
        hls::stream<bool>            &outStream_eos
        )
{
    //Constants
    const int c_byte_width = 8 ; // 8bit is each BYTE
    const int c_upsize_factor = OUT_WIDTH/ c_byte_width;
    const int c_in_size = IN_WIDTH / c_byte_width;

    ap_uint<OUT_WIDTH> outBuffer = 0; 
    ap_uint<IN_WIDTH> outBuffer_int[c_upsize_factor];
#pragma HLS array_partition variable=outBuffer_int dim=1 complete
    uint32_t byteIdx = 0;
    bool done = false;
    ////printme("%s: reading next data=%d outSize=%d c_in_size=%d\n ",__FUNCTION__, size,outSize,c_in_size);
    outBuffer_int[byteIdx] = inStream.read();
    stream_upsizer:for ( bool eos_flag=inStream_eos.read(); eos_flag==false ; eos_flag=inStream_eos.read()){
    #pragma HLS PIPELINE II=1
            for(int j=0;j<c_upsize_factor;j+=c_in_size) {                
                #pragma HLS unroll
                outBuffer.range((j+1)*c_byte_width-1,j*c_byte_width) = outBuffer_int[j];
            }
            byteIdx +=1;           
            ////printme("%s: value=%c, chunk_size = %d and byteIdx=%d\n",__FUNCTION__,(char)tmpValue, chunk_size,byteIdx);
            if (byteIdx >= c_upsize_factor) {
                outStream << outBuffer;
                outStream_eos << 0;
                byteIdx -=c_upsize_factor;
            }
            outBuffer_int[byteIdx] = inStream.read();
        }

    if (byteIdx){
        outStream_eos << 0;
        outStream << outBuffer;
    }
    //end of block 

    outStream << 0;
    outStream_eos << 1;
    //printme("%s:Ended \n",__FUNCTION__);
}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH, int NUM_BLOCKS>
void    s2mm_eos_nb(
        ap_uint<DATAWIDTH>                  *out, 
        const uint32_t                      output_idx[PARALLEL_BLOCK],
        hls::stream<ap_uint<DATAWIDTH> >    inStream[PARALLEL_BLOCK],
        hls::stream<bool>                   endOfStream[PARALLEL_BLOCK],
        hls::stream<uint32_t>               compressedSize[PARALLEL_BLOCK],
        STREAM_SIZE_DT                      output_size[PARALLEL_BLOCK]
        )
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH/c_byte_size;
    const int c_max_burst_size = BURST_SIZE;

    ap_uint<PARALLEL_BLOCK> is_pending = 1;
    ap_uint<DATAWIDTH> local_buffer[PARALLEL_BLOCK][BURST_SIZE];
    #pragma HLS ARRAY_PARTITION variable=local_buffer dim=1 complete
    #pragma HLS RESOURCE variable=local_buffer core=RAM_2P_LUTRAM

    ap_uint<PARALLEL_BLOCK> end_of_stream = 0;
    uint32_t read_size[PARALLEL_BLOCK];
    uint32_t write_size[PARALLEL_BLOCK];
    uint32_t write_idx[PARALLEL_BLOCK];
    uint32_t burst_size[PARALLEL_BLOCK];
    #pragma HLS ARRAY PARTITION variable=read_size  dim=0 complete
    #pragma HLS ARRAY PARTITION variable=write_size dim=0 complete
    #pragma HLS ARRAY PARTITION variable=write_idx dim=0 complete
    #pragma HLS ARRAY PARTITION variable=burst_size dim=0 complete

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
        for (; done==false; ){
            #pragma HLS PIPELINE II=1 
            for (uint8_t pb = 0; pb < NUM_BLOCKS; pb++){
            #pragma HLS UNROLL
                if(!endOfStream[pb].empty()){ 
                    bool eos_flag = endOfStream[pb].read(); 
                    local_buffer[pb][write_idx[pb]] = inStream[pb].read(); 
                    if (eos_flag){ 
                        end_of_stream.range(pb,pb) = 1; 
                        done = true; 
                    }else{ 
                        read_size[pb] += 1; 
                        write_idx[pb] += 1; 
                    } 
                    if (read_size[pb] >= BURST_SIZE){ 
                        done = true; 
                    } 
                    burst_size[pb] = c_max_burst_size; 
                    if(end_of_stream.range(pb,pb) && (read_size[pb] - write_size[pb]) < burst_size[pb]){ 
                        burst_size[pb] = (read_size[pb]>write_size[pb])?(read_size[pb]-write_size[pb]):0;
                    } 
                }  
            }
        }

        for(int i = 0; i < PARALLEL_BLOCK; i++){
            //Write the data to global memory
            if((read_size[i]> write_size[i]) && (read_size[i] - write_size[i]) >= burst_size[i]){
                uint32_t base_addr = output_idx[i]/c_word_size;
                uint32_t base_idx = base_addr + write_size[i];
                uint32_t burst_size_in_words = (burst_size[i])?burst_size[i]:0;
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
            if(end_of_stream.range(i,i)){
                is_pending.range(i,i) = 0;                
            }
            else{
                is_pending.range(i,i) = 1;
            }
       }
    }

    for (uint8_t i = 0; i < PARALLEL_BLOCK; i++){
        #pragma HLS PIPELINE II=1
        output_size[i] = compressedSize[i].read();
    }
}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH, int NUM_BLOCKS>
void    s2mm_nb(
        ap_uint<DATAWIDTH>                  *out, 
        const uint32_t                      output_idx[PARALLEL_BLOCK],
        hls::stream<ap_uint<DATAWIDTH> >    inStream[PARALLEL_BLOCK],
        const STREAM_SIZE_DT                input_size[PARALLEL_BLOCK]
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
            for (uint8_t pb = 0; pb < NUM_BLOCKS; pb++) {
            #pragma HLS UNROLL
                burst_size[pb] = c_max_burst_size;
                if(((input_size[pb] - write_size[pb]) < burst_size[pb])){
                    burst_size[pb] = (input_size[pb]>write_size[pb])?(input_size[pb]-write_size[pb]):0;
                }
                if(((read_size[pb] - write_size[pb]) < burst_size[pb]) && (input_size[pb] > read_size[pb])){
                    bool is_empty = inStream[pb].empty();
                    if(!is_empty){
                        local_buffer[pb][write_idx[pb]] = inStream[pb].read();
                        write_idx[pb] += 1;
                        read_size[pb] += c_word_size;
                        is_pending.range(pb,pb) = true;
                    }else{
                        is_pending.range(pb,pb) = false;
                    }
                }
                else{
                    if(burst_size[pb]) done = true;
                }
            }
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
