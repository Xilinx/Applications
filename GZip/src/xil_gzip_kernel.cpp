/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include <ap_int.h>
#include "xil_gzip_config.h"

//LZ77 specific Defines
#define BIT 8
#define LZ77_HASH_BIT 10
#define LZ77_TABLE_SIZE (1 << LZ77_HASH_BIT)
#define LZ77_MAX_OFFSET_LIMIT 4096
#define MATCH_LEN (1*VEC)
#define SEEK_WINDOW (MATCH_LEN + VEC)
#define XGZIP 1
typedef ap_uint<VEC * BIT> uintV_t;
typedef ap_uint<MATCH_LEN * BIT> uintMatchV_t;
typedef ap_uint<MATCH_LEN * BIT + 32> uintDictV_t;

#define OUT_VEC (VEC) 
typedef ap_uint< OUT_VEC * BIT> uintOutV_t;
typedef ap_uint<GMEM_DWIDTH> uint512_t;
typedef ap_uint<32> encoded_dt;
typedef ap_uint<VEC*32> encodedV_dt;

template <int DATAWIDTH, int BURST_SIZE>
void gmem_to_stream(const ap_uint<DATAWIDTH>* in, 
                    hls::stream<ap_uint<DATAWIDTH> > &outStream, 
                    uint32_t input_size)
{
    const int c_byte_size = 8;
    const int c_word_size = DATAWIDTH/c_byte_size;
    uint32_t  sizeInWord = (input_size -1)/c_word_size + 1;
    ap_uint<DATAWIDTH> buffer[BURST_SIZE];
    
    for (uint32_t i = 0 ; i < sizeInWord; i+=BURST_SIZE){
        uint32_t chunk_size = BURST_SIZE; 
        if (i+BURST_SIZE> sizeInWord) chunk_size = sizeInWord-i;
        mrd1:for (uint32_t j = 0 ; j < chunk_size ;j++){
        #pragma HLS PIPELINE II=1
            buffer[j] = in[i+j];
        }
        mrd2:for (uint32_t j = 0 ; j < chunk_size ; j++) {
        #pragma HLS PIPELINE II=1
            outStream <<  buffer[j];
        }
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void streamDownsizer(
        hls::stream<ap_uint<IN_WIDTH>  > &inStream,
        hls::stream<ap_uint<OUT_WIDTH> > &outStream,
        SIZE_DT input_size)
{
    const int c_byte_width = 8;
    const int c_input_word = IN_WIDTH/c_byte_width;
    const int c_out_word   = OUT_WIDTH/c_byte_width;
    long sizeOutputV   = (input_size -1)/c_out_word + 1;
    int factor = c_input_word / c_out_word;
    ap_uint<IN_WIDTH> inBuffer= 0;
    
    conv512toV:for (int i = 0 ; i < sizeOutputV ; i++){
    #pragma HLS PIPELINE II=1
        int idx = i % factor;
        if (idx == 0)  inBuffer= inStream.read();
        ap_uint<OUT_WIDTH> tmpValue = inBuffer.range((idx+1)*VEC*8 -1, idx*VEC*8);
        outStream << tmpValue;
    }
}

template <class SIZE_DT, int IN_WIDTH, int OUT_WIDTH>
void streamUpsizer(
        hls::stream<ap_uint<IN_WIDTH> >     &inStream,
        hls::stream<ap_uint<OUT_WIDTH> >    &outStream)
{
    //Constants
    const int c_byte_width = 8 ; // 8bit is each BYTE
    const int c_upsize_factor = OUT_WIDTH/ c_byte_width;
    const int c_in_size = IN_WIDTH / c_byte_width;

    ap_uint<2 * OUT_WIDTH> outBuffer = 0; // Declaring double buffers 
    uint32_t byteIdx = 0; 
    
    for (SIZE_DT size = 1 ; size != 0 ; ){
        size = inStream.read();
        //rounding off the output size
        uint16_t outSize = ((size+byteIdx)/c_upsize_factor ) * c_upsize_factor;
        if (outSize) outStream << outSize;
        
        streamUpsizer:for (int i = 0 ; i < size ; i+=c_in_size ){
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
        outStream << byteIdx;
        outStream << outBuffer.range(OUT_WIDTH-1,0);
    }
    //end of block
    outStream << 0;
}

template <class STREAM_SIZE_DT, int DATAWIDTH>
uint32_t stream_to_gmem(
        ap_uint<DATAWIDTH>                  *out, 
        hls::stream<ap_uint<DATAWIDTH> >    &inStream)
{
    const int c_byte_size = 8 ;
    const int c_factor = DATAWIDTH/c_byte_size;
    
    uint32_t outIdx = 0 ;
    uint32_t size = 1;
    uint32_t sizeIdx=0;
    uint32_t total_size=0;
    for( outIdx = 0 ; size != 0 ; ) 
    {
        size = inStream.read();
        total_size += size;
        uint32_t sizeInWord = size?((size-1)/c_factor + 1):0;
        
        mwr:for (int i = 0 ; i < sizeInWord ; i++){
        #pragma HLS PIPELINE II=1
            out[outIdx + i] = inStream.read();
        }
        outIdx +=sizeInWord;
    }
    return total_size;
}


void splitter(
        hls::stream<encodedV_dt> &inStream,          
        hls::stream<encodedV_dt> &outStream0, 
        hls::stream<encodedV_dt> &outStream1, 
        hls::stream<encodedV_dt> &outStream2, 
        hls::stream<encodedV_dt> &outStream3, 
        hls::stream<encodedV_dt> &outStream4, 
        hls::stream<encodedV_dt> &outStream5, 
        hls::stream<encodedV_dt> &outStream6, 
        hls::stream<encodedV_dt> &outStream7, 
        long input_size                      
        )
{
    int uIdx=0;
    for (int i =0 ; i < input_size; i += BLOCK_PARITION, uIdx = (uIdx+1)%VEC){
        int chunk_size = BLOCK_PARITION;
        if (chunk_size + i > input_size) chunk_size = input_size - i;
        switch(uIdx){
                case 0: outStream0 << chunk_size; break; 
                case 1: outStream1 << chunk_size; break; 
                case 2: outStream2 << chunk_size; break; 
                case 3: outStream3 << chunk_size; break; 
                case 4: outStream4 << chunk_size; break; 
                case 5: outStream5 << chunk_size; break; 
                case 6: outStream6 << chunk_size; break; 
                case 7: outStream7 << chunk_size; break; 
        };
        
        splitter_main:for (int j = 0 ; j < chunk_size-VEC; j+=VEC){
        #pragma HLS PIPELINE II=1
            encodedV_dt inData = inStream.read();
            switch(uIdx){
                    case 0: outStream0 << inData; break; 
                    case 1: outStream1 << inData; break; 
                    case 2: outStream2 << inData; break; 
                    case 3: outStream3 << inData; break; 
                    case 4: outStream4 << inData; break; 
                    case 5: outStream5 << inData; break; 
                    case 6: outStream6 << inData; break; 
                    case 7: outStream7 << inData; break; 
            };
        }
        encodedV_dt inData = inStream.read();
        for (int v = 0 ; v < VEC ; v++){
        #pragma HLS UNROLL
            encoded_dt tmpData = inData.range((v+1)*32-1,v*32);
            tmpData.range(15,8) = 0;
            tmpData.range(31,16) = 0;
            inData.range((v+1)*32-1,v*32) = tmpData;
        }
        switch(uIdx){
                case 0: outStream0 << inData;break; 
                case 1: outStream1 << inData;break; 
                case 2: outStream2 << inData;break; 
                case 3: outStream3 << inData;break; 
                case 4: outStream4 << inData;break; 
                case 5: outStream5 << inData;break; 
                case 6: outStream6 << inData;break; 
                case 7: outStream7 << inData;break; 
        };
    }
    //End of Stream Data
    outStream0 << 0; 
    outStream1 << 0; 
    outStream2 << 0; 
    outStream3 << 0; 
    outStream4 << 0; 
    outStream5 << 0; 
    outStream6 << 0; 
    outStream7 << 0; 
}

int bitPacking(
        hls::stream<uintOutV_t> &outStream,
        hls::stream<uintOutV_t> &inStream, hls::stream<uint16_t> &inStreamSize,
        int prevSize, ap_uint< 2 * OUT_VEC * BIT> &bufferOut, int &bitIdx)
{
    int size = prevSize;
    if (size)   size  = inStreamSize.read();
    
    int sizeInOutV = size / (OUT_VEC * 8);
    int leftOverBit = size % (OUT_VEC * 8);
    int sizeOut = ((size+bitIdx)/(OUT_VEC * 8)) * OUT_VEC;
    if (sizeOut) outStream <<  sizeOut;
    bitPacking: 
    for (int i = 0 ; i < sizeInOutV; i++){
    #pragma HLS PIPELINE II=1
        uintOutV_t inputValue = inStream.read();
        bufferOut.range(bitIdx+(OUT_VEC*8)-1,bitIdx) = inputValue;
        outStream << bufferOut.range(OUT_VEC*8-1,0);
        bufferOut >>= OUT_VEC * 8;
    }
    if (leftOverBit){
        
        uintOutV_t inputValue = inStream.read();
        bufferOut.range(bitIdx+leftOverBit-1,bitIdx) = inputValue.range(leftOverBit-1,0);
        bitIdx += leftOverBit;
    }
    if(bitIdx >= OUT_VEC*8){
        
        outStream << bufferOut.range(OUT_VEC*8-1,0);
        bufferOut >>= (OUT_VEC * 8);
        bitIdx -= (OUT_VEC * 8);
        sizeInOutV++;
    }
    
    return size;
}

void merger(
        hls::stream<uintOutV_t> &outStream, 
        hls::stream<uintOutV_t> &inStream0, hls::stream<uint16_t> &inStreamSizeInBits0,       
        hls::stream<uintOutV_t> &inStream1, hls::stream<uint16_t> &inStreamSizeInBits1,       
        hls::stream<uintOutV_t> &inStream2, hls::stream<uint16_t> &inStreamSizeInBits2,       
        hls::stream<uintOutV_t> &inStream3, hls::stream<uint16_t> &inStreamSizeInBits3,       
        hls::stream<uintOutV_t> &inStream4, hls::stream<uint16_t> &inStreamSizeInBits4,       
        hls::stream<uintOutV_t> &inStream5, hls::stream<uint16_t> &inStreamSizeInBits5,       
        hls::stream<uintOutV_t> &inStream6, hls::stream<uint16_t> &inStreamSizeInBits6,       
        hls::stream<uintOutV_t> &inStream7, hls::stream<uint16_t> &inStreamSizeInBits7,
        uint32_t input_size
        )
{
    int idx = 0;
    int input_idx = 0;
    
    int sizeInBits[VEC];
    for (int i = 0 ;  i < VEC ; i++){
        sizeInBits[i] = 1;
    }
    // Modify at this location to support static block - 3 bits / 
    // STATIC_BLOCK 1, 1<<1 = 2 
    ap_uint< 2 * OUT_VEC * BIT> bufferOut;
    int bitIdx=0;
    int itr=0;
    bool eob_flag=false;
    for (int overallSize=1; overallSize != 0 ; ){
        input_idx += 8 *1024;
        ap_uint<3> start_of_block = 3;
        if (input_idx < input_size){
            start_of_block = 2;
        }
        itr++;
        if(itr%2 == 1){
            bufferOut.range(bitIdx+3-1,bitIdx) = start_of_block;
            bitIdx+=3;
            if(bitIdx >= OUT_VEC*8){
                outStream << OUT_VEC;
                outStream << bufferOut.range(OUT_VEC*8-1,0);
                bufferOut >>= (OUT_VEC * 8);
                bitIdx -= (OUT_VEC * 8);
            }
        }
        sizeInBits[0]= bitPacking(outStream,inStream0,inStreamSizeInBits0, sizeInBits[0],bufferOut,bitIdx);
        sizeInBits[1]= bitPacking(outStream,inStream1,inStreamSizeInBits1, sizeInBits[1],bufferOut,bitIdx);
        sizeInBits[2]= bitPacking(outStream,inStream2,inStreamSizeInBits2, sizeInBits[2],bufferOut,bitIdx);
        sizeInBits[3]= bitPacking(outStream,inStream3,inStreamSizeInBits3, sizeInBits[3],bufferOut,bitIdx);
        sizeInBits[4]= bitPacking(outStream,inStream4,inStreamSizeInBits4, sizeInBits[4],bufferOut,bitIdx);
        sizeInBits[5]= bitPacking(outStream,inStream5,inStreamSizeInBits5, sizeInBits[5],bufferOut,bitIdx);
        sizeInBits[6]= bitPacking(outStream,inStream6,inStreamSizeInBits6, sizeInBits[6],bufferOut,bitIdx);
        sizeInBits[7]= bitPacking(outStream,inStream7,inStreamSizeInBits7, sizeInBits[7],bufferOut,bitIdx);
        overallSize = 0;
        for (int i = 0 ; i < VEC ; i++){
            overallSize += sizeInBits[i];
        }
        if(itr%2 == 0){
            //Adding zero for End of Block 
            bufferOut.range(bitIdx+7-1,bitIdx) = 0;
            bitIdx+=7;
            eob_flag = true;
            if(bitIdx >= OUT_VEC*8){
                outStream<< OUT_VEC;
                outStream << bufferOut.range(OUT_VEC*8-1,0);
                bufferOut >>= (OUT_VEC * 8);
                bitIdx -= (OUT_VEC * 8);
            }
        }else{
            eob_flag = false;
        }
    }
    if (eob_flag != true ){
        //Adding zero for End of Block 
        bufferOut.range(bitIdx+7-1,bitIdx) = 0;
        bitIdx+=7;
    }
    //Adding zero for End of Block 
    int extraOutSize = (bitIdx-1)/8 + 1;
    outStream << extraOutSize;
    for(int i = 0 ; i < bitIdx ; i += (OUT_VEC*8)){
        outStream << bufferOut.range(OUT_VEC*8-1,0);
        bufferOut >>= OUT_VEC * 8;
    }
    
    //end of Stream
    outStream << 0;
}

// HUFFMAN specific
void fixedHuffman(
        hls::stream<encodedV_dt> &inStream, 
        hls::stream<uintOutV_t> &outStream, hls::stream<uint16_t> &outStreamSizeInBits       
)
{
    #include "huffman_fixed_table.h"
    uintOutV_t tmpOut;  
    for ( uint16_t size = inStream.read(); size != 0 ; size = inStream.read()) {
        bool is_encoded = false;  
        uint32_t outByteCnt = 0;  
        uint8_t length = 0;
        uint8_t value1,value2;
        encodedV_dt currV;
        uint32_t loc_idx = 0; 
        uint16_t localB[OUT_VEC];
        #pragma HLS ARRAY_PARTITION variable=localB dim=0 complete  
        ap_uint<64> localBits=0;  
        uint32_t localBits_idx=0;
        for (unsigned int i = 0;  i < OUT_VEC; i++){  
            #pragma HLS UNROLL
            localB[i] = 0;  
        }  
        huffman_loop:for (long i = 0; i < size; i++){
        #pragma HLS PIPELINE  II=1
            is_encoded = true;
            int v = i % VEC;
            if (v == 0) currV =  inStream.read(); 
            encoded_dt tmpEncodedValue = currV.range((v+1)*32-1,v*32);
            uint8_t tCh      = tmpEncodedValue.range(7,0);
            uint8_t tLen     = tmpEncodedValue.range(15,8);
            uint16_t tOffset = tmpEncodedValue.range(31,16);
            if (length){ 
                --length;
            }else if (tLen > 0){
                length = tLen - 1;  
                // Pick hash table index based on length
                uint16_t length_final = fixed_len_code[tLen];
    
                // Encode Length first with LenLit table
                uint8_t length_code = fixed_lenlit_table[length_final];
                uint8_t bit_rep = fixed_lenlit_bl[length_final];
                localBits.range(bit_rep+localBits_idx-1,localBits_idx) = length_code; 
                localBits_idx +=bit_rep;

                // --------------------------------------------------
                
                //      Encode distance using distance huffman
                
                // --------------------------------------------------
                
                // This contains the codes 0-29 range, 1 to 4096
                uint32_t distance = tOffset;  
                uint16_t distance_code     = fixed_dist_table[distance];
                uint8_t bit_length_dist    = fixed_dist_bl[distance];
                localBits.range(bit_length_dist+localBits_idx-1,localBits_idx) = distance_code;
                localBits_idx +=bit_length_dist;
                
            }else{  
                uint16_t lenlit_code   = fixed_lenlit_table[tCh];
                uint8_t bit_length     = fixed_lenlit_bl[tCh];
                localBits.range(bit_length+localBits_idx-1,localBits_idx) = lenlit_code;
                localBits_idx +=bit_length;
            }
            if(localBits_idx >= 16){
                ap_uint<16> pack_byte = 0;
                // Pack byte
                pack_byte = localBits.range(15,0);
                localBits >>= 16;
                localBits_idx -=16;
                localB[loc_idx++] = pack_byte.range(15,0);
            }
            if(loc_idx >= OUT_VEC/2){  
                for (int j = 0 ; j < OUT_VEC/2; j++)   
                    tmpOut.range(16*(j+1)-1, 16*j) = localB[j];  
                for (int j = 0 ; j < OUT_VEC/2; j++)   
                    localB[j] = localB[OUT_VEC/2+j];  
                for (int j = 0 ; j < OUT_VEC/2; j++)   
                    localB[OUT_VEC/2+j] = 0;  
                outStream << tmpOut;
                outByteCnt += OUT_VEC;  
                loc_idx -= OUT_VEC/2;  
            } 
        }
        outByteCnt += (2*loc_idx);
        uint32_t totalOutBits = outByteCnt * 8 + localBits_idx; 
        outStreamSizeInBits << totalOutBits;
        for (int i = 0; i < localBits_idx ; i +=16) {
            uint16_t pack_byte = 0;
            pack_byte = localBits.range(15,0);
            // Write packed byte to output buffer 
            localB[loc_idx++] = pack_byte; 
            localBits >>= 16;
        } 
        for(int i = 0 ; i < loc_idx ;  i +=(OUT_VEC/2) ){  
            for (int j = 0 ; j < OUT_VEC/2; j++)   
                tmpOut.range(16*(j+1)-1, 16*j) = localB[j];  
            for (int j = 0 ; j < OUT_VEC/2; j++)   
                localB[j] = localB[OUT_VEC/2+j];  
            for (int j = 0 ; j < OUT_VEC/2; j++)   
                localB[OUT_VEC/2+j] = 0;  
            outStream << tmpOut;
        }
    }  
    outStreamSizeInBits << 0;
}
void huffman_encode(
        hls::stream<encodedV_dt> &inStream,          
        hls::stream<uintOutV_t>  &outStream,
        long input_size                      
        )
{
    const int c_stream_depth = 2 * ( BLOCK_PARITION / VEC);
    hls::stream<encodedV_dt> inStream0;
    hls::stream<encodedV_dt> inStream1;
    hls::stream<encodedV_dt> inStream2;
    hls::stream<encodedV_dt> inStream3;
    hls::stream<encodedV_dt> inStream4;
    hls::stream<encodedV_dt> inStream5;
    hls::stream<encodedV_dt> inStream6;
    hls::stream<encodedV_dt> inStream7;
    hls::stream<uintOutV_t>  outStream0   ;
    hls::stream<uintOutV_t>  outStream1   ;
    hls::stream<uintOutV_t>  outStream2   ;
    hls::stream<uintOutV_t>  outStream3   ;
    hls::stream<uintOutV_t>  outStream4   ;
    hls::stream<uintOutV_t>  outStream5   ;
    hls::stream<uintOutV_t>  outStream6   ;
    hls::stream<uintOutV_t>  outStream7   ;
    hls::stream<uint16_t>    outStreamSizeInBits0;
    hls::stream<uint16_t>    outStreamSizeInBits1;
    hls::stream<uint16_t>    outStreamSizeInBits2;
    hls::stream<uint16_t>    outStreamSizeInBits3;
    hls::stream<uint16_t>    outStreamSizeInBits4;
    hls::stream<uint16_t>    outStreamSizeInBits5;
    hls::stream<uint16_t>    outStreamSizeInBits6;
    hls::stream<uint16_t>    outStreamSizeInBits7;
    #pragma HLS STREAM variable=inStream0             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream1             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream2             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream3             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream4             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream5             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream6             depth=c_stream_depth
    #pragma HLS STREAM variable=inStream7             depth=c_stream_depth
    #pragma HLS STREAM variable=outStream0            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream1            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream2            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream3            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream4            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream5            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream6            depth=c_stream_depth
    #pragma HLS STREAM variable=outStream7            depth=c_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits0  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits1  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits2  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits3  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits4  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits5  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits6  depth=c_size_stream_depth
    #pragma HLS STREAM variable=outStreamSizeInBits7  depth=c_size_stream_depth


    #pragma HLS dataflow
    splitter(inStream,
            inStream0, inStream1, inStream2, inStream3,
            inStream4, inStream5, inStream6, inStream7,
            input_size 
            );
    fixedHuffman(inStream0,outStream0,outStreamSizeInBits0);
    fixedHuffman(inStream1,outStream1,outStreamSizeInBits1);
    fixedHuffman(inStream2,outStream2,outStreamSizeInBits2);
    fixedHuffman(inStream3,outStream3,outStreamSizeInBits3);
    fixedHuffman(inStream4,outStream4,outStreamSizeInBits4);
    fixedHuffman(inStream5,outStream5,outStreamSizeInBits5);
    fixedHuffman(inStream6,outStream6,outStreamSizeInBits6);
    fixedHuffman(inStream7,outStream7,outStreamSizeInBits7);
    merger( 
            outStream,
            outStream0, outStreamSizeInBits0,
            outStream1, outStreamSizeInBits1,
            outStream2, outStreamSizeInBits2,
            outStream3, outStreamSizeInBits3,
            outStream4, outStreamSizeInBits4,
            outStream5, outStreamSizeInBits5,
            outStream6, outStreamSizeInBits6,
            outStream7, outStreamSizeInBits7,
            input_size);
}
// LZ77 compress module
void lz77_encode(
        hls::stream<uintV_t> &inStream,          
        hls::stream<encodedV_dt> &outStream,          
        long input_size                      
    )
{
    // Look ahead buffer
    uint8_t present_window[SEEK_WINDOW];
    #pragma HLS ARRAY_PARTITION variable=present_window complete 

    // History Dictionaries
    uintDictV_t history_table[VEC][VEC][LZ77_TABLE_SIZE];   
    #pragma HLS ARRAY_PARTITION variable=history_table dim=1 complete
    #pragma HLS ARRAY_PARTITION variable=history_table dim=2 complete

    // Hold window        
    uintMatchV_t hold_window[VEC][VEC];
    #pragma HLS ARRAY_PARTITION variable=hold_window dim=1 complete
    #pragma HLS ARRAY_PARTITION variable=hold_window dim=2 complete
 
    // Hold index 
    int32_t hold_idx[VEC][VEC];  
    #pragma HLS ARRAY_PARTITION variable=hold_idx dim=1 complete 
    #pragma HLS ARRAY_PARTITION variable=hold_idx dim=2 complete 

    uint32_t out_cntr = 0;

    // Flush main history_hash_table and index history_hash_table
    uintDictV_t resetValue= 0;
    resetValue.range(MATCH_LEN*BIT+31, MATCH_LEN*BIT) = -1;

    // Initialize history tables
    flush: for(int i = 0; i < LZ77_TABLE_SIZE; i++) {
    #pragma HLS PIPELINE II=1
    #pragma HLS UNROLL FACTOR=2
       for(int j = 0; j < VEC; j++) {
       #pragma HLS UNROLL
            for (int k = 0; k < VEC; k++){
            #pragma HLS UNROLL
                history_table[k][j][i] = resetValue;
            }
        }
    }

    //Pre-filled the current window
    for(int m = 0; m < MATCH_LEN; m+=VEC){
        uintV_t tmpValue = inStream.read();
        for(int i = 0; i < VEC; i++){
        #pragma HLS UNROLL
            present_window[VEC+ m + i] = tmpValue.range(i * BIT + BIT - 1, i * BIT);
        }
    }

    // Holds best index value
    int32_t good_idx[VEC];
    #pragma HLS ARRAY_PARTITION variable=good_idx complete
    
    uint32_t inputSizeV = input_size/VEC;
    uint32_t leftOverBytes = input_size%VEC;
    
    // Run over input data
    lz77_main: for(int inIdx = MATCH_LEN/VEC; inIdx < inputSizeV; inIdx++) {
    #pragma HLS PIPELINE II=1
    #pragma HLS dependence variable=history_table inter false
        /************************************************************************
        *    Fetch Input Data Start
        ***********************************************************************/
       
        //shift current window
        fetch_in1: for(int m = 0; m < MATCH_LEN; m++)
            present_window[m] = present_window[VEC + m];
       
        // load new values
        uintV_t tmpValue = inStream.read(); 
        fetch_in2: for(int m = 0; m < VEC; m++){
            present_window[MATCH_LEN + m] = tmpValue.range(m * BIT + BIT - 1, m * BIT);
        }

        /************************************************************************
        *    Fetch Input Data End 
        ***********************************************************************/

        /************************************************************************/
        //         History Hash Table Read/Write  
        /***********************************************************************/

        uint32_t hash[VEC];
        int32_t present_idx[VEC]; 
        uintMatchV_t present_windowV[VEC];  
        hash_cal: for (int i = 0 ; i < VEC ; i++) {
        #pragma HLS UNROLL
            present_idx[i] = i + inIdx * VEC;
            hash[i] = (present_window[i] << 2)     ^
                      (present_window[i + 1] << 1) ^
                      (present_window[i + 2])      ^
                      (present_window[i + 3]);

            uintMatchV_t tmpDictValue;
            for (int j = 0 ; j < MATCH_LEN ; j++){
                tmpDictValue.range(j * BIT + BIT - 1, j * BIT) = present_window[i+j];
            }
            present_windowV[i] = tmpDictValue;
        }
        // Calculate hash & history history_table search
        history_table_lookup: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            // Run over literals
            history_table2: for(int j = 0; j < VEC ; j++) {
            #pragma HLS UNROLL
                // Loop over history_tableionaries
                uintDictV_t tmpValue  = history_table[i][j][hash[i]];
                hold_window[i][j] = tmpValue.range(MATCH_LEN*BIT -1,0);
                hold_idx[i][j]    = tmpValue.range(MATCH_LEN*BIT +31, MATCH_LEN*BIT);
            }
        }
        history_table_update: for (int i = 0 ; i < VEC ; i++){
        #pragma HLS UNROLL
            for (int m = 0 ; m < VEC ; m++){
            #pragma HLS UNROLL
                uintDictV_t tmpValue;
                tmpValue.range(MATCH_LEN*BIT-1,0) = present_windowV[i];
                tmpValue.range(MATCH_LEN*BIT+31, MATCH_LEN*BIT)= present_idx[i];
                history_table[m][i][hash[i]] = tmpValue;
            }
        }
        
        /************************************************************************/
        //          Dictionary Lookup and Update Module -- End
        /***********************************************************************/
        

        /************************************************************************/
        //          Good Length and Offset Finder -- Start
        /***********************************************************************/
        // Match search and filtering

            
        // Hold history_table pick
        uint8_t hold_history_table[VEC];
        int8_t good_length[VEC];
        
        search_init: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            hold_history_table[i] = 0;
            good_length[i] = 0;
        }
        
        
        // Loop over hold slots
        length_seek1: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            int8_t length[VEC];
            // Loop over present data slot
            length_seek2: for(int j = 0; j < VEC; j++) {
                
                bool done = 0;
                int8_t temp = 0;
                // Compare present/Hold data
                uintMatchV_t tmpCompVal = hold_window[j][i];
                length_seek3 : for(int k = 0; k < MATCH_LEN; k++) {
                    if((present_window[j + k] == tmpCompVal.range(k * BIT + BIT - 1, k * BIT) && !done)) 
                        temp++;          
                    else
                        done = 1;
                }
                
                int32_t twl_bit = present_idx[i] - hold_idx[i][j];
                if(twl_bit >= LZ77_MAX_OFFSET_LIMIT){
                    length[j] = 0;
                }
                else {
                    length[j] = temp;
                }
            }

            // Update good length
            length_seek4: for(int m = 0; m < VEC; m++) {
                
                if(length[m] > good_length[m]) {
                    good_length[m] = length[m];
                    hold_history_table[m] = i;               
                }
            }

        }//end

        bestid: for(int s = 0; s < VEC; s++){
            good_idx[s] = hold_idx[s][hold_history_table[s]];
        }
        
        uint32_t good_distance[VEC];
        int8_t good_length_0[VEC];
        
        // Find the good distance 
        find_main : for(int i = 0; i < VEC; i++) {
            int32_t distance = present_idx[i] - good_idx[i] - 1;  
            
            if( good_length[i] >= 4  && distance < LZ77_MAX_OFFSET_LIMIT && distance < present_idx[i] && (good_length[i] < distance)) {
                good_distance[i] = distance;
                good_length_0[i] = good_length[i];
            }else {
                good_distance[i] = 0;
                good_length_0[i] = 0;
            }
        }   
  
        /************************************************************************/
        //          Good Length and Offset Finder -- End
        /***********************************************************************/
        encodedV_dt tmpV;
        for (int i = 0 ; i < VEC ; i++){
             encoded_dt tmpValue;
             tmpValue.range(7,0)    = present_window[i];
             tmpValue.range(15,8)    = good_length_0[i];
             tmpValue.range(31,16)    = good_distance[i];
             tmpV.range((i+1)*32-1,i*32) = tmpValue;
            
        }
        outStream << tmpV;
        
    }

    for(int m = 0 ; m < MATCH_LEN ; m+=VEC){
        encodedV_dt tmpV;
        lz77_leftover1:for (int i = 0; i < VEC ; i++){  
        #pragma HLS UNROLL
             encoded_dt tmpValue;
             tmpValue.range(7,0)   = present_window[VEC + m + i];  
             tmpValue.range(15,8)  = 0;  
             tmpValue.range(31,16) = 0;  
             tmpV.range((i+1)*32-1,i*32) = tmpValue;  
        }
        outStream << tmpV;
    }

    if(leftOverBytes){
        //Loading leftover Bytes
        encodedV_dt tmpV;
        uintV_t tmpInputValue = inStream.read(); 
        lz77_leftover2:for (int i = 0; i < VEC ; i++){  
        #pragma HLS UNROLL
             encoded_dt tmpValue;
             tmpValue.range(7,0)    = tmpInputValue.range(8*(i+1)-1,8*i);  
             tmpValue.range(15,8)   = 0;  
             tmpValue.range(31,16)  = 0;  
             tmpV.range((i+1)*32-1,i*32) = tmpValue;  
        }
        outStream << tmpV;
    }
}
void gzip(
                const uint512_t *in,      
                uint512_t       *out,          
                uint32_t        *encoded_size,
                long input_size                      
                )
{
    hls::stream<uint512_t>   inStream512   ;
    hls::stream<uintV_t>     inStreamV     ;
    hls::stream<encodedV_dt> encodedStream;
    hls::stream<uintOutV_t>  outCompressedStream    ;
    hls::stream<uint512_t>   outStream512    ;
    #pragma HLS STREAM variable=inStream512         depth=c_gmem_burst_size
    #pragma HLS STREAM variable=inStreamV           depth=c_gmem_burst_size
    #pragma HLS STREAM variable=encodedStream       depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outCompressedStream depth=c_gmem_burst_size
    #pragma HLS STREAM variable=outStream512        depth=c_gmem_burst_size

    #pragma HLS dataflow
    gmem_to_stream<GMEM_DWIDTH,GMEM_BURST_SIZE>(in,inStream512,input_size);
    streamDownsizer<uint32_t, GMEM_DWIDTH, VEC * 8>(inStream512,inStreamV,input_size);
    lz77_encode(inStreamV, encodedStream,input_size);
    huffman_encode(encodedStream,outCompressedStream,input_size);
    streamUpsizer<uint16_t,OUT_VEC * 8, GMEM_DWIDTH>(outCompressedStream,outStream512);
    encoded_size[0] = stream_to_gmem<uint16_t,GMEM_DWIDTH>(out,outStream512);
}

extern "C"{
void gZip_cu(
             const uint512_t *in,      
             uint512_t       *out,          
             uint32_t        *encoded_size,
             long input_size                      
            )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=encoded_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=encoded_size bundle=control
    #pragma HLS INTERFACE s_axilite port=input_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS data_pack variable=in
    #pragma HLS data_pack variable=out

    gzip(in,out,encoded_size,input_size);

}
}
