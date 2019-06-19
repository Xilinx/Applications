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
#include "hls_stream.h"
#include <ap_int.h>
#include <string.h>
#include <stdint.h>

typedef ap_uint<64> compressd_dt;
typedef ap_uint<512> apuint512_t;
template <int MATCH_LEN, int MATCH_LEVEL , int LZ_DICT_SIZE, int BIT, int MIN_OFFSET, int MIN_MATCH, int LZ_MAX_OFFSET_LIMIT>
void lz_compress(
        hls::stream<ap_uint<BIT> > &inStream,          
        hls::stream<compressd_dt> &outStream,		
		apuint512_t  *dict_buff1,
		//apuint512_t  *dict_buff2,
		//apuint512_t  *dict_buff3,
		//apuint512_t  *dict_buff4,
        uint32_t input_size,
        uint32_t left_bytes,
		uint32_t last_index //,
		//hls::stream<uint16_t> &outStreamSize
        )
{
    const int DICT_ELE_WIDTH = (MATCH_LEN*BIT + 32);
    typedef ap_uint< MATCH_LEVEL * DICT_ELE_WIDTH> uintDictV_t;
    typedef ap_uint<DICT_ELE_WIDTH> uintDict_t;
	static uint8_t previous = 0;
	uint8_t lastmatchcontext = 0;
	//std::cout<<"MATCH_LEN:"<<MATCH_LEN<<" DICT_ELE_WIDTH:"<<DICT_ELE_WIDTH<<" BIT:"<<BIT<<" MATCH_LEVEL:"<<MATCH_LEVEL<<" LZ_DICT_SIZE:"<<LZ_DICT_SIZE<<"\n";
	//const int GWIDTH = 16;
	//const int GLEVEL = 4;
	//printf("last_index:%u\n",last_index);
    if(input_size == 0) return;      
    
    uint8_t present_window[MATCH_LEN];
    #pragma HLS ARRAY_PARTITION variable=present_window complete 
    for (uint8_t i = 1 ; i < MATCH_LEN; i++){
        present_window[i] = inStream.read();
    }
    lz_compress:for (uint32_t i = MATCH_LEN-1; i < input_size -left_bytes; i++)
              {
    #pragma HLS PIPELINE II=1
    #pragma HLS dependence variable=dict_buff1 inter false
	//#pragma HLS dependence variable=dict_buff2 inter false
	//#pragma HLS dependence variable=dict_buff3 inter false
	//#pragma HLS dependence variable=dict_buff4 inter false
	//#pragma HLS dependence variable=dict inter false
        uint32_t currIdx = i - MATCH_LEN +1;
		currIdx += last_index;
        //shift present window and load next value
        for(int m = 0 ; m < MATCH_LEN -1; m++){
            #pragma HLS UNROLL
            present_window[m] = present_window[m+1];
        }
        present_window[MATCH_LEN-1] = inStream.read();
		uint32_t hash = 0;
//		hash = (present_window[4] <<4) ^ (present_window[2] <<3) ^ (present_window[1] <<3) ^ (present_window[0]);
		
//		hash = ((hash<<5) - hash + present_window[1])%LZ_DICT_SIZE;
		hash = (present_window[0])%LZ_DICT_SIZE;
		hash = ((hash<<8) + hash + present_window[1])%LZ_DICT_SIZE;
		hash = ((hash<<8) + hash + present_window[2])%LZ_DICT_SIZE;
		hash = ((hash<<8) + hash + present_window[3])%LZ_DICT_SIZE;
		//uint8_t dictval = (hash>>26) & 0x03;
		//hash = hash%(1<<26);
		//uint8_t dictval = (hash>>24) & 0x01;
		//hash = hash%(1<<24);
		//printf("%d ",dictval);
/*
		uintDictV_t dictReadValue;
		if(dictval == 0)
			dictReadValue = dict_buff1[hash];
		else if(dictval == 1)
			dictReadValue = dict_buff2[hash];

		else if(dictval == 2)
			dictReadValue = dict_buff3[hash];
		else if(dictval == 3)
			dictReadValue = dict_buff4[hash];
*/
		uintDictV_t dictReadValue = dict_buff1[hash];
		uintDictV_t dictWriteValue = dictReadValue << DICT_ELE_WIDTH;
        for(int m = 0 ; m < MATCH_LEN ; m++){
            #pragma HLS UNROLL
            dictWriteValue.range((m+1)*BIT-1,m*BIT) = present_window[m];
        }
        dictWriteValue.range(DICT_ELE_WIDTH -1, MATCH_LEN*BIT) = currIdx;
        //Dictionary Update
/*
		if(dictval == 0)
			dict_buff1[hash] = dictWriteValue;
		else if(dictval == 1)
			dict_buff2[hash] = dictWriteValue;
		else if(dictval == 2)
			dict_buff3[hash] = dictWriteValue;

		else if(dictval == 3)
			dict_buff4[hash] = dictWriteValue;
*/
		dict_buff1[hash] = dictWriteValue;
		//uintDictV_t dictReadValue1 = dict_buff1[hash];	
        //Match search and Filtering
        uint8_t match_length=0;
        uint32_t match_offset=-1;

        for (int l = 0 ; l < MATCH_LEVEL ; l++){
            uint8_t len = 0;
            bool done = 0;
            uintDict_t compareWith = dictReadValue.range((l+1)*DICT_ELE_WIDTH-1, l*DICT_ELE_WIDTH);
            uint32_t compareIdx = compareWith.range(DICT_ELE_WIDTH-1, MATCH_LEN*BIT);
            for (int m = 0; m < MATCH_LEN; m++){
				//std::cout<<"present_window["<<m<<"]:"<<present_window[m]<<" compareWith:"<<compareWith.range((m+1)*BIT-1,m*BIT)<<"\n";
				if (present_window[m] == compareWith.range((m+1)*BIT-1,m*BIT) && !done){
					//printf("present_window[%d]:%x  compareWith:%x\n",m,present_window[m],(char)compareWith.range((m+1)*BIT-1,m*BIT));
                    len++;
                }else{
                    done = 1;
                }
            }
			//std::cout<<"["<<i<<"] len:"<<(int)len<<" compareIdx:"<<compareIdx<<" currIdx:"<<currIdx<<"\n";
            if ((len >= MIN_MATCH)&& (currIdx > compareIdx) && ((currIdx -compareIdx) < LZ_MAX_OFFSET_LIMIT) && ((currIdx - compareIdx - 1) >= MIN_OFFSET)){
                len = len;
            }else{
                len = 0;
            }
			if(len == MATCH_LEN)
				len--;
            if (len > match_length){
				//std::cout<<"len:"<<(int)len<<" compareIdx:"<<compareIdx<<" currIdx:"<<currIdx<<"\n";
                match_length = len;
                match_offset = currIdx -compareIdx - 1;
				lastmatchcontext = compareWith.range((len+1)*BIT-1,len*BIT);
            }
        }

		//std::cout<<"currIdx:"<<currIdx<<"\n";
        compressd_dt outValue = 0;
        outValue.range(7,0)     = present_window[0];
        outValue.range(15,8)    = match_length;
        outValue.range(47,16)   = match_offset;//(match_offset != 0) ? match_offset:-1;
		outValue.range(55,48)	= previous;
		outValue.range(63,56)	= lastmatchcontext;
        outStream << outValue;
		//outStreamSize << 6;
		previous = present_window[0];
		//printf("[%d] LIT:%x len:%u offset:%u\n",i,(int)outValue.range(7,0),(int)outValue.range(15,8),(unsigned int)outValue.range(47,16));
    }
    lz_compress_leftover:for (int m = 1 ; m < MATCH_LEN ; m++){
        #pragma HLS PIPELINE
        compressd_dt outValue = 0;
        outValue.range(7,0)     = present_window[m];
		outValue.range(47,16)   = -1;
		outValue.range(55,48)	= previous;
		outValue.range(63,56)	= lastmatchcontext;
        outStream << outValue;
		//outStreamSize << 6;
		previous = present_window[m];
		//printf("[%d] LIT:%x len:%u offset:%u\n",m,(int)outValue.range(7,0),(int)outValue.range(15,8),(unsigned int)outValue.range(47,16));
    }
    lz_left_bytes:for (int l = 0 ; l < left_bytes ; l++){
        #pragma HLS PIPELINE
        compressd_dt outValue = 0;
        outValue.range(7,0)     = inStream.read();
		outValue.range(47,16)   = -1;
		outValue.range(55,48)	= previous;
		outValue.range(63,56)	= lastmatchcontext;
        outStream << outValue;
		//outStreamSize << 6;
		previous = present_window[l];
		//printf("[%d] LIT:%x len:%u offset:%u\n",l,(int)outValue.range(7,0),(int)outValue.range(15,8),(unsigned int)outValue.range(47,16));
    }
	//outStreamSize << 0;
}

template <int MATCH_LEN, int OFFSET_WINDOW>
void lz_cr_improve_959to999(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size, uint32_t left_bytes
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
        for(uint32_t j = 0; j < c_max_match_length-1; j++)
        {   
            #pragma HLS UNROLL
            compare_window[j] = compare_window[j+1];
        }   

        compare_window[c_max_match_length-1] = inStream.read();
        uint8_t compareLen1 = compare1.range(15,8);        
        uint8_t compareLen2 = compare2.range(15,8);        
        uint32_t compareOff1 = compare1.range(47,16);
        uint32_t compareOff2 = compare2.range(47,16);
        //printf("\ncmp2:[%d]tCh:%d tLen:%d tOffset:%d P:%d LM:%d\n",i,(uint32_t)compare2.range(7,0),(uint32_t)compare2.range(15,8),(uint32_t)compare2.range(47,16),(uint32_t)compare2.range(55,48),(uint32_t)compare2.range(63,56));
        for(uint32_t j = 0; j < c_max_match_length-2; j++)
        {
            compressd_dt compare3 = compare_window[j];
            uint8_t compareLen3 = compare3.range(15,8);
            uint32_t compareOff3 = compare3.range(47,16);
            if(compareLen1 == MATCH_LEN-1 && compareLen3 == MATCH_LEN-1 && compareOff1 == compareOff3)
            {
                compare2.range(15,8) = compareLen1;
                compare2.range(47,16) = compareOff1;
                compare2.range(63,56) = compare_window[MATCH_LEN-2].range(7,0);
                //printf("-------- [%d]tCh:%d tLen:%d tOffset:%d P:%d LM:%d\n",i,(uint32_t)compare2.range(7,0),(uint32_t)compare2.range(15,8),(uint32_t)compare2.range(47,16),(uint32_t)compare2.range(55,48),(uint32_t)compare2.range(63,56));
                break;
            }
                
        }
        outStream << compare2;
        compare1 = compare2;
        compare2 = compare_window[0];
     }
     
     lz_959to999_left_over:
     for(uint32_t i = 0; i < c_max_match_length; i++)
     {
         outStream << compare_window[i];
     }     
}

template <int MATCH_LEN, int OFFSET_WINDOW>
void lz_cr_improve_965to987(
        hls::stream<compressd_dt> &inStream,
        hls::stream<compressd_dt> &outStream,
        uint32_t input_size, uint32_t left_bytes
        )
{
    const int c_max_match_length = MATCH_LEN;
    if(input_size == 0) return;
    
    compressd_dt compare = inStream.read();
    outStream << compare;

    lz_cr_improve_965to987:for (uint32_t i = 1; i < input_size; i++){
        #pragma HLS PIPELINE II=1

        compressd_dt outValue = inStream.read(); 
        uint8_t compareLen = compare.range(15,8);        
        uint8_t outValueLen = outValue.range(15,8);        
        if(compareLen-1 > 3 && compareLen-1 > outValueLen){
            outValue.range(15,8) = compareLen - 1;
            outValue.range(47,16) = compare.range(47,16);            
            outValue.range(63,56) = compare.range(63,56);            
        }
        outStream << outValue;
        compare = outValue;
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
    compressd_dt outStreamValue;
    bool matchFlag=false;
    bool outFlag = false;
	int lo = 0;
	int maxoff = 0;
   lz_booster:for (uint32_t i = 0 ; i < (input_size-left_bytes); i++){
        #pragma HLS PIPELINE II=1 
        #pragma HLS dependence variable=local_mem inter false
        compressd_dt inValue = inStream.read();
		//std::cout<<"IN	P:"<<i<<"  "<<(int)inValue.range(7,0)<<" L:"<<(int)inValue.range(15,8)<<" O:"<<(int)inValue.range(39,16)<<"\n";
        uint8_t tCh      = inValue.range(7,0);
        uint8_t tLen     = inValue.range(15,8);
        uint32_t tOffset = inValue.range(47,16);
		//std::cout<<"["<<i<<"] "<<i-tOffset;
		//if((i- tOffset) > 1048575 )
		//	std::cout<<"["<<i<<"] "<<i-tOffset<<" ****\n";
		//std::cout<<"\n";
		if(i - tOffset > maxoff) {
			maxoff = i - tOffset;
			lo = i;
		} 
        uint8_t match_ch = local_mem[match_loc%OFFSET_WINDOW];
        local_mem[i%OFFSET_WINDOW] = tCh;
        outFlag = false;
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
            if (i) outFlag = true;
            outStreamValue = outValue;
            outValue = inValue;
            if(tLen){
                matchFlag = true;
            }else{
                matchFlag =false;
            }
        }
        if(outFlag){ outStream << outStreamValue; 
			//std::cout<<"OUT	P:"<<(int)outStreamValue.range(7,0)<<" L:"<<(int)outStreamValue.range(15,8)<<" O:"<<(int)outStreamValue.range(39,16)<<"\n";
			//getchar();
		}

    }
    outStream << outValue;
	//std::cout<<"OUT	P:"<<(int)outValue.range(7,0)<<" L:"<<(int)outValue.range(15,8)<<" O:"<<(int)outValue.range(39,16)<<"\n";
	//getchar();
    lz_booster_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        outStream << inStream.read();
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
    compressd_dt compare1, compare2, compare3,compare4;

    compare1 = inStream.read();
    uint8_t compareLen1 = compare1.range(15,8);
    uint32_t compareOff1 = compare1.range(47,16);
    
    lz_booster_999to11109: for(uint32_t i = 1; i < input_size; i++){
        #pragma HLS PIPELINE II=1
        compare4 = compare2;                            
        compare2 = inStream.read();

        uint8_t compareLen2 = compare2.range(15,8);
        uint32_t compareOff2 = compare2.range(47,16);
        if((compareLen1+match_len) < MAX_MATCH_LEN && compareLen1 == (MATCH_LEN-1) && compareLen1==compareLen2 && compareOff1==compareOff2){
            local_mem[match_len] = compare2;
            match_len++;
            matchFlag = true;
        }
        else if(matchFlag){
            compare1.range(15,8)= compareLen1 + (match_len? match_len:0);
            if(match_len) compare1.range(63,56) = compare4.range(63,56);
            outStream <<  compare1;
            for(int i = 0; i < match_len; i++){
                compare3 = local_mem[i];
                compare3.range(15,8) = compare1.range(15,8) - (i + 1);
                compare3.range(63,56) = compare1.range(63,56);
                outStream << compare3;
            }
            matchFlag = false;
            match_len = 0;
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(47,16);
        }      
        else{
            outStream << compare1;
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(47,16);
        }
     }
    outStream << compare1;
}

template<int MAX_MATCH_LEN, int OFFSET_WINDOW, int MATCH_LEN>
void lz_booster_999to11(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,       
        uint32_t input_size, uint32_t left_bytes,
       	hls::stream<uint16_t>               &outStreamSize
)
{
    if(input_size == 0) return;
   	static uint8_t lastmatchcontext = 0;
    bool outFlag = false;
    uint32_t match_len =0;
    uint16_t skip_len = 0;
    bool skipdone = false;
    compressd_dt compare1, compare2, compare3,compare4;
    compare1 = inStream.read();
    uint8_t compareLen1 = compare1.range(15,8);
    uint32_t compareOff1 = compare1.range(47,16);
    
    lz_booster_999to11: for(uint32_t i = 1; i < (input_size-left_bytes); i++){
        #pragma HLS PIPELINE II=1
        compare4 = compare2;
        compare2 = inStream.read();
        //printf("[%u] tCh:%u tLen:%u tOffset:%u P:%u LM:%u\n",i,(uint32_t)compare2.range(7,0),(uint32_t)compare2.range(15,8),(uint32_t)compare2.range(47,16),(uint32_t)compare2.range(55,48),(uint32_t)compare2.range(63,56));
        uint8_t compareLen2 = compare2.range(15,8);
        uint32_t compareOff2 = compare2.range(47,16);
        
        if(skipdone){
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(47,16);
            skipdone = false;
        } else if(skip_len){
            skip_len--;
            skipdone = skip_len?false:true;
        }else if(compareLen1+match_len < MAX_MATCH_LEN && compareLen1 == MATCH_LEN-1 && compareLen1==compareLen2 && compareOff1==compareOff2){
            match_len++;
        }else{
            skip_len = compareLen1?compareLen1-2:0;
            compare1.range(15,8)= compareLen1 + (match_len? match_len:0);
            if(match_len) compare1.range(63,56)= compare4.range(63,56);
            outFlag = true;
        }

        if(outFlag){
            if(compareLen1!=0) {
                lastmatchcontext = compare1.range(63,56);
            }
            compare1.range(63,56) = lastmatchcontext;
            //printf("==================[%u] tCh:%u tLen:%u tOffset:%u P:%u LM:%u\n",i,(uint32_t)compare1.range(7,0),(uint32_t)compare1.range(15,8),(uint32_t)compare1.range(47,16),(uint32_t)compare1.range(55,48),(uint32_t)compare1.range(63,56));
            outStream << compare1;
            outStreamSize << 8;
            outFlag = false;
            match_len = 0;
            compare1 = compare2;
            compareLen1 = compare1.range(15,8);
            compareOff1 = compare1.range(47,16);
        }
    }
    if(skipdone == false) {
        if(compareLen1!=0)
            lastmatchcontext = compare1.range(63,56);
        compare1.range(63,56) = lastmatchcontext;
        outStream << compare1;                               
        outStreamSize << 8;
    }
    lz_booster_999to11_left_bytes:
    for (uint32_t i = 0 ; i < left_bytes ; i++){
        compare3 = inStream.read();
        uint16_t compareLen3 = compare3.range(15,8);
        if(compareLen3!=0)
            lastmatchcontext = compare3.range(63,56);
        compare3.range(63,56) = lastmatchcontext;

        outStream << compare3; 
        outStreamSize << 8;
    }
   outStreamSize << 0;
}    


static void lz_filter(
        hls::stream<compressd_dt> &inStream,       
        hls::stream<compressd_dt> &outStream,
		uint32_t input_size, uint32_t left_bytes,
		hls::stream<uint16_t>               &outStreamSize
)
{
    if(input_size == 0) return;
    uint32_t skip_len =0;
	static uint8_t lastmatchcontext = 0;
	int col = 0;
	int first = 0;
	ap_uint<512> local;
	uint32_t local_count =0;
	uint16_t compout = 0;
    lz_filter:for (uint32_t i = 0 ; i < input_size-left_bytes; i++){
        #pragma HLS PIPELINE II=1 
        compressd_dt inValue = inStream.read();
        uint8_t   tLen     = inValue.range(15,8);
		uint32_t tOffset = inValue.range(47,16);
		//if((i- tOffset) > 4194304 )
		//	std::cout<<"["<<i<<"] "<<i-tOffset<<" ****\n";
		//printf("[%d] LIT:%u len:%u offset:%u p:%u LM:%u\n",i,(int)inValue.range(7,0),(int)inValue.range(15,8),(unsigned int)inValue.range(47,16),(unsigned int)inValue.range(55,48),(unsigned int)inValue.range(63,56));
		//std::cout<<"IN	P:"<<i<<"  "<<(int)inValue.range(7,0)<<" L:"<<(int)inValue.range(15,8)<<" O:"<<(int)inValue.range(47,16)<<"\n";
		if (skip_len){
            skip_len--;
        }else{			
			//printf("--------------[%d] LIT:%u len:%u offset:%u p:%u lm:%u\n",i,(int)inValue.range(7,0),(int)inValue.range(15,8),(unsigned int)inValue.range(47,16),(unsigned int)inValue.range(55,48),(unsigned int)inValue.range(63,56));
            if(tLen!=0) 
				lastmatchcontext = inValue.range(63,56);            
			inValue.range(63,56) = lastmatchcontext;
			outStream << inValue;
			//printf("-------------==============[%d] LIT:%u len:%u offset:%u p:%u lm:%u\n",i,(int)inValue.range(7,0),(int)inValue.range(15,8),(unsigned int)inValue.range(47,16),(unsigned int)inValue.range(55,48),(unsigned int)inValue.range(63,56));
			outStreamSize << 8;
			compout++;
            if(tLen)skip_len = tLen-1;
        }
    }

	//printf("compout:%d\n",compout);
	//outStreamSize <<compout*6;//local_count;
	outStreamSize << 0;
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

