/**********
Copyright (c) 2017, Xilinx, Inc.
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
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "hls_stream.h"
#include <ap_int.h>

#ifdef VEC_8
    #define VEC 8 
#else
    #define VEC 16
#endif

#define BIT 8
#define DICT_SIZE VEC
#define HASH_BIT 10
#define TABLE_SIZE (1 << HASH_BIT)
#define LEN VEC 
#define SLIDE_WINDOW (VEC * VEC)
#define SEEK_WINDOW (2 * VEC)

#define MARKER 255
#define MAX_OFFSET_LIMIT 4096
#define BUFFER_SIZE 16
typedef ap_uint<VEC * BIT> uintV_t;
typedef ap_uint<VEC * BIT + 32> uintDictV_t;

#define OUT_VEC (VEC) 
typedef ap_uint< OUT_VEC * BIT> uintOutV_t;
typedef ap_uint<512> uint512_t;
typedef ap_uint<32> encoded_dt;
typedef ap_uint<VEC*32> encodedV_dt;

#if (COMPUTE_UNIT == 1)
namespace  cu1 
#elif (COMPUTE_UNIT == 2)
namespace  cu2 
#elif (COMPUTE_UNIT == 3)
namespace  cu3 
#elif (COMPUTE_UNIT == 4)
namespace  cu4
#endif
{
int passthrough(
        hls::stream<uintOutV_t> &outStream, hls::stream<uint16_t> &outStreamSize,       
        hls::stream<uintOutV_t> &inStream, hls::stream<uint16_t> &inStreamSize,
        int inputSize );       

void gmem_read(const uint512_t* in, hls::stream<uint512_t> &outStream, long input_size)
{
    //printme("%s:inputsize=%d\n",__FUNCTION__, input_size);
    uint512_t  buffer[BUFFER_SIZE];
    long sizein512 = (input_size -1)/64 + 1;
    for (int i = 0 ; i < sizein512; i+=BUFFER_SIZE){
        int chunk_size = BUFFER_SIZE; 
        if (i+BUFFER_SIZE > sizein512) chunk_size = sizein512-i;
        mrd1:for (int j = 0 ; j < chunk_size ;j++){
        #pragma HLS PIPELINE
            buffer[j] = in[i+j];
        }
        mrd2:for (int j = 0 ; j < chunk_size ; j++) {
        #pragma HLS PIPELINE
            outStream <<  buffer[j];
        }
    }
}
void conv512toV(
        hls::stream<uint512_t> &inStream,
        hls::stream<uintV_t> &outStream,
        long input_size)
{
    long sizeinV   = (input_size -1)/VEC + 1;
    int factor = 64 / VEC;
    uint512_t tmp512Value = 0;
    //printme("%s:inputsize=%d sizeV=%d factor=%d\n",__FUNCTION__, input_size,sizeinV,factor);
    conv512toV:for (int i = 0 ; i < sizeinV ; i++){
    #pragma HLS PIPELINE
        int idx = i % factor;
        if (idx == 0)  tmp512Value = inStream.read();
        uintV_t tmpValue = tmp512Value.range((idx+1)*VEC*8 -1, idx*VEC*8);
        outStream << tmpValue;
    }
}
void convOutVto512(
        hls::stream<uintOutV_t> &inStream,
        hls::stream<uint16_t>   &inStreamSize,
        hls::stream<uint512_t>  &outStream,
        hls::stream<uint16_t>    &outStreamSize)
{
    int factor = 64 ;
    uint512_t tmp512Value = 0;
    //printme("%s: factor=%d\n",__FUNCTION__,factor);
    for (uint16_t size = 1 ; size != 0 ; ){
        size = inStreamSize.read();
        //printme("%s: reading next data=%d\n",__FUNCTION__, size);
        bool left_over=false;
        convVto512:for (int i = 0 ; i < size ; i+=OUT_VEC ){
        #pragma HLS PIPELINE 
            int idx = i % factor;
            uintOutV_t tmpValue = inStream.read();
            tmp512Value.range((idx+OUT_VEC)* 8 -1, idx*8) = tmpValue;
            if (idx +OUT_VEC == factor) {
                outStream << tmp512Value;
                left_over = false;
            }else{
                left_over = true;
            }
        }

        if (left_over){
            outStream << tmp512Value;
        }
        outStreamSize << size;
    }
    //printme("%s:Ended \n",__FUNCTION__);
}

void gmem_write(uint512_t* out, 
        hls::stream<uint512_t> &inStream,
        hls::stream<uint16_t> & inStreamSize,
        uint32_t  *encoded_size
        )
{
    //printme("%s:Started\n",__FUNCTION__);
    uint32_t outIdx = 0 ;
    uint32_t factor = 64;
    uint32_t next_addr = 2 * 1024/64;
    uint32_t size = 1;
    uint32_t sizeIdx=0;
    for( outIdx = 0 ; size != 0 ; outIdx += next_addr) {
        size = inStreamSize.read();
        uint32_t sizeIn512 = size?((size-1)/factor + 1):0;
        //printme("%s:outIdx=%d input size = %d sizeIn512 = %d \n",__FUNCTION__,outIdx, size,sizeIn512);
        mwr:for (int i = 0 ; i < sizeIn512 ; i++){
        #pragma HLS PIPELINE
            out[outIdx + i] = inStream.read();
        }
        encoded_size[sizeIdx++] = size;
    }
    //printme("%s:End\n",__FUNCTION__);
}
void splitter(
        hls::stream<encodedV_dt> &inStream,          
        hls::stream<encodedV_dt> &outStream0, hls::stream<uint16_t> &outStreamSize0,       
        hls::stream<encodedV_dt> &outStream1, hls::stream<uint16_t> &outStreamSize1,       
        hls::stream<encodedV_dt> &outStream2, hls::stream<uint16_t> &outStreamSize2,       
        hls::stream<encodedV_dt> &outStream3, hls::stream<uint16_t> &outStreamSize3,       
        hls::stream<encodedV_dt> &outStream4, hls::stream<uint16_t> &outStreamSize4,       
        hls::stream<encodedV_dt> &outStream5, hls::stream<uint16_t> &outStreamSize5,       
        hls::stream<encodedV_dt> &outStream6, hls::stream<uint16_t> &outStreamSize6,       
        hls::stream<encodedV_dt> &outStream7, hls::stream<uint16_t> &outStreamSize7,       
        long input_size                      
        )
{
    //printme("%s:input_size=%d\n",__FUNCTION__, input_size);  
    int uIdx=0;
    for (int i =0 ; i < input_size; i += 1024, uIdx = (uIdx+1)%VEC){
        int chunk_size = 1024;
        if (chunk_size + i > input_size) chunk_size = input_size - i;
        //printme("%s:chunk_size=%d\n",__FUNCTION__, chunk_size);  
        splitter_main:for (int j = 0 ; j < chunk_size-VEC; j+=VEC){
        #pragma HLS PIPELINE 
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
                case 0: outStream0 << inData;outStreamSize0 << chunk_size; break; 
                case 1: outStream1 << inData;outStreamSize1 << chunk_size; break; 
                case 2: outStream2 << inData;outStreamSize2 << chunk_size; break; 
                case 3: outStream3 << inData;outStreamSize3 << chunk_size; break; 
                case 4: outStream4 << inData;outStreamSize4 << chunk_size; break; 
                case 5: outStream5 << inData;outStreamSize5 << chunk_size; break; 
                case 6: outStream6 << inData;outStreamSize6 << chunk_size; break; 
                case 7: outStream7 << inData;outStreamSize7 << chunk_size; break; 
        };
    }
    //End of Stream Data
    outStreamSize0 << 0; 
    outStreamSize1 << 0; 
    outStreamSize2 << 0; 
    outStreamSize3 << 0; 
    outStreamSize4 << 0; 
    outStreamSize5 << 0; 
    outStreamSize6 << 0; 
    outStreamSize7 << 0; 
    //printme("%s:input_size=%d\n",__FUNCTION__, input_size);  
}
void merger(
        hls::stream<uintOutV_t> &outStream, hls::stream<uint16_t> &outStreamSize,       
        hls::stream<uintOutV_t> &inStream0, hls::stream<uint16_t> &inStreamSize0,       
        hls::stream<uintOutV_t> &inStream1, hls::stream<uint16_t> &inStreamSize1,       
        hls::stream<uintOutV_t> &inStream2, hls::stream<uint16_t> &inStreamSize2,       
        hls::stream<uintOutV_t> &inStream3, hls::stream<uint16_t> &inStreamSize3,       
        hls::stream<uintOutV_t> &inStream4, hls::stream<uint16_t> &inStreamSize4,       
        hls::stream<uintOutV_t> &inStream5, hls::stream<uint16_t> &inStreamSize5,       
        hls::stream<uintOutV_t> &inStream6, hls::stream<uint16_t> &inStreamSize6,       
        hls::stream<uintOutV_t> &inStream7, hls::stream<uint16_t> &inStreamSize7       
        )
{
    int idx = 0;
    //printme("%s:Starting\n",__FUNCTION__);  
    int size[VEC];
    for (int i = 0 ;  i < VEC ; i++){
        size[i] = 1;
    }
    for (int overallSize=1; overallSize != 0 ; ){
        size[0]= passthrough(outStream,outStreamSize,inStream0,inStreamSize0, size[0]);
        size[1]= passthrough(outStream,outStreamSize,inStream1,inStreamSize1, size[1]);
        size[2]= passthrough(outStream,outStreamSize,inStream2,inStreamSize2, size[2]);
        size[3]= passthrough(outStream,outStreamSize,inStream3,inStreamSize3, size[3]);
        size[4]= passthrough(outStream,outStreamSize,inStream4,inStreamSize4, size[4]);
        size[5]= passthrough(outStream,outStreamSize,inStream5,inStreamSize5, size[5]);
        size[6]= passthrough(outStream,outStreamSize,inStream6,inStreamSize6, size[6]);
        size[7]= passthrough(outStream,outStreamSize,inStream7,inStreamSize7, size[7]);

        overallSize = 0;
        for (int i = 0 ; i < VEC ; i++){
            overallSize += size[i];
        }
    }
    //end of Stream
    outStreamSize << 0;
}
int passthrough(
        hls::stream<uintOutV_t> &outStream, hls::stream<uint16_t> &outStreamSize,       
        hls::stream<uintOutV_t> &inStream, hls::stream<uint16_t> &inStreamSize,
        int prevSize
        )       
{
    int size = prevSize;
    if (size)   size  = inStreamSize.read();
    //printme("%s:Reading Size=%d\n",__FUNCTION__,size);  
    passthrough:for (int i = 0 ; i < size ; i+=OUT_VEC){
    #pragma HLS PIPELINE
        outStream << inStream.read();
    }
    if (size) outStreamSize << size;
    return size;
}
void bytePacking (
        hls::stream<encodedV_dt> &inStream, hls::stream<uint16_t> &inStreamSize,       
        hls::stream<uintOutV_t> &outStream, hls::stream<uint16_t> &outStreamSize       
)
{
    //printme("%s:Starting\n",__FUNCTION__);
    uint8_t marker = MARKER; 
  
    uintOutV_t tmpOut;  
    for ( uint16_t size = inStreamSize.read(); size != 0 ; size = inStreamSize.read()) {
        //printme("%s:first chunkSize=%d\n",__FUNCTION__,size);
        bool is_encoded = false;  
        uint32_t out_idx = 0;  
        uint8_t length = 0;
        uint8_t value1,value2;
        encodedV_dt currV;
        uint32_t loc_idx = 0; 
        uint8_t localB[2 * OUT_VEC];  
        #pragma HLS ARRAY_PARTITION variable=localB dim=0 complete  
        for (unsigned int i = 0;  i < 2 * OUT_VEC; i++){  
            #pragma HLS UNROLL
            localB[i] = 0;  
        }  
        byte_packing_wr:for (long i = 0; i < size; i++){
        #pragma HLS PIPELINE 
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
                uint32_t offset = tOffset;  
                length = tLen- 1;  
                uint8_t temp    = (length << 4) & 0xF0;  
                uint8_t temp1   = (offset >> 8) & 0x0F;  
                uint8_t res     = temp | temp1;  
                uint8_t temp2   = offset & 0xFF;  
                localB[loc_idx++] = marker;
                localB[loc_idx++] = res;
                localB[loc_idx++] = temp2;
            }else if (tCh== marker){
                localB[loc_idx++] = marker;  
                localB[loc_idx++] = 0;  
                localB[loc_idx++] = 0;  
            }else{  
                localB[loc_idx++] = tCh;  
            }  
            if (loc_idx >= OUT_VEC){  
                for (int j = 0 ; j < OUT_VEC; j++)   
                    tmpOut.range(8*(j+1)-1, 8*j) = localB[j];  
                for (int j = 0 ; j < OUT_VEC; j++)   
                    localB[j] = localB[OUT_VEC+j];  
                for (int j = 0 ; j < OUT_VEC; j++)   
                    localB[OUT_VEC+j] = 0;  
                outStream << tmpOut;
                out_idx += OUT_VEC;  
                loc_idx -= OUT_VEC;  
            } 
        }
        if (is_encoded){
            wr_leftover:for (int j = 0; j < OUT_VEC; j++){  
            #pragma HLS UNROLL
                tmpOut.range(8*(j+1)-1, 8*j) = localB[j];  
            }
            if (loc_idx) outStream << tmpOut; 
        }
        out_idx += loc_idx;  
        outStreamSize << out_idx;
    }  
    outStreamSize << 0;
    //printme("%s:End\n",__FUNCTION__);
}
// LZ77 compress module
void compress(
        hls::stream<uintV_t> &inStream,          
        hls::stream<encodedV_dt> &outStream,          
        long input_size                      
    )
{
    //printme("%s:Starting\n",__FUNCTION__);
    // Look ahead buffer
    uint8_t curr_window[SEEK_WINDOW];
    #pragma HLS ARRAY_PARTITION variable=curr_window complete 
    // History Dictionaries
    uintDictV_t dict[VEC][VEC][TABLE_SIZE];   
    #pragma HLS ARRAY_PARTITION variable=dict dim=1 complete
    #pragma HLS ARRAY_PARTITION variable=dict dim=2 complete

    // Comparison window        
    uintV_t comp_window[LEN][VEC];
    #pragma HLS ARRAY_PARTITION variable=comp_window dim=1 complete
    #pragma HLS ARRAY_PARTITION variable=comp_window dim=2 complete
 
    // Compare index 
    int32_t comp_idx[VEC][VEC];  
    #pragma HLS ARRAY_PARTITION variable=comp_idx dim=1 complete 
    #pragma HLS ARRAY_PARTITION variable=comp_idx dim=2 complete 

    uint32_t out_cntr = 0;

    // Flush main dictionary and index dictionary
    uintDictV_t zeroValue = 0;
    zeroValue.range(VEC*BIT+31, VEC*BIT) = -1;

    // Initialize history dictionaries
    flush: for(int i = 0; i < TABLE_SIZE; i++) {
    #pragma HLS PIPELINE
    #pragma HLS UNROLL FACTOR=2
       for(int j = 0; j < VEC; j++) {
       #pragma HLS UNROLL
            for (int k = 0; k < VEC; k++){
            #pragma HLS UNROLL
                dict[k][j][i] = zeroValue;
            }
        }
    }

    //shift current window
    uintV_t tmpValue = inStream.read();
    curr_win: for(int m = 0; m < VEC; m++){
    #pragma HLS UNROLL
        curr_window[VEC+m] = tmpValue.range(m * BIT + BIT - 1, m * BIT);
    }

    // Holds best index value
    int32_t best_idx[VEC];
    #pragma HLS ARRAY_PARTITION variable=best_idx complete
    
    uint32_t inputSizeV = input_size/VEC;
    // Run over input data
    lz77_main: for(int inIdx = 1; inIdx < inputSizeV; inIdx++) {
    #pragma HLS PIPELINE ii=1
    #pragma HLS dependence variable=dict inter false
        /************************************************************************
        *    Shift module Start
        ***********************************************************************/
       
        //shift current window
        shift1: for(int m = 0; m < VEC; m++)
            curr_window[m] = curr_window[VEC + m];
       
        // load new values
        tmpValue = inStream.read(); 
        shift2: for(int m = 0; m < VEC; m++){
            curr_window[VEC + m] = tmpValue.range(m * BIT + BIT - 1, m * BIT);
        }

        /************************************************************************
        *    Shift module End 
        ***********************************************************************/

        /************************************************************************/
        //          Dictionary Lookup and Update Module Start
        /***********************************************************************/

        uint32_t hash[VEC];
        int32_t curr_idx[VEC]; 
        uintV_t  curr_windowV[VEC];  
        hash_cal: for (int i = 0 ; i < VEC ; i++) {
        #pragma HLS UNROLL
            curr_idx[i] = i + inIdx * VEC;
            hash[i] = (curr_window[i] << 2)     ^
                      (curr_window[i + 1] << 1) ^
                      (curr_window[i + 2])      ^
                      (curr_window[i + 3]);

            uintV_t tmpDictValue;
            for (int j = 0 ; j < VEC ; j++){
                tmpDictValue.range(j * BIT + BIT - 1, j * BIT) = curr_window[i+j];
            }
            curr_windowV[i] = tmpDictValue;
        }
        // Calculate hash & history dict search
        dict_lookup: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            // Run over literals
            dict2: for(int j = 0; j < VEC; j++) {
            #pragma HLS UNROLL
                // Loop over dictionaries
                uintDictV_t tmpValue  = dict[i][j][hash[i]];
                comp_window[i][j] = tmpValue.range(VEC*BIT -1,0);
                comp_idx[i][j]    = tmpValue.range(VEC*BIT +31, VEC*BIT);
            }
        }
        dict_update: for (int i = 0 ; i < VEC ; i++){
        #pragma HLS UNROLL
            for (int m = 0 ; m < VEC ; m++){
            #pragma HLS UNROLL
                uintDictV_t tmpValue;
                tmpValue.range(VEC*BIT-1,0) = curr_windowV[i];
                tmpValue.range(VEC*BIT+31, VEC*BIT)= curr_idx[i];
                dict[m][i][hash[i]] = tmpValue;
            }
        }
        
        /************************************************************************/
        //          Dictionary Lookup and Update Module -- End
        /***********************************************************************/
        

        /************************************************************************/
        //          Match Search and Filter -- Start
        /***********************************************************************/
        // Match search and filtering

            
        // Comp dict pick
        uint8_t comp_dict[VEC];
        int8_t best_length[VEC];
        
        search_init: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            comp_dict[i] = 0;
            best_length[i] = 0;
        }
        
        // Match search and reduction
        // Loop over comparison window
        search_main: for(int i = 0; i < VEC; i++) {
        #pragma HLS UNROLL
            int8_t length[VEC];
            // Loop over current window
            search2: for(int j = 0; j < VEC; j++) {
                
                bool done = 0;
                int8_t temp = 0;
                // Compare current/compare
                uintV_t tmpCompVal = comp_window[j][i];
                search3 : for(int k = 0; k < LEN; k++) {
                    if((curr_window[j + k] == tmpCompVal.range(k * BIT + BIT - 1, k * BIT) && !done)) 
                        temp++;          
                    else
                        done = 1;
                }
                
                int32_t twl_bit = curr_idx[i] - comp_idx[i][j];
                if(twl_bit >= MAX_OFFSET_LIMIT){
                    length[j] = 0;
                }
                else {
                    length[j] = temp;
                }
            }

            // update best length
            search4: for(int m = 0; m < VEC; m++) {
                
                if(length[m] > best_length[m]) {
                    best_length[m] = length[m];
                    comp_dict[m] = i;               
                }
            }

        }//end

        bestid: for(int s = 0; s < VEC; s++){
            best_idx[s] = comp_idx[s][comp_dict[s]];
        }
        
        uint32_t best_offset[VEC];
        int8_t best_length_0[VEC];
        // Find the best offset 
        find_main : for(int i = 0; i < VEC; i++) {
            int32_t offset = curr_idx[i] - best_idx[i];  
            if( best_length[i] >= 4  && offset < MAX_OFFSET_LIMIT && offset < curr_idx[i]) {
                best_offset[i] = offset;
                best_length_0[i] = best_length[i];
            }else {
                best_offset[i] = 0;
                best_length_0[i] = 0;
            }
        }   
  
        /************************************************************************/
        //          Match Search and Filter -- End
        /***********************************************************************/
        encodedV_dt tmpV;
        for (int i = 0 ; i < VEC ; i++){
             encoded_dt tmpValue;
             tmpValue.range(7,0)    = curr_window[i];
             tmpValue.range(15,8)    = best_length_0[i];
             tmpValue.range(31,16)    = best_offset[i];
             tmpV.range((i+1)*32-1,i*32) = tmpValue;
        }
        outStream << tmpV;
        
    }
    encodedV_dt tmpV;  
    leftover2:for (int i = 0; i < VEC ; i++){  
    #pragma HLS UNROLL
         encoded_dt tmpValue;
         tmpValue.range(7,0)    = curr_window[VEC + i];  
         tmpValue.range(15,8)   = 0;  
         tmpValue.range(31,16) = 0;  
         tmpV.range((i+1)*32-1,i*32) = tmpValue;  
    }
    outStream << tmpV;
    //printme("%s:Starting\n",__FUNCTION__);
}
void lz77(
                const uint512_t *in,      
                uint512_t       *out,          
                uint32_t        *encoded_size,
                long input_size                      
                )
{
    hls::stream<uint512_t>   inStream512   ;
    hls::stream<uintV_t>     inStreamV     ;
    hls::stream<encodedV_dt> encodedStream;
    hls::stream<encodedV_dt> encodedStream0;
    hls::stream<encodedV_dt> encodedStream1;
    hls::stream<encodedV_dt> encodedStream2;
    hls::stream<encodedV_dt> encodedStream3;
    hls::stream<encodedV_dt> encodedStream4;
    hls::stream<encodedV_dt> encodedStream5;
    hls::stream<encodedV_dt> encodedStream6;
    hls::stream<encodedV_dt> encodedStream7;
    hls::stream<uint16_t> encodedStreamSize0;
    hls::stream<uint16_t> encodedStreamSize1;
    hls::stream<uint16_t> encodedStreamSize2;
    hls::stream<uint16_t> encodedStreamSize3;
    hls::stream<uint16_t> encodedStreamSize4;
    hls::stream<uint16_t> encodedStreamSize5;
    hls::stream<uint16_t> encodedStreamSize6;
    hls::stream<uint16_t> encodedStreamSize7;
    hls::stream<uintOutV_t>  outCompressedStream     ;
    hls::stream<uintOutV_t>  outCompressedStream0   ;
    hls::stream<uintOutV_t>  outCompressedStream1   ;
    hls::stream<uintOutV_t>  outCompressedStream2   ;
    hls::stream<uintOutV_t>  outCompressedStream3   ;
    hls::stream<uintOutV_t>  outCompressedStream4   ;
    hls::stream<uintOutV_t>  outCompressedStream5   ;
    hls::stream<uintOutV_t>  outCompressedStream6   ;
    hls::stream<uintOutV_t>  outCompressedStream7   ;
    hls::stream<uint16_t>    outCompressedStreamSize  ;
    hls::stream<uint16_t>    outCompressedStreamSize0   ;
    hls::stream<uint16_t>    outCompressedStreamSize1   ;
    hls::stream<uint16_t>    outCompressedStreamSize2   ;
    hls::stream<uint16_t>    outCompressedStreamSize3   ;
    hls::stream<uint16_t>    outCompressedStreamSize4   ;
    hls::stream<uint16_t>    outCompressedStreamSize5   ;
    hls::stream<uint16_t>    outCompressedStreamSize6   ;
    hls::stream<uint16_t>    outCompressedStreamSize7   ;
    hls::stream<uint512_t>   outStream512    ;
    hls::stream<uint16_t>    outStream512Size;
    #pragma HLS STREAM variable=inStream512              depth=64
    #pragma HLS STREAM variable=inStreamV                depth=64
    #pragma HLS STREAM variable=encodedStream            depth=64
    #pragma HLS STREAM variable=encodedStream0           depth=256
    #pragma HLS STREAM variable=encodedStream1           depth=256
    #pragma HLS STREAM variable=encodedStream2           depth=256
    #pragma HLS STREAM variable=encodedStream3           depth=256
    #pragma HLS STREAM variable=encodedStream4           depth=256
    #pragma HLS STREAM variable=encodedStream5           depth=256
    #pragma HLS STREAM variable=encodedStream6           depth=256
    #pragma HLS STREAM variable=encodedStream7           depth=256
    #pragma HLS STREAM variable=encodedStreamSize0       depth=2
    #pragma HLS STREAM variable=encodedStreamSize1       depth=2
    #pragma HLS STREAM variable=encodedStreamSize2       depth=2
    #pragma HLS STREAM variable=encodedStreamSize3       depth=2
    #pragma HLS STREAM variable=encodedStreamSize4       depth=2
    #pragma HLS STREAM variable=encodedStreamSize5       depth=2
    #pragma HLS STREAM variable=encodedStreamSize6       depth=2
    #pragma HLS STREAM variable=encodedStreamSize7       depth=2
    #pragma HLS STREAM variable=outCompressedStream      depth=256
    #pragma HLS STREAM variable=outCompressedStreamSize  depth=64
    #pragma HLS STREAM variable=outCompressedStream0      depth=256
    #pragma HLS STREAM variable=outCompressedStream1      depth=256
    #pragma HLS STREAM variable=outCompressedStream2      depth=256
    #pragma HLS STREAM variable=outCompressedStream3      depth=256
    #pragma HLS STREAM variable=outCompressedStream4      depth=256
    #pragma HLS STREAM variable=outCompressedStream5      depth=256
    #pragma HLS STREAM variable=outCompressedStream6      depth=256
    #pragma HLS STREAM variable=outCompressedStream7      depth=256
    #pragma HLS STREAM variable=outCompressedStreamSize0  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize1  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize2  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize3  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize4  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize5  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize6  depth=2
    #pragma HLS STREAM variable=outCompressedStreamSize7  depth=2
    #pragma HLS STREAM variable=outStream512             depth=64
    #pragma HLS STREAM variable=outStream512Size         depth=2

    #pragma HLS dataflow
    gmem_read(in,inStream512,input_size);
    conv512toV(inStream512,inStreamV,input_size);
    compress(inStreamV, encodedStream,input_size);
    splitter(encodedStream,
            encodedStream0, encodedStreamSize0,
            encodedStream1, encodedStreamSize1,
            encodedStream2, encodedStreamSize2,
            encodedStream3, encodedStreamSize3,
            encodedStream4, encodedStreamSize4,
            encodedStream5, encodedStreamSize5,
            encodedStream6, encodedStreamSize6,
            encodedStream7, encodedStreamSize7,
            input_size 
            );
    bytePacking(encodedStream0,encodedStreamSize0,outCompressedStream0,outCompressedStreamSize0);
    bytePacking(encodedStream1,encodedStreamSize1,outCompressedStream1,outCompressedStreamSize1);
    bytePacking(encodedStream2,encodedStreamSize2,outCompressedStream2,outCompressedStreamSize2);
    bytePacking(encodedStream3,encodedStreamSize3,outCompressedStream3,outCompressedStreamSize3);
    bytePacking(encodedStream4,encodedStreamSize4,outCompressedStream4,outCompressedStreamSize4);
    bytePacking(encodedStream5,encodedStreamSize5,outCompressedStream5,outCompressedStreamSize5);
    bytePacking(encodedStream6,encodedStreamSize6,outCompressedStream6,outCompressedStreamSize6);
    bytePacking(encodedStream7,encodedStreamSize7,outCompressedStream7,outCompressedStreamSize7);
    merger( outCompressedStream, outCompressedStreamSize,
            outCompressedStream0, outCompressedStreamSize0,
            outCompressedStream1, outCompressedStreamSize1,
            outCompressedStream2, outCompressedStreamSize2,
            outCompressedStream3, outCompressedStreamSize3,
            outCompressedStream4, outCompressedStreamSize4,
            outCompressedStream5, outCompressedStreamSize5,
            outCompressedStream6, outCompressedStreamSize6,
            outCompressedStream7, outCompressedStreamSize7);
    convOutVto512(outCompressedStream,outCompressedStreamSize,outStream512,outStream512Size);
    gmem_write(out,outStream512,outStream512Size,encoded_size);
}

}//end of namepsace

extern "C"{
//For DDR1
#if (COMPUTE_UNIT == 1)
void gZip_cu1
#elif (COMPUTE_UNIT == 2)
void gZip_cu2
#elif (COMPUTE_UNIT == 3)
void gZip_cu3
#elif (COMPUTE_UNIT == 4)
void gZip_cu4
#endif
(
                const uint512_t *in,      
                uint512_t       *out,          
                uint32_t        *encoded_size,
                long input_size                      
                )
{
    #pragma HLS INTERFACE m_axi port=in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=out offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=encoded_size offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=in bundle=control
    #pragma HLS INTERFACE s_axilite port=out bundle=control
    #pragma HLS INTERFACE s_axilite port=encoded_size bundle=control
    #pragma HLS INTERFACE s_axilite port=input_size bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS data_pack variable=in
    #pragma HLS data_pack variable=out

#if (COMPUTE_UNIT == 1)
    cu1::lz77(in,out,encoded_size,input_size);
#elif (COMPUTE_UNIT == 2)
    cu2::lz77(in,out,encoded_size,input_size);
#elif (COMPUTE_UNIT == 3)
    cu3::lz77(in,out,encoded_size,input_size);
#elif (COMPUTE_UNIT == 4)
    cu4::lz77(in,out,encoded_size,input_size);
#endif
}
}
