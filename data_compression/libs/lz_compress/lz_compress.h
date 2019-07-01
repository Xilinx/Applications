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
#include "hls_stream.h"
#include <ap_int.h>

typedef ap_uint<32> compressd_dt;

template <int MATCH_LEN, int MATCH_LEVEL , int LZ_DICT_SIZE, int BIT, int MIN_OFFSET, int MIN_MATCH, int LZ_MAX_OFFSET_LIMIT>
void lz_compress(
        hls::stream<ap_uint<BIT> > &inStream,          
        hls::stream<compressd_dt> &outStream,          
        uint32_t input_size,
        uint32_t left_bytes
        )
{
    const int DICT_ELE_WIDTH = (MATCH_LEN*BIT + 24);
    typedef ap_uint< MATCH_LEVEL * DICT_ELE_WIDTH> uintDictV_t;
    typedef ap_uint<DICT_ELE_WIDTH> uintDict_t;

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

template<int MATCH_LEN, int OFFSET_WINDOW>
void lz_cr_improve_965to987(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size , uint32_t left_bytes
        )
{
    const int c_max_match_length = MATCH_LEN;
    if(input_size == 0) return;

    compressd_dt compare;
    compare = inStream.read();
    outStream << compare;

    lz_cr_improve_965to987:for (uint32_t i = 1; i < input_size; i++){
        #pragma HLS PIPELINE II=1
        compressd_dt outValue = inStream.read();
        uint8_t compareLen = compare.range(15,8);
        uint8_t outValueLen = outValue.range(15,8);
        if(compareLen-1 > 3 && compareLen-1 > outValueLen)
        {
            outValue.range(15,8) = compareLen - 1;
            outValue.range(31,16) = compare.range(31,16);
        }
    outStream << outValue;
    compare.range(31,0) = outValue.range(31,0);
    }
 }

template<int MATCH_LEN, int OFFSET_WINDOW>
void lz_cr_improve_959to999(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size , uint32_t left_bytes
        )
{
    const int c_max_match_length = MATCH_LEN;
    if(input_size == 0) return;

    compressd_dt compare1 = inStream.read();
    outStream << compare1;

    compressd_dt compare_window[MATCH_LEN];
    #pragma HLS array_partition variable=compare_window

    for(uint32_t i = 0; i < c_max_match_length; i++)
    {
        #pragma HLS PIPELINE
        compare_window[i] = inStream.read();
    }

    lz_cr_improve_959to999:for (uint32_t i = c_max_match_length+1; i < input_size; i++){
        #pragma HLS PIPELINE II=1

        compressd_dt compare2 = compare_window[0];
        for(uint32_t j = 0; j < c_max_match_length; j++)
        {   
            #pragma HLS UNROLL
            compare_window[j] = compare_window[j+1];
        }   

        compare_window[c_max_match_length-1] = inStream.read();
        bool match = 0;
        uint8_t compareLen1 = compare1.range(15,8);        
        uint8_t compareLen2 = compare2.range(15,8);        
        uint32_t compareOff1 = compare1.range(31,16);
        uint32_t compareOff2 = compare2.range(31,16);
        for(uint32_t j = 0; j < c_max_match_length-1; j++)
        {   
            compressd_dt compare3 = compare_window[j];
            uint8_t compareLen3 = compare3.range(15,8);
            uint32_t compareOff3 = compare3.range(31,16);
            if(compareLen1 == MATCH_LEN && compareLen3 == MATCH_LEN && compareOff1 == compareOff3 && compareLen2 < MATCH_LEN)
            {
                match = 1;
                break;
            }
            
        }
        if(match)
        {    
            compare2.range(15,8) = compareLen1;
            compare2.range(31,16) = compareOff1;
        }
        outStream << compare2;
        compare1.range(31,0) = compare2.range(31,0);
        compare2.range(31,0) = compare_window[0];
     }    
    lz_cr_improve_959to999_left_over:
     for(uint32_t i = 0; i < c_max_match_length; i++)
     {
         outStream << compare_window[i];
     }     

}

template<int MATCH_LEN, int OFFSET_WINDOW>
void lz_bestMatchFilter(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size , uint32_t left_bytes
        )        
{
     
    const int c_max_match_length = MATCH_LEN; 
    if(input_size == 0) return;

    compressd_dt compare_window[MATCH_LEN];
    #pragma HLS array_partition variable=compare_window

    //Initializing shift registers
    for(uint32_t i = 0 ; i < c_max_match_length; i++){
        #pragma HLS UNROLL
        compare_window[i] = inStream.read();
    }

    lz_bestMatchFilter:for (uint32_t i = c_max_match_length; i < input_size ; i++){
        #pragma HLS PIPELINE II=1
        //shift register logic
        compressd_dt outValue = compare_window[0];
        for(uint32_t j = 0 ; j < c_max_match_length-1;j++){
            #pragma HLS UNROLL
            compare_window[j] = compare_window[j+1];
        }
        compare_window[c_max_match_length-1] = inStream.read();
        
        uint8_t match_length = outValue.range(15,8);
        bool best_match = 1;
        //Find Best match 
        for(uint32_t j = 0; j < c_max_match_length ; j++){
            compressd_dt compareValue = compare_window[j];
            uint8_t compareLen = compareValue.range(15,8);
            if ( match_length+j < compareLen){
                best_match = 0;
            }
        }
        if(best_match == 0){
            outValue.range(15,8) = 0;
            outValue.range(31,16) = 0;
        }
        outStream << outValue;
    }

    lz_bestMatchFilter_left_over:    
    for (uint32_t i = 0 ; i < c_max_match_length; i++){
        outStream << compare_window[i];
    }
}

template<int MAX_MATCH_LEN, int OFFSET_WINDOW, int MATCH_LEN>
void lz_booster_999to11109(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,       
        uint32_t input_size,
        uint32_t left_bytes
        )
{
    if(input_size == 0) return;
    compressd_dt local_mem[MAX_MATCH_LEN];
    #pragma HLS array_partition variable=local_mem

    uint32_t match_len =0;    
    bool matchFlag = false;
    compressd_dt compare1, compare2, compare3;

    compare1 = inStream.read();
    uint8_t compareLen1 = compare1.range(15,8);
    uint32_t compareOff1 = compare1.range(31,16);
    
    lz_booster_999to11109: for(uint32_t i = 1; i < input_size; i++){
        #pragma HLS PIPELINE II=1
        compare2 = inStream.read();

        uint8_t compareLen2 = compare2.range(15,8);
        uint32_t compareOff2 = compare2.range(31,16);
        if((compareLen1+match_len) < MAX_MATCH_LEN && compareLen1 == (MATCH_LEN-1) && compareLen1==compareLen2 && compareOff1==compareOff2){
            local_mem[match_len] = compare2;
            match_len++;
            matchFlag = true;
        }
        else if(matchFlag){
            compare1.range(15,8)= compareLen1 + (match_len? match_len:0);
            outStream <<  compare1;
            for(int i = 0; i < match_len; i++){
                compare3 = local_mem[i];
                compare3.range(15,8) = compare1.range(15,8) - (i + 1);
                outStream << compare3;
            }
            matchFlag = false;
            match_len = 0;
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(31,16);
        }      
        else{
            outStream << compare1;
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(31,16);
        }
     }
    outStream << compare1;
}


template<int MAX_MATCH_LEN, int BOOSTER_OFFSET_WINDOW, int MATCH_LEN>
void lz_booster_999to11(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size, uint32_t left_bytes
        )   
{
    if(input_size == 0) return;
    bool outFlag = false;
    uint32_t match_len =0;
    uint16_t skip_len = 0;
    bool skipdone = false;
    compressd_dt compare1, compare2;
    compare1 = inStream.read();
    uint8_t compareLen1 = compare1.range(15,8);
    uint8_t compareLen1Last = 0;
    uint16_t compareOff1 = compare1.range(31,16);

    lz_booster_999to11: for(uint32_t i = 1; i < (input_size-left_bytes); i++){
        #pragma HLS PIPELINE II=1
        compare2 = inStream.read();
        uint8_t compareLen2 = compare2.range(15,8);
        uint16_t compareOff2 = compare2.range(31,16);
    
    if(skipdone){
        compare1.range(31,0) = compare2.range(31,0);
        compareLen1 = compare1.range(15,8);
        compareOff1 = compare1.range(31,16);
        skipdone = false;
    } else if(skip_len){
        skip_len--;
        skipdone = skip_len?false:true;
    }else if(compareLen1+match_len < MAX_MATCH_LEN && compareLen1 == MATCH_LEN && compareLen1==compareLen2 && compareOff1==compareOff2){
        match_len++;
    }else{
        skip_len = compareLen1?compareLen1-2:0;
        compare1.range(15,8)= compareLen1 + match_len;
        outFlag = true;
    }

    if(outFlag){
        outStream << compare1;
        outFlag = false;
        compareLen1Last = compare1.range(15,8);
        match_len = 0;
        compare1.range(31,0) = compare2.range(31,0);
        compareLen1 = compare1.range(15,8);
        compareOff1 = compare1.range(31,16);
     }
   }
    if(skipdone == false) {
        outStream << compare1;                               
    }
    lz_booster_999to11_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        compressd_dt compare3;
        compare3 = inStream.read();
        outStream << compare3;
   }
}

template<int MAX_MATCH_LEN, int BOOSTER_OFFSET_WINDOW>
void lz_booster(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,       
        uint32_t input_size, uint32_t left_bytes
)
{

    if(input_size == 0) return;
    uint8_t local_mem[BOOSTER_OFFSET_WINDOW];
    uint32_t match_loc = 0;
    uint32_t match_len =0;
    compressd_dt outValue;
    compressd_dt outStreamValue;
    bool matchFlag=false;
    bool outFlag = false;
    bool boostFlag = false;
    uint16_t skip_len = 0;
    lz_booster:for (uint32_t i = 0 ; i < (input_size-left_bytes); i++){
        #pragma HLS PIPELINE II=1 
        #pragma HLS dependence variable=local_mem inter false
        compressd_dt inValue = inStream.read();
        uint8_t tCh      = inValue.range(7,0);
        uint8_t tLen     = inValue.range(15,8);
        uint16_t tOffset = inValue.range(31,16);
        if (tOffset < BOOSTER_OFFSET_WINDOW) {
            boostFlag = true;
        }else{
            boostFlag = false;
        }
        uint8_t match_ch = local_mem[match_loc%BOOSTER_OFFSET_WINDOW];
        local_mem[i%BOOSTER_OFFSET_WINDOW] = tCh;
        outFlag = false;

        if (skip_len){
            skip_len--;
        }else if (matchFlag
                && (match_len< MAX_MATCH_LEN) 
                && (tCh == match_ch) 
           ){
                match_len++;
                match_loc++;
                outValue.range(15,8) = match_len;
        }else{
            match_len = 1;
            match_loc = i - tOffset;
            if (i) outFlag = true;
            outStreamValue = outValue;
            outValue = inValue;
            if(tLen){
                if (boostFlag){
                    matchFlag = true;
                    skip_len = 0;               
                }else{
                    matchFlag = false;
                    skip_len = tLen-1;               
                }
            }else{
                matchFlag =false;
            }
        }
        if(outFlag) outStream << outStreamValue;

    }
    outStream << outValue;
    lz_booster_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        outStream << inStream.read();
    }
}
static void lz_filter(
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

template<int HISTORY_SIZE, int READ_STATE, int MATCH_STATE, int LOW_OFFSET_STATE, int LOW_OFFSET>
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
                //printf("HISTORY=%x\n",(uint8_t)outValue);
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
                //printf("LITERAL=%x\n",(uint8_t)outValue);
            }
        }else if (next_states == LOW_OFFSET_STATE){
            outValue = prevValue[offset];
            match_loc++;
            out_len++;
            if (out_len == match_len) next_states = READ_STATE;
        }else{
            outValue = local_buf[match_loc % HISTORY_SIZE];
            //printf("HISTORY=%x\n",(uint8_t)outValue);
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

