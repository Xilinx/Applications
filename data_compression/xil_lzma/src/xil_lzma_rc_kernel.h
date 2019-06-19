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
#include <stdint.h>
#include <ap_int.h>
#include "hls_stream.h"
#include <iostream>
#include <bits/stdc++.h>
#include <stdint.h>
#include "fastpos.h"
#include "common.h"
typedef ap_uint<64> compressd_dt;
typedef ap_uint<11> probability;

#define	rc_literal_OFFSET		0
#define	rc_is_match_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)
#define	rc_is_rep0_long_OFFSET 	(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)
#define	rc_is_rep_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)
#define	rc_is_rep0_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)
#define	rc_is_rep1_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)
#define	rc_is_rep2_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)
#define	rc_pos_slot_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)
#define	rc_pos_special_OFFSET	(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)
#define	rc_pos_align_OFFSET		(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)

#define	match_len_encoder_choice ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX) +(ALIGN_TABLE_SIZE))
#define match_len_encoder_choice2	((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX) +(ALIGN_TABLE_SIZE)+1)
#define match_len_encoder_low  ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX) +(ALIGN_TABLE_SIZE)+1+1)
#define match_len_encoder_mid	((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX) +(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS))
#define match_len_encoder_high	((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS))
 
#define rep_len_encoder_choice	((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+(LEN_HIGH_SYMBOLS))
#define rep_len_encoder_choice2  ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+(LEN_HIGH_SYMBOLS)+1)
#define rep_len_encoder_low  ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+(LEN_HIGH_SYMBOLS)+1+1)
#define rep_len_encoder_mid  ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+(LEN_HIGH_SYMBOLS)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS))
#define rep_len_encoder_high  ((LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)+(STATES*POS_STATES_MAX)+(STATES*POS_STATES_MAX)+(STATES)+(STATES)+(STATES)+(STATES)+(LEN_TO_POS_STATES*POS_SLOTS)+(FULL_DISTANCES - END_POS_MODEL_INDEX)+(ALIGN_TABLE_SIZE)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+(LEN_HIGH_SYMBOLS)+1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS))

#define END_ALLPROBS (rep_len_encoder_high+LEN_HIGH_SYMBOLS)

#define RC_LITERAL(x,y)			(rc_literal_OFFSET + (x*LITERAL_CODER_SIZE) + y)
#define RC_LITERAL_x(x)			(rc_literal_OFFSET + (x*LITERAL_CODER_SIZE))
#define RC_IS_MATCH(x,y)		(rc_is_match_OFFSET + (x*STATES) + y)
#define RC_IS_REP0_LONG(x,y)		(rc_is_rep0_long_OFFSET + (x*STATES) + y)
#define RC_IS_REP(x)			(rc_is_rep_OFFSET + x)
#define RC_IS_REP0(x)			(rc_is_rep0_OFFSET + x)
#define RC_IS_REP1(x)			(rc_is_rep1_OFFSET + x)
#define RC_IS_REP2(x)			(rc_is_rep2_OFFSET + x)
//#define RC_POS_SLOT(x,y)		(rc_pos_slot_OFFSET + (x*POS_SLOTS) + y)
#define RC_POS_SLOT(x)			(rc_pos_slot_OFFSET + (x*POS_SLOTS))
#define RC_POS_SPECIAL(x)		(rc_pos_special_OFFSET + x)
#define RC_POS_ALIGN(x)			(rc_pos_align_OFFSET + x)

#define RC_MATCH_LEN_ENCODE_CHOICE	(match_len_encoder_choice)
#define RC_MATCH_LEN_ENCODE_CHOICE2	(match_len_encoder_choice2)
#define RC_MATCH_LEN_ENCODE_LOW(x,y)	(match_len_encoder_low + (x*LEN_LOW_SYMBOLS) + y)
#define RC_MATCH_LEN_ENCODE_MID(x,y)	(match_len_encoder_mid + (x*LEN_MID_SYMBOLS) + y)
#define RC_MATCH_LEN_ENCODE_HIGH(x)	(match_len_encoder_high + x)

#define RC_REP_LEN_ENCODE_CHOICE	(rep_len_encoder_choice)
#define RC_REP_LEN_ENCODE_CHOICE2	(rep_len_encoder_choice2)
#define RC_REP_LEN_ENCODE_LOW(x,y)	(rep_len_encoder_low + (x*LEN_LOW_SYMBOLS) + y)
#define RC_REP_LEN_ENCODE_MID(x,y)	(rep_len_encoder_mid + (x*LEN_MID_SYMBOLS) + y)
#define RC_REP_LEN_ENCODE_HIGH(x)	(rep_len_encoder_high + x)

#define literal_subcoder1(lc, lp_mask, pos, prev_byte) \
	(RC_LITERAL_x((((pos) & lp_mask) << lc) + ((prev_byte) >> (8 - lc))))

#if (C_COMPUTE_UNIT == 1)
namespace cu1
#elif (C_COMPUTE_UNIT == 2)
namespace cu2
#elif (C_COMPUTE_UNIT == 3)
namespace cu3
#elif (C_COMPUTE_UNIT == 4)
namespace cu4
#endif
{

typedef struct {
        uint32_t pos_mask;
        uint32_t literal_context_bits;
        uint32_t literal_pos_mask;
        uint32_t reps[REP_DISTANCES];
        lzma_lzma_state state;
        bool is_flushed;
} Range_Coder;



void initrc(Range_Coder *rc,int lc,int pb,int lp) {
    rc->is_flushed = false;
    rc->state = STATE_LIT_LIT;
    rc->pos_mask = (1U << pb) - 1;
    rc->literal_context_bits = lc;
    rc->literal_pos_mask = (1U << lp) - 1;
    for (size_t i = 0; i < REP_DISTANCES; ++i)
 		rc->reps[i] = 0;
}
/*
unsigned int find_pos_slot(unsigned int pos) {
	unsigned int val = 0;
	
	if(pos < 4)
		val = pos;
	else if(pos < 6)
		val = 4;
	else if(pos < 8)
		val = 5;
	else if(pos < 12)
		val = 6;
	else if(pos < 16)
		val = 7;
	else if(pos < 24)
		val = 8;
	else if(pos < 32)
		val = 9;
	else if(pos < 48)
		val = 10;
	else if(pos < 64)
		val = 11;
	else if(pos < 96)
		val = 12;
	else if(pos < 128)
		val = 13;
	else if(pos < 192)
		val = 14;
	else if(pos < 256)
		val = 15;
	else if(pos < 384)
		val = 16;
	else if(pos < 512)
		val = 17;
	else if(pos < 768)
		val = 18;
	else if(pos < 1024)
		val = 19;
	else if(pos < 1536)
		val = 20;
	else if(pos < 2048)
		val = 21;
	else if(pos < 3072)
		val = 22;
	else if(pos < 4096)
		val = 23;
	else if(pos < 6144)
		val = 24;
	else 
		val = 25;
	 
	return val;
}

uint32_t find_pos_slot1(uint32_t pos) {
	uint32_t val = 0;
	if(pos < 4)
		return pos;
	for(int i=2;i<=12;i++) {
		uint32_t x = 1<<i;
		if(pos<x) {
			val = i+2;
			break;
		}else {
			val = i+3;
			break;
		}
	}
	return val;
}

uint32_t get_pos_slot3(uint32_t pos)
{
	uint32_t val = 0;
	if (pos < 8192)
		val = 0;
	else if(pos < 33554432) {
		val = 24;
		pos = pos >> 12;
	}else {
		val = 48;
		pos = pos >> 24;
	}
	return find_pos_slot(pos) + val;
}
*/
void lzma_rc_1 (
        hls::stream<compressd_dt> &inStream,		
        hls::stream<uint16_t>  &inStreamSize,
        hls::stream<ap_uint<512> > &symStream,
        hls::stream<ap_uint<1024> > &probsStream,
        hls::stream<uint16_t> &outStreamSize,
        uint32_t input_size,
        uint32_t last_index
		) 
{
    Range_Coder rc;
    uint32_t lc = 3;
    uint32_t pb = 2;
    uint32_t lp = 0;
    uint32_t rcposition = last_index;
    initrc(&rc,lc,pb,lp);
    uint32_t len = 1;
    #pragma HLS ARRAY_PARTITION variable=rc.reps complete

    uint8_t sdata[58];
    uint16_t pdata[58];
    uint16_t cadta = 0;
    #pragma HLS ARRAY_PARTITION variable=sdata complete
    #pragma HLS ARRAY_PARTITION variable=pdata complete

    if(last_index == 0) {
        uint16_t size = inStreamSize.read();
        compressd_dt inValue = inStream.read();
        uint8_t   tstr	   = inValue.range(7,0);
        sdata[cadta] = 0;
        pdata[cadta++] = RC_IS_MATCH(0,0);

        uint32_t model_index = 1;
        rc_bittree_in11:for(int i=7; i>= 0;i--){
            #pragma HLS unroll
            const uint32_t bit = (tstr >> i) & 1;
            uint32_t index = model_index;
            model_index = (index << 1) + bit;
            sdata[cadta] = bit;
            pdata[cadta++] = RC_LITERAL(0,index);
        }
        ap_uint<512> sym64;
        ap_uint<1024> probs64;
        lzma_rc_1_1_11:for(int i=0;i<9;i++) {
            #pragma HLS unroll
            sym64.range(((i+1)*8)-1,i*8) = sdata[i];
            probs64.range(((i+1)*16)-1,i*16) = pdata[i];
        }
        symStream << sym64;
        probsStream << probs64;
        outStreamSize << cadta;
        rcposition = 1;
    }

    lzma_rc_1:for (uint16_t size = inStreamSize.read() ; size != 0 ; size = inStreamSize.read()) {
        #pragma HLS PIPELINE II=1
        compressd_dt inValue	= inStream.read();

        uint8_t   tstr	   		= inValue.range(7,0);
        uint8_t   tLen     		= inValue.range(15,8);
        uint32_t tOffset 		= inValue.range(47,16);
        uint8_t tstr_prev 		= inValue.range(55,48);
        uint8_t match_byte 		= inValue.range(63,56);

        uint32_t pos_state = rcposition & rc.pos_mask;
        cadta = 0;
        //LITERAL ENCODE	
        len = (tLen == 0)? 1:tLen;
        if(tOffset == UINT32_MAX) { //4294967295

            uint32_t subcoder = literal_subcoder1(rc.literal_context_bits, rc.literal_pos_mask,rcposition, tstr_prev);
            sdata[cadta] = 0;
            pdata[cadta++] = RC_IS_MATCH(rc.state,pos_state);

            if (is_literal_state(rc.state)) {
                uint32_t model_index = 1;
                rc_bittree_in1:for(int i=7; i>= 0;i--){
                    #pragma HLS unroll
                    const uint32_t bit = (tstr >> i) & 1;
                    uint32_t index = model_index;
                    model_index = (index << 1) + bit;
                    sdata[cadta] = bit;
                    pdata[cadta++] = subcoder+index;
                }
            } else {
                uint32_t offset = 0x100;
                uint32_t symbol = (1U << 8) + tstr;
                uint32_t l_match_byte = match_byte;

                literal_matched:for(int i=0;i<8;i++) {
                    #pragma HLS unroll
                    l_match_byte <<= 1;
                    const uint32_t match_bit = l_match_byte & offset;
                    const uint32_t subcoder_index = offset + match_bit + (symbol >> 8);					
                    const uint32_t bit = (symbol >> 7) & 1;
                    sdata[cadta] = bit;
                    pdata[cadta++] = subcoder+subcoder_index;
                    symbol <<= 1;
                    offset &= ~(l_match_byte ^ symbol);
                }
            }

            if(rc.state <= STATE_SHORTREP_LIT_LIT)
                rc.state = STATE_LIT_LIT;
            else if(rc.state <= STATE_LIT_SHORTREP)
                rc.state = (lzma_lzma_state)(rc.state -3);
            else rc.state = (lzma_lzma_state)(rc.state -6);
        } else {

            if(tOffset == rc.reps[0])
                tOffset = 0;//rc->reps[0];
            else if(tOffset == rc.reps[1])
                tOffset = 1;//rc->reps[1];
            else if(tOffset == rc.reps[2])
                tOffset = 2;//rc->reps[2];
            else if(tOffset == rc.reps[3])
                tOffset = 3;//rc->reps[3];
            else
                tOffset = tOffset + 4;
            sdata[cadta] = 1;
            pdata[cadta++] = RC_IS_MATCH(rc.state,pos_state);

            if (tOffset < 4) {
                sdata[cadta] = 1;
                pdata[cadta++] = RC_IS_REP(rc.state);
                uint32_t rep = tOffset;
                if (rep == 0) {
                    sdata[cadta] = 0;
                    pdata[cadta++] = RC_IS_REP0(rc.state);
                    sdata[cadta] = (len!=1);
                    pdata[cadta++] = RC_IS_REP0_LONG(rc.state,pos_state);
                } else {
                    const uint32_t distance = rc.reps[rep];
                    sdata[cadta] = 1;
                    pdata[cadta++] = RC_IS_REP0(rc.state);

                    if (rep == 1) {
                        sdata[cadta] = 0;
                        pdata[cadta++] = RC_IS_REP1(rc.state);
                    } else {
                        sdata[cadta] = 1;
                        pdata[cadta++] = RC_IS_REP1(rc.state);
                        sdata[cadta] = (rep-2);
                        pdata[cadta++] = RC_IS_REP2(rc.state);

                        if (rep == 3)
                            rc.reps[3] = rc.reps[2];
                        rc.reps[2] = rc.reps[1];
                    }
                    rc.reps[1] = rc.reps[0];
                    rc.reps[0] = distance;
                }

                if (len == 1) {
                    update_short_rep(rc.state);
                } else {
                    uint8_t mlen = len;
                    mlen -= MATCH_LEN_MIN;
                    if (mlen < LEN_LOW_SYMBOLS) {
                        sdata[cadta] = 0;
                        pdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE;
                        uint32_t model_index = 1;
                        rc_bittree_in_replen_1:for(int i=LEN_LOW_BITS-1; i>= 0;i--){
                            #pragma HLS unroll
                            const uint32_t bit = (mlen >> i) & 1;
                            uint32_t index = model_index;
                            model_index = (index << 1) + bit;
                            sdata[cadta] = bit;
                            pdata[cadta++] = RC_REP_LEN_ENCODE_LOW(pos_state,index);
                        }
                    } else {
                        sdata[cadta] = 1;
                        pdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE;
                        mlen -= LEN_LOW_SYMBOLS;

                        if (mlen < LEN_MID_SYMBOLS) {
                            sdata[cadta] = 0;
                            pdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2;
                            uint32_t model_index = 1;
                            rc_bittree_in_replen_2:for(int i=LEN_MID_BITS-1; i>= 0;i--){
                                #pragma HLS unroll
                                const uint32_t bit = (mlen >> i) & 1;
                                uint32_t index = model_index;
                                model_index = (index << 1) + bit;
                                sdata[cadta] = bit;
                                pdata[cadta++] = RC_REP_LEN_ENCODE_MID(pos_state,index);
                            }
                        } else {
                            sdata[cadta] = 1;
                            pdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2;
                            mlen -= LEN_MID_SYMBOLS;
                            uint32_t model_index = 1;
                            rc_bittree_in_replen_3:for(int i=LEN_HIGH_BITS-1; i>= 0;i--){
                                #pragma HLS unroll
                                const uint32_t bit = (mlen >> i) & 1;
                                uint32_t index = model_index;
                                model_index = (index << 1) + bit;
                                sdata[cadta] = bit;
                                pdata[cadta++] = RC_REP_LEN_ENCODE_HIGH(index);
                            }
                        }
                    }

                    update_long_rep(rc.state);
                }
            } else {
                sdata[cadta] = 0;
                pdata[cadta++] = RC_IS_REP(rc.state);
                update_match(rc.state);
                uint8_t mlen = len;
                mlen -= MATCH_LEN_MIN;
                if (mlen < LEN_LOW_SYMBOLS) {
                    sdata[cadta] = 0;
                    pdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE;
                    uint32_t model_index = 1;
                    rc_bittree_in_matchlen_1:for(int i=LEN_LOW_BITS-1; i>= 0;i--){
                        #pragma HLS unroll
                        const uint32_t bit = (mlen >> i) & 1;
                        uint32_t index = model_index;
                        model_index = (index << 1) + bit;
                        sdata[cadta] = bit;
                        pdata[cadta++] = RC_MATCH_LEN_ENCODE_LOW(pos_state,index);
                    }
                } else {
                    sdata[cadta] = 1;
                    pdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE;
                    mlen -= LEN_LOW_SYMBOLS;

                    if (mlen < LEN_MID_SYMBOLS) {
                        sdata[cadta] = 0;
                        pdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2;
                        uint32_t model_index = 1;
                        rc_bittree_in_matchlen_2:for(int i=LEN_MID_BITS-1; i>= 0;i--){
                            #pragma HLS unroll
                            const uint32_t bit = (mlen >> i) & 1;
                            uint32_t index = model_index;
                            model_index = (index << 1) + bit;
                            sdata[cadta] = bit;
                            pdata[cadta++] = RC_MATCH_LEN_ENCODE_MID(pos_state,index);
                        }
                    } else {
                        sdata[cadta] = 1;
                        pdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2;
                        mlen -= LEN_MID_SYMBOLS;
                        uint32_t model_index = 1;
                        rc_bittree_in_matchlen_3:for(int i=LEN_HIGH_BITS-1; i>= 0;i--){
                            #pragma HLS unroll
                            const uint32_t bit = (mlen >> i) & 1;
                            uint32_t index = model_index;
                            model_index = (index << 1) + bit;
                            sdata[cadta] = bit;
                            pdata[cadta++] = RC_MATCH_LEN_ENCODE_HIGH(index);
                        }
                    }
                }

                uint32_t distance = tOffset-4;
                const uint32_t pos_slot = get_pos_slot(distance);//get_pos_slot3(distance);
                const uint32_t len_to_pos_state = get_len_to_pos_state(len);
                uint32_t model_index = 1;
                rc_bittree_in2:for(int i=POS_SLOT_BITS-1; i>= 0;i--){
                    #pragma HLS unroll
                    const uint32_t bit = (pos_slot >> i) & 1;
                    uint32_t index = model_index;
                    model_index = (index << 1) + bit;
                    sdata[cadta] = bit;
                    pdata[cadta++] = (RC_POS_SLOT(len_to_pos_state)+index);
                }
                if (pos_slot >= START_POS_MODEL_INDEX && pos_slot < END_POS_MODEL_INDEX) {
                    const uint32_t footer_bits = (pos_slot >> 1) - 1;	// ******[0 to 6]
                    const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
                    const uint32_t pos_reduced = distance - base;
                    uint32_t model_index = 1;
                    rc_bittree_reverse_in1:for(int i=0;i<6;i++){ //footer_bits 
                        #pragma HLS unroll
                        if(i<footer_bits) {
                            const uint32_t bit = (pos_reduced >> i) & 1;
                            uint32_t index = model_index;
                            model_index = (index << 1) + bit;
                            sdata[cadta] = bit;
						    pdata[cadta++] = (RC_POS_SPECIAL(base - pos_slot - 1)+index);
                        }
                    }
                }
                if (pos_slot >= START_POS_MODEL_INDEX && pos_slot >= END_POS_MODEL_INDEX) {
                    const uint32_t footer_bits = (pos_slot >> 1) - 1;  // ***[6 to 30]
                    const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
                    const uint32_t pos_reduced = distance - base;
                    const uint32_t f_A = footer_bits - ALIGN_BITS;
                    rc_direct:for(int i=1; i<=26;i++){
                        #pragma HLS unroll
                        if(i<=f_A) {
                            sdata[cadta++] = (RC_DIRECT_0 + (((pos_reduced >> ALIGN_BITS) >> (footer_bits - ALIGN_BITS - i)) & 1));
                        }
                    }
                    uint32_t model_index = 1;
                    rc_bittree_reverse_in2:for(int i=0;i<ALIGN_BITS;i++){
                        #pragma HLS unroll
                        const uint32_t bit = ((pos_reduced & ALIGN_MASK) >> i) & 1;
                        uint32_t index = model_index;
                        model_index = (index << 1) + bit;
                        sdata[cadta] = bit;
                        pdata[cadta++] = (rc_pos_align_OFFSET+index);
                    }
                }

                rc.reps[3] = rc.reps[2];
                rc.reps[2] = rc.reps[1];
                rc.reps[1] = rc.reps[0];
                rc.reps[0] = tOffset-4;
            }
			
        }

        ap_uint<512> sym64;
        ap_uint<1024> probs64;
        lzma_rc_1_1_1:for(int i=0;i<58;i++) {
            #pragma HLS unroll
            if(i<cadta) {
                sym64.range(((i+1)*8)-1,i*8) = sdata[i];
                probs64.range(((i+1)*16)-1,i*16) = pdata[i];
            }
        }
        symStream << sym64;
        probsStream << probs64;
        outStreamSize << cadta;

        rcposition += len;
    }

    if (!rc.is_flushed) {
        rc.is_flushed = true;
        ap_uint<512> sym64;
        ap_uint<1024> probs64;
        rc_flush:for (size_t i = 0; i < 5; ++i) {
            #pragma HLS unroll
            sym64.range(((i+1)*8)-1,i*8) = RC_FLUSH;
            probs64.range(((i+1)*16)-1,i*16) = 0;
        }
        symStream << sym64;
        probsStream << probs64;
        outStreamSize << 5;
    }

    rc.is_flushed = false;
    outStreamSize << 0;
}

enum PACKAGE {
    RC_PACKAGE_BIT = 0,
    RC_PACKAGE_BITTREE,
    RC_PACKAGE_BITTREE_REVERSE,
    RC_PACKAGE_DIRECT,
    RC_PACKAGE_LITERAL_MATCH,
    RC_PACKAGE_FLUSH
};


void lzma_rc_1_1 (
		hls::stream<compressd_dt> &inStream,		
		hls::stream<uint16_t>  &inStreamSize,
        hls::stream<ap_uint<512> > &outStream,
        hls::stream<uint8_t> &outStreamSize,
		uint32_t input_size,
		uint32_t last_index
		) 
{
	Range_Coder rc;
    uint32_t lc = 3;
    uint32_t pb = 2;
    uint32_t lp = 0;
	//static uint32_t lzblock = 0;
	static uint32_t rcposition = 0;
	initrc(&rc,lc,pb,lp);
    uint32_t len = 1;
	#pragma HLS ARRAY_PARTITION variable=rc.reps complete

	//rc_encode(&rc,outStream, outStreamSize);
	uint8_t sdata[64];
	uint16_t cadta = 0;
	uint8_t ssize = 0; 
	#pragma HLS ARRAY_PARTITION variable=sdata complete
	if(last_index == 0) {
		uint16_t size = inStreamSize.read();
		compressd_dt inValue = inStream.read();
		uint8_t   tstr	   = inValue.range(7,0);
		//rc_bit(symStream,probsStream,outStreamSize,RC_IS_MATCH(0,0),0);
		sdata[cadta++] = RC_PACKAGE_BIT;
		//printf("[%s:%d] IN:%u \n",__func__,__LINE__,RC_IS_MATCH(0,0));
		sdata[cadta++] = 0;
		sdata[cadta++] = RC_IS_MATCH(0,0) >> 8;
		sdata[cadta++] = RC_IS_MATCH(0,0) & 0xFF;
		ssize++;
		
		//rc_bittree(symStream,probsStream,outStreamSize,RC_LITERAL(0),8, tstr);
		sdata[cadta++] = RC_PACKAGE_BITTREE;
		//printf("[%s:%d] IN:%d [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
		sdata[cadta++] = 7;
		sdata[cadta++] = tstr;
		sdata[cadta++] = RC_LITERAL(0,0) >> 8;
		sdata[cadta++] = RC_LITERAL(0,0) & 0xFF;
		ssize++;
		//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
		ap_uint<512> sym64;
		lzma_rc_1_1_11:for(int i=0;i<9;i++) {
			#pragma HLS unroll
			//printf("...[%u]--> [%u,%u]\n",i,sdata[i],pdata[i]);
			sym64.range(((i+1)*8)-1,i*8) = sdata[i];
			//printf("-----[%u]--->[%u]\n",i,(uint32_t)sym64.range(((i+1)*8)-1,i*8));
		}
		outStream << sym64;
		outStreamSize << ssize;
		//rcposition += 1;
		rcposition = 1;
	}
	
	//lzma_rc_1:for(unsigned int f=(lzblock==0)?1:0 ; f<input_size ; f+=len) {
	lzma_rc_1:for (uint16_t size = inStreamSize.read() ; size != 0 ; size = inStreamSize.read()) {
		#pragma HLS PIPELINE II=1
		//lzblock = 1;
		compressd_dt inValue	= inStream.read();

		uint8_t   tstr	   		= inValue.range(7,0);
		uint8_t   tLen     		= inValue.range(15,8);
		uint32_t tOffset 		= inValue.range(47,16);
		uint8_t tstr_prev 		= inValue.range(55,48);
		uint8_t match_byte 		= inValue.range(63,56);

		//printf("[%.2x, %u, %u, %.2x, %.2x]\n", tstr,tLen,tOffset,tstr_prev,match_byte);

        uint32_t pos_state = rcposition & rc.pos_mask;
		cadta = 0;
		ssize = 0;
		//LITERAL ENCODE	
        len = (tLen == 0)? 1:tLen;
        if(tOffset == UINT32_MAX) {

			//rc_bit(symStream,probsStream,outStreamSize,RC_IS_MATCH(rc.state,pos_state), 0);	
			sdata[cadta++] = RC_PACKAGE_BIT;
			//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
			sdata[cadta++] = 0;
			sdata[cadta++] = RC_IS_MATCH(rc.state,pos_state) >> 8;
			sdata[cadta++] = RC_IS_MATCH(rc.state,pos_state) & 0xFF;
			ssize++;
			uint32_t subcoder = literal_subcoder1(rc.literal_context_bits, rc.literal_pos_mask,rcposition, tstr_prev);
			
			if (is_literal_state(rc.state)) {
				//printf("[%s:%u] [%u] ,[%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
				//rc_bittree(symStream,probsStream,outStreamSize,subcoder, 8, tstr);
				sdata[cadta++] = RC_PACKAGE_BITTREE;
				//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
				sdata[cadta++] = 7;
				sdata[cadta++] = tstr;
				sdata[cadta++] = subcoder >> 8;
				sdata[cadta++] = subcoder & 0xFF;
				ssize++;
				//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
			} else {
				//literal_matched(&rc, subcoder, match_byte, tstr);
				sdata[cadta++] = RC_PACKAGE_LITERAL_MATCH;
				//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
				sdata[cadta++] = match_byte;
				sdata[cadta++] = tstr;
				sdata[cadta++] = subcoder >> 8;
				sdata[cadta++] = subcoder & 0xFF;
				ssize++;
			}

			//update_literal(rc.state);
			if(rc.state <= STATE_SHORTREP_LIT_LIT)
				rc.state = STATE_LIT_LIT;
			else if(rc.state <= STATE_LIT_SHORTREP)
				rc.state = (lzma_lzma_state)(rc.state -3);
			else rc.state = (lzma_lzma_state)(rc.state -6);
		} else {

			if(tOffset == rc.reps[0])
				tOffset = 0;//rc->reps[0];
			else if(tOffset == rc.reps[1])
				tOffset = 1;//rc->reps[1];
			else if(tOffset == rc.reps[2])
				tOffset = 2;//rc->reps[2];
			else if(tOffset == rc.reps[3])
				tOffset = 3;//rc->reps[3];
			else
				tOffset = tOffset + 4;
			//tOffset += 4;

			//rc_bit(symStream,probsStream,outStreamSize,RC_IS_MATCH(rc.state,pos_state), 1);
			sdata[cadta++] = RC_PACKAGE_BIT;
			//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
			sdata[cadta++] = 1;
			sdata[cadta++] = RC_IS_MATCH(rc.state,pos_state) >> 8;
			sdata[cadta++] = RC_IS_MATCH(rc.state,pos_state) & 0xFF;
			ssize++;
			if (tOffset < 4) {
				//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP(rc.state), 1);
				sdata[cadta++] = RC_PACKAGE_BIT;
				//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
				sdata[cadta++] = 1;
				sdata[cadta++] = RC_IS_REP(rc.state) >> 8;
				sdata[cadta++] = RC_IS_REP(rc.state) & 0xFF;
				ssize++;
				//rep_match(&rc, pos_state, tOffset, len);
				//------------START----rep_match--------------------------------------------
				uint32_t rep = tOffset;
				if (rep == 0) {
					//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP0(rc.state), 0);
					sdata[cadta++] = RC_PACKAGE_BIT;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = 0;
					sdata[cadta++] = RC_IS_REP0(rc.state) >> 8;
					sdata[cadta++] = RC_IS_REP0(rc.state) & 0xFF;
					ssize++;
					//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP0_LONG(rc.state,pos_state),len != 1);
					sdata[cadta++] = RC_PACKAGE_BIT;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = (len!=1);
					sdata[cadta++] = RC_IS_REP0_LONG(rc.state,pos_state) >> 8;
					sdata[cadta++] = RC_IS_REP0_LONG(rc.state,pos_state) & 0xFF;
					ssize++;
				} else {
					const uint32_t distance = rc.reps[rep];
					//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP0(rc.state), 1);
					sdata[cadta++] = RC_PACKAGE_BIT;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = 1;
					sdata[cadta++] = RC_IS_REP0(rc.state) >> 8;
					sdata[cadta++] = RC_IS_REP0(rc.state) & 0xFF;
					ssize++;

					if (rep == 1) {
						//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP1(rc.state), 0);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 0;
						sdata[cadta++] = RC_IS_REP1(rc.state) >> 8;
						sdata[cadta++] = RC_IS_REP1(rc.state) & 0xFF;
						ssize++;
					} else {
						//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP1(rc.state), 1);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 1;
						sdata[cadta++] = RC_IS_REP1(rc.state) >> 8;
						sdata[cadta++] = RC_IS_REP1(rc.state) & 0xFF;
						ssize++;
						//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP2(rc.state),rep - 2);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = (rep-2);
						sdata[cadta++] = RC_IS_REP2(rc.state) >> 8;
						sdata[cadta++] = RC_IS_REP2(rc.state) & 0xFF;
						ssize++;
						
						if (rep == 3)
						    rc.reps[3] = rc.reps[2];
						rc.reps[2] = rc.reps[1];
					}
					rc.reps[1] = rc.reps[0];
					rc.reps[0] = distance;
				}

				if (len == 1) {
					update_short_rep(rc.state);
				} else {
					//length(rc, &rc_rep_len_encoder, pos_state, len, false);
					//-----START---length----------------------------------------------
					uint8_t mlen = len;
					mlen -= MATCH_LEN_MIN;
					if (mlen < LEN_LOW_SYMBOLS) {
						//rc_bit(rc, &lc->choice, 0);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 0;
						sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE >> 8;
						sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE & 0xFF;
						ssize++;
						//rc_bittree(rc, lc->low[pos_state], LEN_LOW_BITS, len);
						sdata[cadta++] = RC_PACKAGE_BITTREE;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = LEN_LOW_BITS-1;
						sdata[cadta++] = mlen;
						sdata[cadta++] = RC_REP_LEN_ENCODE_LOW(pos_state,0) >> 8;
						sdata[cadta++] = RC_REP_LEN_ENCODE_LOW(pos_state,0) & 0xFF;
						ssize++;
						//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
					} else {
						//rc_bit(rc, &lc->choice, 1);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 1;
						sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE >> 8;
						sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE & 0xFF;
						ssize++;
						mlen -= LEN_LOW_SYMBOLS;

						if (mlen < LEN_MID_SYMBOLS) {
							//rc_bit(rc, &lc->choice2, 0);
							sdata[cadta++] = RC_PACKAGE_BIT;
							//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
							sdata[cadta++] = 0;
							sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2 >> 8;
							sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2 & 0xFF;
							ssize++;
							//rc_bittree(rc, lc->mid[pos_state], LEN_MID_BITS, len);
							sdata[cadta++] = RC_PACKAGE_BITTREE;
							//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
							sdata[cadta++] = LEN_MID_BITS-1;
							sdata[cadta++] = mlen;
							sdata[cadta++] = RC_REP_LEN_ENCODE_MID(pos_state,0) >> 8;
							sdata[cadta++] = RC_REP_LEN_ENCODE_MID(pos_state,0) & 0xFF;
							ssize++;
							//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
						} else {
							//rc_bit(rc, &lc->choice2, 1);
							sdata[cadta++] = RC_PACKAGE_BIT;
							//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
							sdata[cadta++] = 1;
							sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2 >> 8;
							sdata[cadta++] = RC_REP_LEN_ENCODE_CHOICE2 & 0xFF;
							ssize++;
							mlen -= LEN_MID_SYMBOLS;
							//rc_bittree(rc, lc->high, LEN_HIGH_BITS, len);
							sdata[cadta++] = RC_PACKAGE_BITTREE;
							//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
							sdata[cadta++] = LEN_HIGH_BITS-1;
							sdata[cadta++] = mlen;
							sdata[cadta++] = RC_REP_LEN_ENCODE_HIGH(0) >> 8;
							sdata[cadta++] = RC_REP_LEN_ENCODE_HIGH(0) & 0xFF;
							ssize++;
							//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
						}
					}

					//-----END-LENGTH--------------------------------------
				
					update_long_rep(rc.state);
				}
				//-------------END--rep_match----------------------------------------
			} else {
				//rc_bit(symStream,probsStream,outStreamSize,RC_IS_REP(rc.state), 0);
				sdata[cadta++] = RC_PACKAGE_BIT;
				//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
				sdata[cadta++] = 0;
				sdata[cadta++] = RC_IS_REP(rc.state) >> 8;
				sdata[cadta++] = RC_IS_REP(rc.state) & 0xFF;
				ssize++;
				//match(&rc, pos_state, tOffset-4, len);
				//---------START----match----------------------------------
			    update_match(rc.state);
				//length(rc, &rc->match_len_encoder, pos_state, len, false);
				//-----START---length----------------------------------------------
				uint8_t mlen = len;
				mlen -= MATCH_LEN_MIN;
				if (mlen < LEN_LOW_SYMBOLS) {
					//rc_bit(rc, &lc->choice, 0);
					sdata[cadta++] = RC_PACKAGE_BIT;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = 0;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE >> 8;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE & 0xFF;
					ssize++;
					//rc_bittree(rc, lc->low[pos_state], LEN_LOW_BITS, len);
					sdata[cadta++] = RC_PACKAGE_BITTREE;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = LEN_LOW_BITS-1;
					sdata[cadta++] = mlen;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_LOW(pos_state,0) >> 8;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_LOW(pos_state,0) & 0xFF;
					ssize++;
					//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
				} else {
					//rc_bit(rc, &lc->choice, 1);
					sdata[cadta++] = RC_PACKAGE_BIT;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = 1;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE >> 8;
					sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE & 0xFF;
					ssize++;
					mlen -= LEN_LOW_SYMBOLS;

					if (mlen < LEN_MID_SYMBOLS) {
						//rc_bit(rc, &lc->choice2, 0);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 0;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2 >> 8;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2 & 0xFF;
						ssize++;
						//rc_bittree(rc, lc->mid[pos_state], LEN_MID_BITS, len);
						sdata[cadta++] = RC_PACKAGE_BITTREE;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = LEN_MID_BITS-1;
						sdata[cadta++] = mlen;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_MID(pos_state,0) >> 8;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_MID(pos_state,0) & 0xFF;
						ssize++;
						//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
					} else {
						//rc_bit(rc, &lc->choice2, 1);
						sdata[cadta++] = RC_PACKAGE_BIT;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = 1;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2 >> 8;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_CHOICE2 & 0xFF;
						ssize++;
						mlen -= LEN_MID_SYMBOLS;
						//rc_bittree(rc, lc->high, LEN_HIGH_BITS, len);
						sdata[cadta++] = RC_PACKAGE_BITTREE;
						//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
						sdata[cadta++] = LEN_HIGH_BITS-1;
						sdata[cadta++] = mlen;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_HIGH(0) >> 8;
						sdata[cadta++] = RC_MATCH_LEN_ENCODE_HIGH(0) & 0xFF;
						ssize++;
						//printf("[%s:%u] [%u] ,[%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
					}
				}

				//-----END-LENGTH--------------------------------------
				uint32_t distance = tOffset-4;
				const uint32_t pos_slot = get_pos_slot(distance);//get_pos_slot(distance);//get_pos_slot3(distance);
				const uint32_t len_to_pos_state = get_len_to_pos_state(len);
				//rc_bittree(symStream,probsStream,outStreamSize,RC_POS_SLOT(len_to_pos_state),POS_SLOT_BITS, pos_slot);
				sdata[cadta++] = RC_PACKAGE_BITTREE;
				//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
				sdata[cadta++] = POS_SLOT_BITS-1;
				sdata[cadta++] = pos_slot;
				sdata[cadta++] = RC_POS_SLOT(len_to_pos_state) >> 8;
				sdata[cadta++] = RC_POS_SLOT(len_to_pos_state) & 0xFF;
				ssize++;
				if (pos_slot >= START_POS_MODEL_INDEX && pos_slot < END_POS_MODEL_INDEX) {
					const uint32_t footer_bits = (pos_slot >> 1) - 1;	// ******[0 to 6]
					const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
					const uint32_t pos_reduced = distance - base;
					//printf("[%s:%u] footer_bits:%u pos_reduced:%u prob:%u\n",__func__,__LINE__,footer_bits,pos_reduced,RC_POS_SPECIAL(base - pos_slot - 1));
//					rc_bittree_reverse(symStream,probsStream,outStreamSize,RC_POS_SPECIAL(base - pos_slot - 1), footer_bits, pos_reduced);
					sdata[cadta++] = RC_PACKAGE_BITTREE_REVERSE;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = footer_bits >> 24;
					sdata[cadta++] = footer_bits >> 16;
					sdata[cadta++] = footer_bits >> 8;
					sdata[cadta++] = footer_bits & 0xFF;
					sdata[cadta++] = pos_reduced >> 24;
					sdata[cadta++] = pos_reduced >> 16;
					sdata[cadta++] = pos_reduced >> 8;
					sdata[cadta++] = pos_reduced & 0xFF;
					sdata[cadta++] = RC_POS_SPECIAL(base - pos_slot - 1) >> 8;
					sdata[cadta++] = RC_POS_SPECIAL(base - pos_slot - 1) & 0xFF;
					ssize++;
					//printf("[%s:%u] [%u]%u ,[%u,%u,%u,%u] ,[%u,%u,%u,%u], [%u,%u]\n",__func__,__LINE__,sdata[cadta-11],cadta,sdata[cadta-10],sdata[cadta-9],sdata[cadta-8],sdata[cadta-7],sdata[cadta-6],sdata[cadta-5],sdata[cadta-4],sdata[cadta-3],sdata[cadta-2],sdata[cadta-1]);
				}
				if (pos_slot >= START_POS_MODEL_INDEX && pos_slot >= END_POS_MODEL_INDEX) {
					const uint32_t footer_bits = (pos_slot >> 1) - 1;  // ***[6 to 30]
					const uint32_t base = (2 | (pos_slot & 1)) << footer_bits;
					const uint32_t pos_reduced = distance - base;
//					rc_direct(symStream,outStreamSize,pos_reduced >> ALIGN_BITS, footer_bits - ALIGN_BITS);
					const uint32_t f_A = footer_bits - ALIGN_BITS;
					sdata[cadta++] = RC_PACKAGE_DIRECT;
					//printf("[%s:%d] IN:%d [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = footer_bits >> 24;
					sdata[cadta++] = footer_bits >> 16;
					sdata[cadta++] = footer_bits >> 8;
					sdata[cadta++] = footer_bits & 0xFF;
					sdata[cadta++] = pos_reduced >> 24;
					sdata[cadta++] = pos_reduced >> 16;
					sdata[cadta++] = pos_reduced >> 8;
					sdata[cadta++] = pos_reduced & 0xFF;
					ssize++;
//					rc_bittree_reverse(symStream,probsStream,outStreamSize,rc_pos_align_OFFSET, ALIGN_BITS, pos_reduced & ALIGN_MASK);
					sdata[cadta++] = RC_PACKAGE_BITTREE_REVERSE;
					//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,sdata[cadta-1],cadta-1);
					sdata[cadta++] = 0;
					sdata[cadta++] = 0;
					sdata[cadta++] = 0;
					sdata[cadta++] = ALIGN_BITS & 0xFF;
					sdata[cadta++] = pos_reduced >> 24;
					sdata[cadta++] = pos_reduced >> 16;
					sdata[cadta++] = pos_reduced >> 8;
					sdata[cadta++] = pos_reduced & 0xFF;
					sdata[cadta++] = rc_pos_align_OFFSET >> 8;
					sdata[cadta++] = rc_pos_align_OFFSET & 0xFF;
					ssize++;
				}

				rc.reps[3] = rc.reps[2];
				rc.reps[2] = rc.reps[1];
				rc.reps[1] = rc.reps[0];
				rc.reps[0] = tOffset-4;
				//++rc->match_price_count;
				//----------END match-------------------------------------

			}
			
		}

		ap_uint<512> sym64;
		lzma_rc_1_1_1:for(int i=0;i<64;i++) {
			#pragma HLS unroll
			if(i<cadta) {
				//printf("...[%u]--> [%u,%u]\n",i,sdata[i],pdata[i]);
				sym64.range(((i+1)*8)-1,i*8) = sdata[i];
			}
		}
		outStream << sym64;
		outStreamSize << ssize;
		rcposition += len;
	}


	if (!rc.is_flushed) {
		rc.is_flushed = true;
		ap_uint<512> sym64;
		//rc_flush(symStream,outStreamSize);
		rc_flush:for (size_t i = 0; i < 5; ++i) {
			#pragma HLS unroll
			sym64.range(((i+1)*8)-1,i*8) = RC_PACKAGE_FLUSH;
			//sdata[cadta++] = RC_FLUSH;
		}
		outStream << sym64;
		outStreamSize << 5;
		//rc_encode(&rc, outStream, outStreamSize);
	}

	rc.is_flushed = false;
	outStreamSize << 0;
}

void lzma_rc_1_2 (		
        hls::stream<ap_uint<512> > &inStream,
        hls::stream<uint8_t> &inStreamSize,
        hls::stream<ap_uint<512> > &symStream64,
        hls::stream<ap_uint<1024> > &probsStream64,
        hls::stream<uint16_t> &outStreamSize64
)
{
	ap_uint<512> pack;
	uint8_t cpack;
	uint8_t next_size = inStreamSize.read();
	//printf("[%s:%d] insize:%u\n",__func__,__LINE__,next_size);
	uint8_t curr_size = 0;
	uint8_t i =0;
    bool end_of_byte = true;
	uint8_t sdata[30];
	uint16_t pdata[30];
	#pragma HLS ARRAY_PARTITION variable=sdata complete
	#pragma HLS ARRAY_PARTITION variable=pdata complete
	uint16_t cdata = 0;
	//uint32_t xxxx = 0;
	lzma_rc_1_2:for (; next_size != 0 || end_of_byte == false; i++ ) {
		#pragma HLS PIPELINE II=1
		//#pragma HLS dependence variable=sdata inter false
		//#pragma HLS dependence variable=pdata inter false
		if (i == 0 ){
			curr_size = next_size;
			next_size = inStreamSize.read();
			//printf("[%s:%d] insize:%u\n",__func__,__LINE__,next_size);
			//printf("----------\n");
			pack = inStream.read();
			cpack = 0;
		}
		cdata = 0;
		//printf("[%s:%d] IN:%u [%u]\n",__func__,__LINE__,(uint32_t)pack(((cpack+1)*8)-1,cpack*8),cpack);
		switch(pack(((cpack+1)*8)-1,cpack*8)) {
			case RC_PACKAGE_BIT: {
						cpack++;
						sdata[cdata] = pack(((cpack+1)*8)-1,cpack*8);
						cpack++;
						uint8_t p1 = pack(((cpack+1)*8)-1,cpack*8);
						uint8_t p2 = pack(((cpack+2)*8)-1,(cpack+1)*8);
						uint16_t prob = pack(((cpack+1)*8)-1,cpack*8)<<8 | pack(((cpack+2)*8)-1,(cpack+1)*8);	
						pdata[cdata++] = pack(((cpack+1)*8)-1,cpack*8)<<8 | pack(((cpack+2)*8)-1,(cpack+1)*8);
						//printf("[%s:%u] RC_PACKAGE_BIT:[%u]/[%u,%u]\n",__func__,__LINE__,pdata[cdata-1],p1,p2);
						cpack+=2;
						break;
					}
			case RC_PACKAGE_BITTREE: {
						cpack++;
						uint8_t start = pack(((cpack+1)*8)-1,cpack*8);
						cpack++;
						uint8_t tstr = pack(((cpack+1)*8)-1,cpack*8);
						cpack++;
						uint16_t prob = pack(((cpack+1)*8)-1,cpack*8)<<8 | pack(((cpack+2)*8)-1,(cpack+1)*8);
						cpack+=2;
						//printf("[%s:%u] RC_PACKAGE_BITTREE: [%u] ,[%u], [%u]/[%u,%u]\n",__func__,__LINE__,start,tstr,prob,p1,p2);
						uint32_t model_index = 1;
						rc_bittree:for(int i=7; i>= 0;i--){
							#pragma HLS unroll
							if(i<=start) {
								const uint32_t bit = (tstr >> i) & 1;
								uint32_t index = model_index;
								model_index = (index << 1) + bit;
								sdata[cdata] = bit;
								pdata[cdata++] = prob+index;
							}
						}
						break;
					}
			case RC_PACKAGE_BITTREE_REVERSE: {
						cpack++;
						uint32_t footer_bits = pack(((cpack+1)*8)-1,cpack*8)<<24 | pack(((cpack+2)*8)-1,(cpack+1)*8)<<16 | pack(((cpack+3)*8)-1,(cpack+2)*8)<<8 | pack(((cpack+4)*8)-1,(cpack+3)*8);
						cpack+=4;
						uint32_t pos_reduced = pack(((cpack+1)*8)-1,cpack*8)<<24 | pack(((cpack+2)*8)-1,(cpack+1)*8)<<16 | pack(((cpack+3)*8)-1,(cpack+2)*8)<<8 | pack(((cpack+4)*8)-1,(cpack+3)*8);
						cpack+=4;
						uint16_t prob = pack(((cpack+1)*8)-1,cpack*8)<<8 | pack(((cpack+2)*8)-1,(cpack+1)*8);
						cpack+=2;
						//printf("cpack:%u footer_bits:%u pos_reduced:%u prob:%u [%u,%u,%u,%u]\n",cpack,footer_bits,pos_reduced,prob,p1,p2,p3,p4);
						//getchar();
						uint32_t model_index = 1;
						rc_bittree_reverse_in1:for(int i=0;i<6;i++){ //footer_bits 
							#pragma HLS unroll
							if(i<footer_bits) {
							const uint32_t bit = (pos_reduced >> i) & 1;
							uint32_t index = model_index;
							model_index = (index << 1) + bit;
							sdata[cdata] = bit;
							pdata[cdata++] = (prob+index);
							}
						}
						break;
					}
			case RC_PACKAGE_DIRECT: {
						cpack++;
						uint32_t footer_bits = pack(((cpack+1)*8)-1,cpack*8)<<24 | pack(((cpack+2)*8)-1,(cpack+1)*8)<<16 | pack(((cpack+3)*8)-1,(cpack+2)*8)<<8 | pack(((cpack+4)*8)-1,(cpack+3)*8);
						cpack+=4;
						uint32_t pos_reduced = pack(((cpack+1)*8)-1,cpack*8)<<24 | pack(((cpack+2)*8)-1,(cpack+1)*8)<<16 | pack(((cpack+3)*8)-1,(cpack+2)*8)<<8 | pack(((cpack+4)*8)-1,(cpack+3)*8);
						cpack+=4;
						const uint32_t f_A = footer_bits - ALIGN_BITS;
						rc_direct:for(int i=1; i<=26;i++){
							#pragma HLS unroll
							if(i<=f_A) {
								sdata[cdata++] = (RC_DIRECT_0 + (((pos_reduced >> ALIGN_BITS) >> (footer_bits - ALIGN_BITS - i)) & 1));
							}
						}
						break;
					}
			case RC_PACKAGE_LITERAL_MATCH: {
						cpack++;
						uint8_t match_byte = pack(((cpack+1)*8)-1,cpack*8);
						cpack++;
						uint8_t tstr = pack(((cpack+1)*8)-1,cpack*8);
						cpack++;
						uint16_t prob = pack(((cpack+1)*8)-1,cpack*8)<<8 | pack(((cpack+2)*8)-1,(cpack+1)*8);
						cpack+=2;
						uint32_t offset = 0x100;
						uint32_t symbol = (1U << 8) + tstr;
						uint32_t l_match_byte = match_byte;
						//symbol += (1U << 8);
						literal_matched:for(int i=0;i<8;i++) {
							#pragma HLS unroll
							l_match_byte <<= 1;
							const uint32_t match_bit = l_match_byte & offset;
							const uint32_t subcoder_index = offset + match_bit + (symbol >> 8);					
							const uint32_t bit = (symbol >> 7) & 1;
							sdata[cdata] = bit;
							pdata[cdata++] = prob+subcoder_index;
							symbol <<= 1;
							offset &= ~(l_match_byte ^ symbol);
						}
						break;
					}
			case RC_PACKAGE_FLUSH: {
						cpack++;
						sdata[cdata++] = RC_FLUSH;
						break;
					}
			default :
					break;
			
		}
		

		ap_uint<512> sym64;
        ap_uint<1024> probs64;
		lzma_rc_1_1_1:for(int i=0;i<30;i++) {
			#pragma HLS unroll
			if(i<cdata) {
				//printf("...[%u]--> [%u,%u]\n",i,sdata[i],pdata[i]);
				sym64.range(((i+1)*8)-1,i*8) = sdata[i];
				probs64.range(((i+1)*16)-1,i*16) = pdata[i];
				//printf("-----[%u]--> [%u,%u]\n",i,(uint32_t)sym64.range(((i+1)*8)-1,i*8),(uint32_t)probs64.range(((i+1)*16)-1,i*16));
			}
		}
		symStream64 << sym64;
		probsStream64 << probs64;
		outStreamSize64 << cdata;
		//xxxx+=cdata;
		//if(cdata == 0)
			//printf("[%s:%d] outsize:%u [%u]/%u\n",__func__,__LINE__,cdata,xxxx,curr_size);
		if(i == (curr_size-1) && next_size == 0){
			end_of_byte = true;
		} else if(i == (curr_size-1) && next_size != 0) {
        	i =-1;
        	end_of_byte = true;
		}else{
			end_of_byte = false;
		}
	}
	outStreamSize64 << 0;
}

void lzma_rc_converter (
        hls::stream<ap_uint<512> > &symStream64,
        hls::stream<ap_uint<1024> > &probsStream64,
        hls::stream<uint16_t> &outStreamSize64,
        hls::stream<ap_uint<8> > &symStream,
        hls::stream<ap_uint<32> > &probsStream,
        hls::stream<uint16_t> &outStreamSize
)
{
    uint16_t next_size = outStreamSize64.read();
    ap_uint<512> sym64;
    ap_uint<1024> probs64;
    uint16_t curr_size = 0;    
    uint16_t i =0;
    bool end_of_byte = true;
    lzma_rc_converter:for (; next_size != 0 || end_of_byte == false; i++ ) {
        #pragma HLS PIPELINE II=1
        if (i == 0 ){
            curr_size = next_size;
            next_size = outStreamSize64.read();
		    sym64 = symStream64.read();
            probs64 = probsStream64.read();
        }
        uint8_t val = sym64.range(((i+1)*8)-1,i*8);
        symStream << val;
        if(val == 0 || val == 1)
            probsStream << probs64.range(((i+1)*16)-1,i*16);
        outStreamSize << 1;
        if(i == (curr_size-1) && next_size == 0){
            end_of_byte = true;
        } else if(i == (curr_size-1) && next_size != 0) {
            i =-1;
            end_of_byte = true;
        }else{
            end_of_byte = false;
        }
    }
    outStreamSize << 0;
}

/*
void rc_reset(Range_Coder &rc)
{
    rc.low = 0;
    rc.cache_size = 1;
    rc.range = UINT32_MAX;
    rc.cache = 0;
    //rc.count = 0;
    //rc.pos = 0;
}
*/

void lzma_rc_2 (
        hls::stream<ap_uint<8> > &symStream,
        hls::stream<ap_uint<32> > &probsStream,
        hls::stream<uint16_t> &outStreamSize,
        hls::stream<ap_uint<32> > &rangeStream,
        hls::stream<ap_uint<64> > &lowStream,
        hls::stream<uint16_t> &outStreamSize2
        )
{
    uint64_t rc_low = 0;
    uint32_t rc_range = 0xffffffff;
    uint32_t temp_rc_range;
    ap_uint<32> p;
    probability prob;
    probability rc_allprobs[(LITERAL_CODERS_MAX*LITERAL_CODER_SIZE)	//rc_literal
                           +(STATES*POS_STATES_MAX)					//rc_is_match
                           +(STATES*POS_STATES_MAX)					//rc_is_rep0_long
                           +(STATES)									//rc_is_rep
                           +(STATES)									//rc_is_rep0
                           +(STATES)									//rc_is_rep1
                           +(STATES)									//rc_is_rep2
                           +(LEN_TO_POS_STATES*POS_SLOTS)				//rc_pos_slot
                           +(FULL_DISTANCES - END_POS_MODEL_INDEX)		//rc_pos_special
                           +(ALIGN_TABLE_SIZE)							//rc_pos_align
                           +1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+LEN_HIGH_SYMBOLS
                           +1+1+(POS_STATES_MAX*LEN_LOW_SYMBOLS)+(POS_STATES_MAX*LEN_MID_SYMBOLS)+LEN_HIGH_SYMBOLS ];

    for (size_t i = 0; i <  END_ALLPROBS; ++i) {
        //#pragma HLS unroll
        #pragma HLS PIPELINE II=1
        bit_reset(rc_allprobs[i]);
    }

    rangeStream << 0;
    lowStream << 0;
    outStreamSize2 << 1;

    lzma_rc_2:for (uint16_t size = outStreamSize.read() ; size != 0 ; size = outStreamSize.read()) {
        #pragma HLS PIPELINE II=1
        #pragma HLS dependence variable=rc_allprobs inter false

        ap_uint<8> symb = symStream.read();

        if(symb == RC_BIT_0 || symb == RC_BIT_1){
            p = probsStream.read();
            prob = rc_allprobs[p];
            temp_rc_range = (rc_range >> RC_BIT_MODEL_TOTAL_BITS)* prob; 
        }

        switch (symb) {
        case RC_BIT_0: {
            rc_range = temp_rc_range;
            prob += (RC_BIT_MODEL_TOTAL - prob) >> RC_MOVE_BITS;
            rc_allprobs[p] = prob;
            break;
        }
        case RC_BIT_1: {
            const uint32_t bound = temp_rc_range;
            rc_low += bound;
            rc_range -= bound;
            prob -= prob >> RC_MOVE_BITS;
            rc_allprobs[p] = prob;
            break;
        }
        case RC_DIRECT_0:
            rc_range >>= 1;
            break;
        case RC_DIRECT_1:
            rc_range >>= 1;
            rc_low += rc_range;
            break;
        case RC_FLUSH:
            rc_range = UINT32_MAX;
        default:
            break;
        }
        if (rc_range < RC_TOP_VALUE || symb == RC_FLUSH) {
            rangeStream << rc_range;
            lowStream << rc_low;
            outStreamSize2 << 1;
            rc_low = (rc_low & 0x00FFFFFF) << RC_SHIFT_BITS;
            rc_range <<= RC_SHIFT_BITS;
        }
    }
    outStreamSize2 << 0;
}


void lzma_rc_3 (
        hls::stream<ap_uint<32> > &rangeStream,
        hls::stream<ap_uint<64> > &lowStream,
        hls::stream<uint16_t> &outStreamSize2,
        hls::stream<ap_uint<8> > &rcStream,
        hls::stream<uint16_t> &rcOutSize,
        uint32_t input_size
        )
{
    uint32_t rc_cache_size = 0;
    uint64_t rc_low;
    uint32_t range;
    uint8_t rc_cache = 0;
    uint32_t outcount = 0;
    lzma_rc_3:for (uint16_t size = outStreamSize2.read() ; size != 0 ; size = outStreamSize2.read()) {
        #pragma HLS PIPELINE II=1
        rc_low = lowStream.read().range(63,0);
        range = rangeStream.read().range(31,0);
        if ((uint32_t)(rc_low) < (uint32_t)(0xFF000000)
                || (uint32_t)(rc_low >> 32) != 0) {
            rc_shift_low:for(int i = 0;i<rc_cache_size && outcount < input_size;i++) {
                #pragma HLS PIPELINE II=1
                uint8_t val = (((i==0)?rc_cache:0xFF) + (uint8_t)(rc_low >> 32));
                rcStream << val;
                rcOutSize << 1;
                outcount++;
            }
            rc_cache = (rc_low >> 24) & 0xFF;
            rc_cache_size = 0;
        }
        ++rc_cache_size;
    }
    rcOutSize << 0;
}


/*
void lzma_rc_2 (
		hls::stream<ap_uint<8> > &symStream,
        hls::stream<ap_uint<32> > &probsStream,
        hls::stream<uint16_t> &outStreamSize,
		hls::stream<ap_uint<8> > &rcStream,
        hls::stream<uint16_t> &rcOutSize
        )
{
	Range_Coder rc;	
    uint32_t lc = 3;
    uint32_t pb = 2;
    uint32_t lp = 0;
	initrc(&rc,lc,pb,lp);
	lzma_rc_2:for (uint16_t size = outStreamSize.read() ; size != 0 ; size = outStreamSize.read()) {
		#pragma HLS PIPELINE II=1
		ap_uint<8> symb = symStream.read();
		switch (symb) {
		case RC_BIT_0: {
			ap_uint<32> p = probsStream.read();
			probability prob = rc.allprobs[p];
			rc.range = (rc.range >> RC_BIT_MODEL_TOTAL_BITS)* prob;
			prob += (RC_BIT_MODEL_TOTAL - prob) >> RC_MOVE_BITS;
			rc.allprobs[p] = prob;
			break;
		}
		case RC_BIT_1: {
			ap_uint<32> p = probsStream.read();
			probability prob = rc.allprobs[p];
			const uint32_t bound = prob * (rc.range >> RC_BIT_MODEL_TOTAL_BITS);
			rc.low += bound;
			rc.range -= bound;
			prob -= prob >> RC_MOVE_BITS;
			rc.allprobs[p] = prob;
			break;
		}
		case RC_DIRECT_0:
			rc.range >>= 1;
			break;
		case RC_DIRECT_1:
			rc.range >>= 1;
			rc.low += rc.range;
			break;
		case RC_FLUSH:
			rc.range = UINT32_MAX;
			// Flush the last five bytes (see rc_flush()).
			//do {
			//		if (rc_shift_low(rc, out, out_pos))
			//				return true;
			//} while (++rc->pos < rc->count);
			//rc_reset(rc);
			//return false;
		default:
			break;
		}
		
		if (rc.range < RC_TOP_VALUE || symb == RC_FLUSH) {
			//---------START--rc_shift_low--------------------------
			if ((uint32_t)(rc.low) < (uint32_t)(0xFF000000)
						|| (uint32_t)(rc.low >> 32) != 0) {
				//int i=0;
				int done = 1;
				//rc_shift_low:do {
				rc_shift_low:for(int i = 0;i<rc.cache_size;i++) {
					#pragma HLS PIPELINE II=1
					//if(i < rc.cache_size && done != 1) {
						uint8_t val = (((i==0) ? rc.cache:0xFF) + (uint8_t)(rc.low >> 32));
						rcStream << val;
						rcOutSize << 1;
					//}else {
					//	done = 1;
					//}
					//rc.cache = 0xFF;
				//} while (--rc.cache_size != 0);
				}
				rc.cache = (rc.low >> 24) & 0xFF;
			}
			++rc.cache_size;
			rc.low = (rc.low & 0x00FFFFFF) << RC_SHIFT_BITS;
			//------------END--rc_shift_low--------------------------
			rc.range <<= RC_SHIFT_BITS;
		}
		//rcStream << symb;
		//rcOutSize << 1;
	}
	rcOutSize << 0;
}
*/
} // End of namespace
