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
#define GET_DIFF_IF_BIG(x,y)   (x>y)?(x-y):0

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
                    read_size[i] += c_word_size; \
                    is_pending.range(i,i) = true;\
                }else{ \
                    is_pending.range(i,i) = false; \
                }\
            } \
            else{ \
                if(burst_size[i]) done = true; \
                if (read_size[i] >= input_size[i]) is_pending.range(i,i) = false; \
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

    ap_uint<OUT_WIDTH> outBuffer = 0; 
    ap_uint<IN_WIDTH> outBuffer_int[c_upsize_factor]; 
#pragma HLS array_partition variable=outBuffer_int dim=1 complete
    uint32_t byteIdx = 0;
    //printme("%s: factor=%d\n",__FUNCTION__,c_upsize_factor);
    for (SIZE_DT size = inStreamSize.read() ; size != 0 ; size = inStreamSize.read()){
        //rounding off the output size
        uint16_t outSize = ((size+byteIdx)/c_upsize_factor ) * c_upsize_factor;
        if (outSize) {
            outStreamSize << outSize;
        }
        //printme("%s: reading next data=%d outSize=%d c_in_size=%d\n ",__FUNCTION__, size,outSize,c_in_size);
        stream_upsizer:for (uint16_t i = 0 ; i < size ; i+=c_in_size ){
        #pragma HLS PIPELINE II=1
            uint16_t chunk_size=c_in_size;
            if (chunk_size + i > size) chunk_size = size-i;
            ap_uint<IN_WIDTH> tmpValue = inStream.read();
            outBuffer_int[byteIdx] = tmpValue;
            for(int j=0;j<c_upsize_factor;j+=c_in_size)
            {
             #pragma HLS unroll
             outBuffer.range((j+1)*c_byte_width-1,j*c_byte_width) = outBuffer_int[j];
            }
            byteIdx +=chunk_size;
            ////printme("%s: value=%c, chunk_size = %d and byteIdx=%d\n",__FUNCTION__,(char)tmpValue, chunk_size,byteIdx);
            if (byteIdx >= c_upsize_factor) {
                outStream << outBuffer;
                byteIdx -=c_upsize_factor;
            }
        }
    }
    if (byteIdx){
        outStreamSize << byteIdx;
        ////printme("sent outSize %d \n", byteIdx);
        outStream << outBuffer;
    }
    //end of block
    outStreamSize << 0;
    //printme("%s:Ended \n",__FUNCTION__);
}

template <class STREAM_SIZE_DT, int BURST_SIZE, int DATAWIDTH>
void    s2mm_compress(
        ap_uint<DATAWIDTH>                  *out, 
        const uint32_t                      output_idx[PARALLEL_BLOCK],
        hls::stream<ap_uint<DATAWIDTH> >    inStream[PARALLEL_BLOCK],
        hls::stream<uint16_t>               inStreamSize[PARALLEL_BLOCK],
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
        for(int i = 0; i < PARALLEL_BLOCK; i++){
        STREAM_UTILS_S2MM_READ_SIZE(i,inStreamSize[i],end_of_stream);
    } 
        done = false;
        for (int i = 0 ; (is_pending != 0) && (done == 0) ; i++ ){
            #pragma HLS PIPELINE II=1 
            for(int i = 0; i < PARALLEL_BLOCK; i++ ){                
                STREAM_UTILS_S2MM_IF_NOT_EMPTY(i,inStream[i],burst_size,input_size,read_size,write_size,write_idx); 
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
