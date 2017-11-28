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

#include <ap_int.h>
#include <hls_stream.h>
#include "vp8_hls_syn.h"
#include "vp8_hls_syn2.h"
#include <stdio.h>
#include <string.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//==========================      kernel_IntraPredLoop2_NoOut             ==============================//
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//kernel_IntraPredLoop2_NoOut
//|-memcpy
//|-TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO
//  |-TopVp8_read_2_32bit_NoStruct
//  | |-TopVp8_read__dataflow_32bit...
//  |-TopVp8_compute...
//  |-TopVp8_RecordCoeff_hls_cnt
//  | |-FindLast
//  | |-VP8RecordCoeffs_hls_str_w_cnt
//  |   |-Record_str
//  |   |-VP8EncBands_hls
//  |-TopVp8_RecordProb_hls_cnt
//  | |-RecordPorb_ReadCoeff_dataflow2_cnt
//  |   |-RecordPorb_ReadCoeff_dataflow_dc_cnt
//  |     |-RecordPorb_ReadCoeff_dataflow_ac_cnt
//  |     | |-VP8RecordCoeffs_hls_str_r_cnt
//  |     |-RecordPorb_ReadCoeff_dataflow_uv_cnt...
//  |     |-RecordPorb_ReadCoeff_dataflow2_cnt...
//  |-TopVp8_send_32bit

void  kernel_IntraPredLoop2_NoOut(
		int32_t* p_info,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob) {
#pragma HLS INTERFACE m_axi port=pout_level offset=slave bundle=gmem1 depth=65536*512/2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=p_info     offset=slave bundle=gmem0 depth=64          num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=ysrc       offset=slave bundle=gmem2 depth=4096*4096/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=usrc       offset=slave bundle=gmem3 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=vsrc       offset=slave bundle=gmem4 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_prob  offset=slave bundle=gmem5 depth=2048        num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16

#pragma HLS INTERFACE s_axilite port=p_info     bundle=control
#pragma HLS INTERFACE s_axilite port=ysrc       bundle=control
#pragma HLS INTERFACE s_axilite port=usrc       bundle=control
#pragma HLS INTERFACE s_axilite port=vsrc       bundle=control
#pragma HLS INTERFACE s_axilite port=pout_level bundle=control
#pragma HLS INTERFACE s_axilite port=pout_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int p_readinfo[64];
	memcpy(p_readinfo, p_info, 64 * sizeof(int));
	ap_uint<32> id_pic;
	ap_uint<32> mb_line;
	ap_uint<LG2_MAX_W_PIX> y_stride;
	ap_uint<LG2_MAX_W_PIX> uv_stride;
	ap_uint<LG2_MAX_W_PIX> width;
	ap_uint<LG2_MAX_W_PIX> height;
	ap_uint<LG2_MAX_NUM_MB_W> mb_w;
	ap_uint<LG2_MAX_NUM_MB_H> mb_h;
	ap_uint<WD_LMD> lambda_p16;
	ap_uint<WD_LMD> lambda_p44;
	ap_uint<WD_LMD> tlambda;
	ap_uint<WD_LMD> lambda_uv;
	ap_uint<WD_LMD> tlambda_m;
	hls_QMatrix hls_qm1, hls_qm2, hls_qm_uv;
	ap_int<WD_sharpen * 16> ap_sharpen, ap_sharpen_uv;

	//Initializing image variables, once for one picture
	{// For convenience, extend the code at top module to show all parameters used by kernel of intra-prediction
		id_pic = p_readinfo[0];//reserved for future
		mb_line = p_readinfo[1];// reserved for future, to show current line number of mb
		y_stride = p_readinfo[2];
		uv_stride = p_readinfo[3];
		width = p_readinfo[4];
		height = p_readinfo[5];
		mb_w = p_readinfo[2 + 2 + 2];
		mb_h = p_readinfo[3 + 2 + 2];
		lambda_p16 = p_readinfo[4 + 2 + 2];
		lambda_p44 = p_readinfo[5 + 2 + 2];
		tlambda = p_readinfo[6 + 2 + 2];
		lambda_uv = p_readinfo[7 + 2 + 2];
		tlambda_m = p_readinfo[8 + 2 + 2];

		hls_qm1.q_0 = p_readinfo[11 + 2];     // quantizer steps
		hls_qm1.q_n = p_readinfo[12 + 2];
		hls_qm1.iq_0 = p_readinfo[13 + 2];    // reciprocals fixed point.
		hls_qm1.iq_n = p_readinfo[14 + 2];
		hls_qm1.bias_0 = p_readinfo[15 + 2];  // rounding bias
		hls_qm1.bias_n = p_readinfo[16 + 2];

		hls_qm2.q_0 = p_readinfo[17 + 2];     // quantizer steps
		hls_qm2.q_n = p_readinfo[18 + 2];
		hls_qm2.iq_0 = p_readinfo[19 + 2];    // reciprocals fixed point.
		hls_qm2.iq_n = p_readinfo[20 + 2];
		hls_qm2.bias_0 = p_readinfo[21 + 2];  // rounding bias
		hls_qm2.bias_n = p_readinfo[22 + 2];

		hls_qm_uv.q_0 = p_readinfo[23 + 2];   // quantizer steps
		hls_qm_uv.q_n = p_readinfo[24 + 2];
		hls_qm_uv.iq_0 = p_readinfo[25 + 2];  // reciprocals fixed point.
		hls_qm_uv.iq_n = p_readinfo[26 + 2];
		hls_qm_uv.bias_0 = p_readinfo[27 + 2];// rounding bias
		hls_qm_uv.bias_n = p_readinfo[28 + 2];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen,i,WD_sharpen) = p_info[29 + 2 + i];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen_uv,i,WD_sharpen) = p_readinfo[29 + 2 + 16 + i];
	}//end of initialization
	int dirty = 0;
	TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(id_pic,  //p_info[0],
				mb_line,  //p_info[1],
				y_stride,  //p_info[2],  // ,//pic->y_stride,
				uv_stride,  //p_info[3], // ,//pic->uv_stride
				width,  //p_info[4],  // ,//pic->width
				height,  //p_info[5],  // ,//pic->height
				mb_w,  //p_info[2+2+2],///,
				mb_h,  //p_info[3+2+2],//,
				lambda_p16,  //p_info[4+2+2],//dqm->lambda_i16_,
				lambda_p44,  //p_info[5+2+2],//dqm->lambda_i4_,
				tlambda,  //p_info[6+2+2],//dqm->tlambda_,
				lambda_uv,  //p_info[7+2+2],//dqm->lambda_uv_,
				tlambda_m,  //p_info[8+2+2],//dqm->lambda_mode_,
				hls_qm1, hls_qm2, hls_qm_uv, ap_sharpen, ap_sharpen_uv, ysrc, //4096x4096
				usrc, //2048x2048
				vsrc, //2048x2048
				pout_level, //65536*512
				pout_prob, &dirty);
}//==END Top kernel_IntraPredLoop2_NoOut ==//

void TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(
		ap_uint<32> id_pic,
		ap_uint<32> mb_line,
		ap_uint<LG2_MAX_W_PIX> y_stride,
		ap_uint<LG2_MAX_W_PIX> uv_stride,
		ap_uint<LG2_MAX_W_PIX> width,
		ap_uint<LG2_MAX_W_PIX> height,
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		ap_uint<WD_LMD> lambda_p16,
		ap_uint<WD_LMD> lambda_p44,
		ap_uint<WD_LMD> tlambda,
		ap_uint<WD_LMD> lambda_uv,
		ap_uint<WD_LMD> tlambda_m,
		hls_QMatrix hls_qm1,
		hls_QMatrix hls_qm2,
		hls_QMatrix hls_qm_uv,
		ap_int<WD_sharpen * 16> ap_sharpen,
		ap_int<WD_sharpen * 16> ap_sharpen_uv,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob,
		int* dirty)
{

	hls::stream<ap_uint<WD_PIX * 16> > str_out_inst;
	hls::stream<ap_uint<WD_PIX * 16> >* str_out = &str_out_inst;
#pragma HLS STREAM variable=str_out depth=8*8
#pragma HLS DATAFLOW
	hls::stream<ap_uint<WD_PIX * 16> > str_din_y;
	hls::stream<ap_uint<WD_PIX * 16> > str_din_uv;
#pragma HLS STREAM variable=str_din_y depth=16*128
#pragma HLS STREAM variable=str_din_uv depth=8*128
	TopVp8_read_2_32bit_NoStruct( //For 4k,
			ysrc,
			usrc,
			vsrc,
			y_stride,
			uv_stride,
			width,
			height,
			mb_w,
			mb_h,
			&str_din_y,
			&str_din_uv);

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_y;
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
	hls::stream<ap_int<64> > str_pred;
	hls::stream<ap_int<6> > str_ret;
#pragma HLS STREAM variable=str_level_y depth=16*4*4//Deep
#pragma HLS STREAM variable=str_level_dc depth=1*4*4//Deep
#pragma HLS STREAM variable=str_level_uv depth=8*4*4//Deep
#pragma HLS STREAM variable=str_pred depth=1*4*4//Deep
#pragma HLS STREAM variable=str_ret depth=1*4*4//Deep
	TopVp8_compute(
			mb_w, mb_h,
			&str_din_y, &str_din_uv,
			lambda_p16, lambda_p44,tlambda, lambda_uv, tlambda_m,
			hls_qm1, hls_qm2, hls_qm_uv,
			ap_sharpen, ap_sharpen_uv,
			str_out,
			&str_level_dc,
			&str_level_y,
			&str_level_uv,
			&str_pred,
			&str_ret);

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_y2;
#pragma HLS STREAM             variable=str_level_y2  depth=4*16*4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc2;
#pragma HLS STREAM             variable=str_level_dc2 depth=4*1*4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv2;
#pragma HLS STREAM             variable=str_level_uv2 depth=4*8*4
	hls::stream<ap_int<64> >  str_pred2;
#pragma HLS STREAM   variable=str_pred2     depth=4*1*4
	hls::stream<ap_int<6> >   str_ret2;
#pragma HLS STREAM   variable=str_ret2      depth=4*1*4
	hls::stream<ap_uint<1> >  str_mb_type;
#pragma HLS STREAM   variable=str_mb_type   depth=16*1
	hls::stream<ap_uint<11> > str_rec_dc;
#pragma HLS STREAM   variable=str_rec_dc    depth=2*1*16*64
	hls::stream<ap_uint<11> > str_rec_ac;
#pragma HLS STREAM   variable=str_rec_ac    depth=4*16*16*8
	hls::stream<ap_uint<11> > str_rec_uv;
#pragma HLS STREAM   variable=str_rec_uv    depth=2*8*16*8
	hls::stream<ap_uint<8> >  str_cnt_dc;
#pragma HLS STREAM   variable=str_cnt_dc    depth=2*1*1*64
	hls::stream<ap_uint<8> >  str_cnt_ac;
#pragma HLS STREAM   variable=str_cnt_ac    depth=2*16*1*8
	hls::stream<ap_uint<8> >  str_cnt_uv;
#pragma HLS STREAM   variable=str_cnt_uv    depth=2*8*1*8
	TopVp8_RecordCoeff_hls_cnt(mb_w, mb_h, &str_level_dc, &str_level_y,
			&str_level_uv, &str_pred, &str_ret, str_mb_type, &str_level_dc2,
			&str_level_y2, &str_level_uv2, &str_pred2, &str_ret2,
			str_rec_dc,str_rec_ac, str_rec_uv,
			str_cnt_dc,str_cnt_ac, str_cnt_uv);

	*dirty = TopVp8_RecordProb_hls_cnt(mb_w, mb_h, str_mb_type,
			str_rec_dc,str_rec_ac, str_rec_uv,
			str_cnt_dc,str_cnt_ac, str_cnt_uv,
			pout_prob);

	TopVp8_send_32bit(mb_w, mb_h, &str_level_dc2, &str_level_y2, &str_level_uv2,
			&str_pred2, &str_ret2, pout_level);

	for (int y = 0; y < mb_h; y++)
		for (int x = 0; x < mb_w; x++)
			for (int i = 0; i < 24; i++)
				str_out->read();
}

void TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO_HideDirty(
		ap_uint<32> id_pic,
		ap_uint<32> mb_line,
		ap_uint<LG2_MAX_W_PIX> y_stride,
		ap_uint<LG2_MAX_W_PIX> uv_stride,
		ap_uint<LG2_MAX_W_PIX> width,
		ap_uint<LG2_MAX_W_PIX> height,
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		ap_uint<WD_LMD> lambda_p16,
		ap_uint<WD_LMD> lambda_p44,
		ap_uint<WD_LMD> tlambda,
		ap_uint<WD_LMD> lambda_uv,
		ap_uint<WD_LMD> tlambda_m,
		hls_QMatrix hls_qm1,
		hls_QMatrix hls_qm2,
		hls_QMatrix hls_qm_uv,
		ap_int<WD_sharpen * 16> ap_sharpen,
		ap_int<WD_sharpen * 16> ap_sharpen_uv,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob)
{

#pragma HLS DATAFLOW
	hls::stream<ap_uint<WD_PIX * 16> > str_din_y;
	hls::stream<ap_uint<WD_PIX * 16> > str_din_uv;
#pragma HLS STREAM variable=str_din_y depth=16*128
#pragma HLS STREAM variable=str_din_uv depth=8*128
	TopVp8_read_2_32bit_NoStruct( //For 4k,
			ysrc,
			usrc,
			vsrc,
			y_stride,
			uv_stride,
			width,
			height,
			mb_w,
			mb_h,
			&str_din_y,
			&str_din_uv);

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_y;
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
	hls::stream<ap_int<64> > str_pred;
	hls::stream<ap_int<6> > str_ret;
#pragma HLS STREAM variable=str_level_y depth=16*4*4//Deep
#pragma HLS STREAM variable=str_level_dc depth=1*4*4//Deep
#pragma HLS STREAM variable=str_level_uv depth=8*4*4//Deep
#pragma HLS STREAM variable=str_pred depth=1*4*4//Deep
#pragma HLS STREAM variable=str_ret depth=1*4*4//Deep
	TopVp8_compute_NoOut(
			mb_w, mb_h,
			&str_din_y, &str_din_uv,
			lambda_p16, lambda_p44,tlambda, lambda_uv, tlambda_m,
			hls_qm1, hls_qm2, hls_qm_uv,
			ap_sharpen, ap_sharpen_uv,
			&str_level_dc,
			&str_level_y,
			&str_level_uv,
			&str_pred,
			&str_ret);

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_y2;
#pragma HLS STREAM             variable=str_level_y2  depth=4*16*4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc2;
#pragma HLS STREAM             variable=str_level_dc2 depth=4*1*4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv2;
#pragma HLS STREAM             variable=str_level_uv2 depth=4*8*4
	hls::stream<ap_int<64> >  str_pred2;
#pragma HLS STREAM   variable=str_pred2     depth=4*1*4
	hls::stream<ap_int<6> >   str_ret2;
#pragma HLS STREAM   variable=str_ret2      depth=4*1*4
	hls::stream<ap_uint<1> >  str_mb_type;
#pragma HLS STREAM   variable=str_mb_type   depth=16*1
	hls::stream<ap_uint<11> > str_rec_dc;
#pragma HLS STREAM   variable=str_rec_dc    depth=2*1*16*64
	hls::stream<ap_uint<11> > str_rec_ac;
#pragma HLS STREAM   variable=str_rec_ac    depth=4*16*16*8
	hls::stream<ap_uint<11> > str_rec_uv;
#pragma HLS STREAM   variable=str_rec_uv    depth=2*8*16*8
	hls::stream<ap_uint<8> >  str_cnt_dc;
#pragma HLS STREAM   variable=str_cnt_dc    depth=2*1*1*64
	hls::stream<ap_uint<8> >  str_cnt_ac;
#pragma HLS STREAM   variable=str_cnt_ac    depth=2*16*1*8
	hls::stream<ap_uint<8> >  str_cnt_uv;
#pragma HLS STREAM   variable=str_cnt_uv    depth=2*8*1*8
	TopVp8_RecordCoeff_hls_cnt(mb_w, mb_h, &str_level_dc, &str_level_y,
			&str_level_uv, &str_pred, &str_ret, str_mb_type, &str_level_dc2,
			&str_level_y2, &str_level_uv2, &str_pred2, &str_ret2,
			str_rec_dc,str_rec_ac, str_rec_uv,
			str_cnt_dc,str_cnt_ac, str_cnt_uv);

	TopVp8_RecordProb_hls_cnt_HideDirty(mb_w, mb_h, str_mb_type,
			str_rec_dc,str_rec_ac, str_rec_uv,
			str_cnt_dc,str_cnt_ac, str_cnt_uv,
			pout_prob);

	TopVp8_send_32bit(mb_w, mb_h, &str_level_dc2, &str_level_y2, &str_level_uv2,
			&str_pred2, &str_ret2, pout_level);

}
//////////======================================================================/////////////////////////////
//////////====================  TopVp8_send_32bit  =================/////////////////////////////
//////////======================================================================/////////////////////////////
void TopVp8_send_32bit(
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
	    ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_dc,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_y,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_uv,
		hls::stream< ap_int<64> >* str_pred,
		hls::stream< ap_int<6> >* str_ret,
		//output
		int32_t* pout_level
		)
{
#pragma HLS dataflow
	hls::stream< int16_t> tmp_str;
	int16_t tmp_arr[512];

    for( int y_mb = 0; y_mb < mb_h; y_mb++){
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
    	for( int x_mb = 0; x_mb < mb_w; x_mb++ ){
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
    		    TopVp8_send__strs_to_array(
            			tmp_arr,
            			//&tmp_str,
            		    str_level_dc,
            		    str_level_y,
            		    str_level_uv,
            		    str_pred,
            		    str_ret);
    			tmp_arr[420] = mb_w;
    			tmp_arr[421] = mb_h;
    		    for(int i=0 ;i<256;i++){
#pragma HLS pipeline
    		    	ap_uint<32> tmp;
    		    	tmp(15,0) = tmp_arr[i*2];
    		    	tmp(31,16) = tmp_arr[i*2+1];
    		    	pout_level[i] = tmp;

    		    }
            	pout_level += 256;
    	}
    }
}
//////////====================  TopVp8_send_32bit  =================/////////////////////////////
void TopVp8_send__strs_to_array(
		short int* pout,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_dc,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_y,
		hls::stream< ap_int<WD_LEVEL*16> >* str_level_uv,
		hls::stream< ap_int<64> >* str_pred,
		hls::stream< ap_int<6> >* str_ret
		)
{
#pragma HLS PIPELINE
  int x, y, ch;
  ap_int<WD_LEVEL*16> tmp16 = str_level_dc->read();

    short int *y_dc_levels=pout;//[16];
    CPY16(y_dc_levels, tmp16, WD_LEVEL);
    pout+=16;


  // luma-AC
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
#pragma HLS PIPELINE
        short int *y_ac_levels=pout;//;[16]
        ap_int<WD_LEVEL*16> tmp = str_level_y->read();
        CPY16(y_ac_levels,tmp,WD_LEVEL);
        pout+=16;
    }
  }

  // U/V
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
#pragma HLS PIPELINE
        short int* uv_levels=pout;//;[16];
        ap_int<WD_LEVEL*16> tmp = str_level_uv->read();
        CPY16(uv_levels,tmp,WD_LEVEL);
        pout+=16;
      }
    }
  }

    uint16_t* pred16=(uint16_t*)pout;
	ap_uint<64> pred = str_pred->read();
#pragma HLS PIPELINE
	CPY16U(pred16,pred,4);
	pout+=16;
	ap_uint<6> ret = str_ret->read();
	*pout = (uint16_t)ret;
}
//////////====================  TopVp8_send_32bit  =================/////////////////////////////


//////////======================================================================/////////////////////////////
//////////====================  TopVp8_read_2_32bit_NoStruct  =================/////////////////////////////
//////////======================================================================/////////////////////////////
void TopVp8_read_2_32bit_NoStruct(
		//input
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		ap_uint<LG2_MAX_W_PIX> y_stride,
		ap_uint<LG2_MAX_W_PIX> uv_stride,
		ap_uint<LG2_MAX_W_PIX> width,
		ap_uint<LG2_MAX_W_PIX> height,
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		//output
		hls::stream<ap_uint<WD_PIX * 16> >* str_din_y,
		hls::stream<ap_uint<WD_PIX * 16> >* str_din_uv)
{
	TopVp8_read__dataflow_32bit( //
			//input
			y_stride,
			uv_stride,
			width,
			height,
			mb_w,
			mb_h,
			ysrc,
			usrc,
			vsrc,
			//output
			str_din_y,
			str_din_uv);
}


void TopVp8_read__dataflow_32bit(
		//input
		ap_uint<LG2_MAX_W_PIX> y_stride,//
	    ap_uint<LG2_MAX_W_PIX> uv_stride,//
	    ap_uint<LG2_MAX_W_PIX> width,//
	    ap_uint<LG2_MAX_W_PIX> height,//
	    ap_uint<LG2_MAX_NUM_MB_W> mb_w,//
	    ap_uint<LG2_MAX_NUM_MB_H> mb_h,//
		uint32_t ysrc[MAX_W_PIX*MAX_H_PIX/4],
		uint32_t usrc[MAX_W_PIX*MAX_H_PIX/4/4],
		uint32_t vsrc[MAX_W_PIX*MAX_H_PIX/4/4],
		//output
		hls::stream< ap_uint<WD_PIX*16> >* str_din_y,
		hls::stream< ap_uint<WD_PIX*16> >* str_din_uv)
{
	    /* MB Line buffer */
	    uint32_t  buff_line_mb_y[MAX_W_PIX*16/4];//32bb
	    uint32_t  buff_line_mb_u[MAX_W_PIX*4/4];//32bb
	    uint32_t  buff_line_mb_v[MAX_W_PIX*4/4];//32bb
	    uint32_t  buff_line_mb_y2[MAX_W_PIX*16/4];//32bb
	    uint32_t  buff_line_mb_u2[MAX_W_PIX*4/4];//32bb
	    uint32_t  buff_line_mb_v2[MAX_W_PIX*4/4];//32bb
	    for( int y_mb = 0; y_mb < mb_h; y_mb++){
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
//#pragma HLS dataflow
	    	hls_ReadMBLine_32bit_const(
	    		ysrc,//[MAX_W_PIX*MAX_H_PIX],
	    		usrc,//[MAX_W_PIX*MAX_H_PIX/4],
	    		vsrc,//[MAX_W_PIX*MAX_H_PIX/4],
	    		y_mb,
				y_stride,
				uv_stride,
	    		//output
	    		buff_line_mb_y,//[MAX_W_PIX*16],
	    		buff_line_mb_u,//[MAX_W_PIX*4],
	    		buff_line_mb_v//[MAX_W_PIX*4]
	    	);

	    	TopVp8_read_MB_32bit_const(//about 650 * mb_w;
	    			width,//      = p_info[4];  // = pic->width
					height,//      = p_info[5];  // = pic->height
					mb_w,// = p_info[2+2+2];///;
					mb_h,// = p_info[3+2+2];//;
	    			y_mb,
	    		    buff_line_mb_y,
	    		    buff_line_mb_u,
	    		    buff_line_mb_v,
					y_stride,
					uv_stride,
	    			//output
	    			str_din_y,
	    			str_din_uv
	    		);
	    }
}

void hls_ReadMBLine_32bit_const(
    uint32_t ysrc[MAX_W_PIX*MAX_H_PIX/4],
    uint32_t usrc[MAX_W_PIX*MAX_H_PIX/4/4],
    uint32_t vsrc[MAX_W_PIX*MAX_H_PIX/4/4],
	int     y_mb,
    int     y_stride,
	int     uv_stride,
	//output
    uint32_t  buff_line_mb_y[MAX_W_PIX*16/4],//32bb
    uint32_t  buff_line_mb_u[MAX_W_PIX*4/4],//32bb
    uint32_t  buff_line_mb_v[MAX_W_PIX*4/4] //32bb
)
{
	int offset_y = y_mb*y_stride*16/4;
	int offset_uv = y_mb*uv_stride*8/4;
#pragma HLS dataflow
    hls_CopyMBLine_y_32bit_const(buff_line_mb_y, ysrc+offset_y, y_stride*16/4);
    hls_CopyMBLine_uv_32bit_const(buff_line_mb_u, usrc+offset_uv, uv_stride*8/4);
    hls_CopyMBLine_uv_32bit_const(buff_line_mb_v, vsrc+offset_uv, uv_stride*8/4 );
}
void hls_CopyMBLine_y_32bit_const(
	uint32_t ydes[MAX_W_PIX*16/4],
	uint32_t ysrc[MAX_W_PIX*MAX_H_PIX/4],
    int num_read
)
{
	int num_loop = (num_read+NUM_BURST_READ-1) >> LG2_NUM_BURST_READ;//
    for( int line = 0; line<num_loop; line++){
#pragma HLS LOOP_TRIPCOUNT min=1920*16/4/64 max=4096*16/4/64
#pragma HLS PIPELINE
    	memcpy(ydes, ysrc, NUM_BURST_READ*sizeof(uint32_t));
    	ydes += NUM_BURST_READ;
    	ysrc += NUM_BURST_READ;
    }
}
void hls_CopyMBLine_uv_32bit_const(
    uint32_t uvdes[MAX_W_PIX/2*8/4],
	uint32_t uvsrc[MAX_W_PIX*MAX_H_PIX/4/4],
    int num_read
	)
{
	int num_loop = (num_read+NUM_BURST_READ-1) >> LG2_NUM_BURST_READ;//
    for( int line = 0; line<num_loop; line++){
#pragma HLS LOOP_TRIPCOUNT min=1920/2*8/4/64 max=4096/2*8/4/64
#pragma HLS PIPELINE
    	memcpy(uvdes, uvsrc, NUM_BURST_READ*sizeof(uint32_t));
    	uvdes += NUM_BURST_READ;
    	uvsrc += NUM_BURST_READ;
    }
}

void TopVp8_read_MB_32bit_const(
	    ap_uint<LG2_MAX_W_PIX> width,//      = p_info[4];  // = pic->width
	    ap_uint<LG2_MAX_W_PIX> height,//      = p_info[5];  // = pic->height
	    ap_uint<LG2_MAX_NUM_MB_W> mb_w,// = p_info[2+2+2];///;
	    ap_uint<LG2_MAX_NUM_MB_H> mb_h,// = p_info[3+2+2];//;
		int y_mb,
	    uint32_t  buff_line_mb_y[MAX_W_PIX*16/4],
	    uint32_t  buff_line_mb_u[MAX_W_PIX*4/4],
	    uint32_t  buff_line_mb_v[MAX_W_PIX*4/4],
	    //uint32_t  buff_line_mb_y2[MAX_W_PIX*16/4],
	    //uint32_t  buff_line_mb_u2[MAX_W_PIX*4/4],
	    //uint32_t  buff_line_mb_v2[MAX_W_PIX*4/4],
		int stride_y,
		int stride_uv,
		//output
		hls::stream< ap_uint<WD_PIX*16> >* str_din_y,
		hls::stream< ap_uint<WD_PIX*16> >* str_din_uv
	)
{
	for( int x_mb = 0; x_mb < mb_w; x_mb++ ){
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
		ap_uint<WD_PIX*16>  ap_yuv_in_[24];
		ap_uint<WD_PIX*16>  ap_y_in_[16];
		ap_uint<WD_PIX*16>  ap_u_in_[4];
		ap_uint<WD_PIX*16>  ap_v_in_[4];
		hls_GetMB_parallel_32bit_const(// 599 cycyle dataflow
						buff_line_mb_y,//[MAX_W_PIX*16],
						buff_line_mb_u,//[MAX_W_PIX*4],
						buff_line_mb_v,//[MAX_W_PIX*4],
						x_mb,
						y_mb,
						width,
						height,
						stride_y,
						stride_uv,
						ap_y_in_,//[16],
						ap_u_in_,//[4],
						ap_v_in_//[4]
						);

		for(int i=0;i<16;i++)
			str_din_y->write(ap_y_in_[i]);
		for(int i=0;i<4;i++)
		    str_din_uv->write(ap_u_in_[i]);
		for(int i=0;i<4;i++)
			str_din_uv->write(ap_v_in_[i]);
	}
}
void hls_GetMB_parallel_32bit_const(
		uint32_t ysrc_MBline[MAX_W_PIX*16/4],
		uint32_t usrc_MBline[MAX_W_PIX*4/4],
		uint32_t vsrc_MBline[MAX_W_PIX*4/4],
		int x_mb,
		int y_mb,
		int width,
		int height,
		int stride_y,
		int stride_uv,
		ap_uint<WD_PIX*16>  ap_y_in_[16],
		ap_uint<WD_PIX*16>  ap_u_in_[4],
		ap_uint<WD_PIX*16>  ap_v_in_[4]
		)
{
//#pragma HLS DATAFLOW
	hls_GetMB_y_32bit_const(
			ysrc_MBline,
			x_mb,
			y_mb,
			width,
			height,
			stride_y,
			ap_y_in_
			);
	hls_GetMB_uv_32bit_const(
			usrc_MBline,
			x_mb,
			y_mb,
			width,
			height,
			stride_uv,
			ap_u_in_
			);
	hls_GetMB_uv_32bit_const(
				vsrc_MBline,
				x_mb,
				y_mb,
				width,
				height,
				stride_uv,
				ap_v_in_
				);
}

static int MinSize32(int a, int b) { return (a < b) ? a : b; };
void hls_GetMB_y_32bit_const(
		uint32_t src[MAX_W_PIX*16/4],
		int x_mb,
		int y_mb,
		int width,
		int height,
		int stride,
		ap_uint<WD_PIX*16>  ap_y_in_[16]
		)
{
	int x = x_mb;
	int y = y_mb;
	const int w = MinSize32(width - x * 16, 16);
	const int h = MinSize32(height - y * 16, 16);
	int off = (w-1)%4;
	uint32_t* ysrc = src + x_mb*16/4;//32bb
	//Two following variables create a slide window
	uint32_t rem_dat;
	uint32_t crt_dat;
	uint32_t w32;
	int addr8 = 0;
    for (int i = 0; i < 16; ++i) {
    	int addr32 = (addr8) >> 2;
    	int num_rem = 4 - ( addr8 & 3 );
    	rem_dat = ysrc[addr32++];
    	for(int base=0; base<4; base++){//j= 0, 4, 8, 12; base = 0,1,2,3
  #pragma HLS PIPELINE II=1
				int  j = base<<2;
				bool isAllIn  = base < (w>>2); //((j+3)<=(w-1));
				bool isAllOut = (j>=w);
				bool isOver   = (num_rem+j)>= w;
				uint32_t rem_dat_2 = rem_dat;
        		if(isOver)
        			crt_dat = rem_dat;
        		else
        			crt_dat = rem_dat = ysrc[addr32++];

        		w32 = get32bits_2_const(num_rem, rem_dat_2, crt_dat);
				ap_uint<32> tmp = GetEdgeImage(w32, off, isAllIn, isAllOut );
				VCT_GET(ap_y_in_[(i&12) + base],i%4, WD_PIX*4) =  tmp(31,0);
      }
      if(i<(h-1))
    	  addr8 += stride;
    }
}

ap_uint<32> GetEdgeImage(ap_uint<32> org, int off, bool isAllIn, bool isAllOut )
{
#pragma HLS PIPELINE
    ap_uint<32> tmp = org;
    ap_uint<8> edge = org(7+off*8, off*8);
    if(isAllIn){
    	tmp = org;
    }else if(isAllOut){
        tmp(7,0) = edge;
        tmp(15,8) = edge;
        tmp(23,16) = edge;
        tmp(31,24) = edge;
    }else{ //at the edge
        if(off==0){
            tmp(15,8) = edge;
            tmp(23,16) = edge;
            tmp(31,24) = edge;
        }else if(off==1){
            tmp(23,16) = edge;
            tmp(31,24) = edge;
        }else if(off==2){
            tmp(31,24) = edge;
        }else{
            tmp(7,0) = edge;
            tmp(15,8) = edge;
            tmp(23,16) = edge;
            tmp(31,24) = edge;
        }
    }
    return tmp;
}

void hls_GetMB_uv_32bit_const(
		uint32_t src[MAX_W_PIX*4/4],
		int x_mb,
		int y_mb,
		int width,
		int height,
		int stride,
		ap_uint<WD_PIX*16>  ap_uv_in_[4]
		)
{
	int x = x_mb;
	int y = y_mb;
	const int w = MinSize32(width - x * 16, 16);
	const int h = MinSize32(height - y * 16, 16);
	const int uv_w = (w + 1) >> 1;
	const int uv_h = (h + 1) >> 1;
	int off = (uv_w-1)%4;
    uint32_t* uvsrc = src + x_mb*8/4;//32bb
	uint32_t rem_dat;
	uint32_t crt_dat;
	uint32_t w32;
	int addr8 = 0;
	int rem_num = 0;

    for (int i = 0; i < 8; ++i) {
    	int addr32 = (addr8+0)/4;
    	int num_rem = 4-(addr8&3);
    	rem_dat = uvsrc[addr32++];
        for(int base=0;base<2;base++){
  #pragma HLS PIPELINE
    		int j=base*4;
    		bool isAllIn  = ((j+3)<=(uv_w-1));
    		bool isAllOut = (j>(uv_w-1));
    		bool isOver   = (num_rem+j)>= uv_w;
    		{//base = 0, 1
    			uint32_t rem_dat_2 = rem_dat;
        		if(isOver)
        			crt_dat = rem_dat;
        		else
        			crt_dat = rem_dat = uvsrc[addr32++];
        		w32 = get32bits_2_const(num_rem, rem_dat_2, crt_dat);
        		ap_uint<32> tmp = GetEdgeImage(w32, off, isAllIn, isAllOut );
        		VCT_GET(ap_uv_in_[(i&4)/2 + base],i%4, WD_PIX*4) =  tmp(31,0);
        	}
      }
      if(i<(uv_h-1))
    	  addr8 += stride;
    }
}

ap_uint<32> get32bits_2_const(ap_uint<3> n_rem, ap_uint<32> rem, ap_uint<32> crt)
{
#pragma HLS PIPELINE
	if(n_rem==4)
		return rem;
	if(n_rem==0)
		return crt;
	ap_uint<32> tmp;
	ap_uint<5> bits = 8*n_rem;
	rem = rem >> (32-bits);
	tmp = rem;
	crt = crt << bits;
	tmp(31, bits)=crt(31,bits);
	return tmp;

}
//////////======================================================================/////////////////////////////
//////////=====================   TopVp8_compute      ==========================/////////////////////////////
//////////======================================================================/////////////////////////////
//TopVp8_compute_NoOut===========================================================================/
//(Note, following names of functions may has already changed but not updated)
//-Intraprediction_mb_syn_str2
//--hls_LoadPre_out
//--hls_LoadPre_mode
//--Pickup_dataflow3
//---Pickup_Y44
//----hls_p4_test
//----hls_GetCost
//----hls_channel_p44
//-----hls_FTransform
//-----hls_QuantizeBlock
//-----hls_ITransformOne
//-----hls_SSE4X4
//-----hls_Disto4x4
//-----hls_fast_cost
//---Pickup_Y16
//----hls_channel_p16
//-----hls_p16_test
//-----hls_FTransform
//-----hls_FTransformWHT
//-----hls_QuantizeBlockWHT
//-----hls_IFTransformWHT
//-----hls_QuantizeBlock
//-----hls_ITransformOne
//-----hls_SSE4X4
//-----hls_Disto4x4
//-----hls_fast_cost
//-----hls_ca_score
//---Pickup_UV
//----hls_p8_test
//----hls_channel_uv_8
//-----hls_p8_test
//-----hls_FTransform
//-----hls_QuantizeBlock
//-----hls_ITransformOne
//-----hls_fast_cost
//-----hls_ca_score
//--hls_SetBestAs4_mode
void TopVp8_compute (
	ap_uint<LG2_MAX_NUM_MB_W> mb_w,
    ap_uint<LG2_MAX_NUM_MB_H> mb_h,
    hls::stream <ap_uint<WD_PIX*16> >  *str_din_y,
	hls::stream <ap_uint<WD_PIX*16> >  *str_din_uv,
    ap_uint<WD_LMD> lambda_p16,
    ap_uint<WD_LMD> lambda_p44,
    ap_uint<WD_LMD> tlambda,
    ap_uint<WD_LMD> lambda_uv,
    ap_uint<WD_LMD> tlambda_m,
    hls_QMatrix hls_qm1,
	hls_QMatrix hls_qm2,
	hls_QMatrix hls_qm_uv,
    ap_int<WD_sharpen*16>   ap_sharpen,
	ap_int<WD_sharpen*16>   ap_sharpen_uv,
	hls::stream< ap_uint<WD_PIX*16> >* str_out,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_dc,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_y,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_uv,
	hls::stream< ap_int<64> >* str_pred,
	hls::stream< ap_int<6> >* str_ret)
{
#pragma HLS interface ap_stable port=lambda_p16
#pragma HLS interface ap_stable port=lambda_p44
#pragma HLS interface ap_stable port=tlambda
#pragma HLS interface ap_stable port=lambda_uv
#pragma HLS interface ap_stable port=tlambda_m

    for( int y_mb = 0; y_mb < mb_h; y_mb++){
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
    	for( int x_mb = 0; x_mb < mb_w; x_mb++ ){
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
            //Intraprediction_mb_syn_str2(// &it,
            		Intraprediction_mb_syn_str2_widen(// &it,
            			x_mb,
    					y_mb,
    					mb_w,
            		    str_din_y,
						str_din_uv,
            		    lambda_p16,
            		    lambda_p44,
            		    tlambda,
            		    lambda_uv,
            		    tlambda_m,
            		    hls_qm1,
            			hls_qm2,
            			hls_qm_uv,
            		    ap_sharpen,
            			ap_sharpen_uv,
    					str_out,
    					str_level_dc,
    					str_level_y,
    					str_level_uv,
						str_pred,
						str_ret);
    	}
    }
}

void TopVp8_compute_NoOut (
	ap_uint<LG2_MAX_NUM_MB_W> mb_w,
    ap_uint<LG2_MAX_NUM_MB_H> mb_h,
    hls::stream <ap_uint<WD_PIX*16> >  *str_din_y,
	hls::stream <ap_uint<WD_PIX*16> >  *str_din_uv,
    ap_uint<WD_LMD> lambda_p16,
    ap_uint<WD_LMD> lambda_p44,
    ap_uint<WD_LMD> tlambda,
    ap_uint<WD_LMD> lambda_uv,
    ap_uint<WD_LMD> tlambda_m,
    hls_QMatrix hls_qm1,
	hls_QMatrix hls_qm2,
	hls_QMatrix hls_qm_uv,
    ap_int<WD_sharpen*16>   ap_sharpen,
	ap_int<WD_sharpen*16>   ap_sharpen_uv,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_dc,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_y,
	hls::stream< ap_int<WD_LEVEL*16> >* str_level_uv,
	hls::stream< ap_int<64> >* str_pred,
	hls::stream< ap_int<6> >* str_ret)
{
#pragma HLS interface ap_stable port=lambda_p16
#pragma HLS interface ap_stable port=lambda_p44
#pragma HLS interface ap_stable port=tlambda
#pragma HLS interface ap_stable port=lambda_uv
#pragma HLS interface ap_stable port=tlambda_m

    for( int y_mb = 0; y_mb < mb_h; y_mb++){
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
    	for( int x_mb = 0; x_mb < mb_w; x_mb++ ){
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
            //Intraprediction_mb_syn_str2(// &it,
            		Intraprediction_mb_syn_str2_widen_NoOut(// &it,
            			x_mb,
    					y_mb,
    					mb_w,
            		    str_din_y,
						str_din_uv,
            		    lambda_p16,
            		    lambda_p44,
            		    tlambda,
            		    lambda_uv,
            		    tlambda_m,
            		    hls_qm1,
            			hls_qm2,
            			hls_qm_uv,
            		    ap_sharpen,
            			ap_sharpen_uv,
    					str_level_dc,
    					str_level_y,
    					str_level_uv,
						str_pred,
						str_ret);
    	}
    }
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////


void Intraprediction_mb_syn_str2_widen (
    ap_uint<LG2_MAX_NUM_MB_W> x_mb,
    ap_uint<LG2_MAX_NUM_MB_W> y_mb,
	ap_uint<LG2_MAX_NUM_MB_W> mb_w,
    hls::stream <ap_uint<WD_PIX*16> >  *str_ap_yuv_in_y,
	hls::stream <ap_uint<WD_PIX*16> >  *str_ap_yuv_in_uv,
    ap_uint<WD_LMD> lambda_p16,
    ap_uint<WD_LMD> lambda_p44,
    ap_uint<WD_LMD> tlambda,
    ap_uint<WD_LMD> lambda_uv,
    ap_uint<WD_LMD> tlambda_m,
    hls_QMatrix hls_qm1,
	hls_QMatrix hls_qm2,
	hls_QMatrix hls_qm_uv,
    ap_int<WD_sharpen*16>   ap_sharpen,
	ap_int<WD_sharpen*16>   ap_sharpen_uv,
	hls::stream<ap_uint<WD_PIX * 16> >* str_out,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv,
	hls::stream< ap_int<64> >* str_pred,
	hls::stream< ap_int<6> >* str_ret
	)
{
	ap_uint<WD_PIX * 16> ap_y_in_y44[16+0];
	ap_uint<WD_PIX * 16> ap_y_in_y16[16+0];
	ap_uint<WD_PIX * 16> ap_uv_in_[8];
	for(int i=0;i<16;i++)
	{
#pragma HLS PIPELINE
		ap_y_in_y44[i] = str_ap_yuv_in_y->read();
		ap_y_in_y16[i] = ap_y_in_y44[i];
		if(i&1)
			ap_uv_in_[i/2] = str_ap_yuv_in_uv->read();
	}

    static ap_uint<WD_PIX*4>   static_ap_y_top_[MAX_NUM_MB_W*4];
#pragma HLS RESOURCE  variable=static_ap_y_top_ core=RAM_2P_LUTRAM
    static ap_uint<WD_PIX*4>   static_ap_uv_top_[MAX_NUM_MB_W*4];
#pragma HLS RESOURCE  variable=static_ap_uv_top_ core=RAM_2P_LUTRAM
    static ap_uint<WD_PIX*4>   static_ap_y_left_[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_ complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_uv_left_[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_left_ complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_y_top_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y_left_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y4_top_c[4];
#pragma HLS ARRAY_PARTITION variable=ap_y4_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y4_left_c[4];
#pragma HLS ARRAY_PARTITION variable=ap_y4_left_c complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_uv_top_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_uv_left_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_left_c complete dim=1
    //Variables for mode
    static ap_uint<WD_MODE>  static_ap_y_top_mode[MAX_NUM_MB_W*4];
//storage for past, updated when bottoms are available
    static ap_uint<WD_MODE>  static_ap_y_left_mode[4];//storage for past, updated when right are available
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_mode complete dim=1
    ap_uint<WD_MODE>         ap_y_top_c_mode[4];// at beginning, default is DC
//#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_mode complete dim=1
    ap_uint<WD_MODE>         ap_y_left_c_mode[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_mode complete dim=1
    ap_uint<WD_MODE>                 ap_y4_top_c_mode[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_top_c_mode complete dim=1
    ap_uint<WD_MODE>    ap_y16_mode_c;
    ap_uint<WD_MODE>    ap_uv_mode_c;
    ap_uint<WD_MODE>    ap_y_m_mode;

    //Variables for rd and nz
    ap_uint<25> 	ap_nz_all=0;
    str_rd          rd_y16_cb[2];
    str_rd          rd_uv_cb[2];
    //str_rd_i4       rd_y4_acc;
    ap_uint<25>             rd_y4_acc_nz=0;
    ap_uint<WD_RD_SCORE+4>  rd_y4_acc_score=-1;

	ap_uint<1> istop = (y_mb ==0);
	ap_uint<1> isleft = (x_mb ==0);
	ap_uint<1> isright = (x_mb ==mb_w-1);

	ap_int<WD_LEVEL*16>  ap_y_level_cb[2][17];
	ap_int<WD_LEVEL*16>  ap_y16dc_level_cb[2];
	ap_int<WD_LEVEL*16>  ap_y4_level_cb[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_level_cb complete dim=1
	ap_int<WD_LEVEL*16>  ap_uv_level_cb[2][16];
    //it_o->hls_LoadPre( ap_y_top_, ap_uv_top_, x_mb, y_mb, mb_w);
	ap_uint<WD_PIX*16>  ap_y_out_cb[2][17];
    ap_uint<WD_PIX*16>  ap_y4_out_cb[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_out_cb complete dim=1
	ap_uint<WD_PIX*16>  ap_uv_out_cb[2][17];
    ap_uint<WD_PIX*4>   ap_y4_topright_c;
    ap_uint<WD_PIX>     ap_y_m;
    ap_uint<WD_PIX>     ap_u_m;
    ap_uint<WD_PIX>     ap_v_m;

    hls_LoadPre_out_widen(
    			&ap_y_m,
    			&ap_u_m,
    			&ap_v_m,
				static_ap_y_top_c,//[4],
				ap_y4_top_c,//[4],
				static_ap_uv_top_c,//[4],
    			&ap_y4_topright_c,
				ap_y_left_c,//[4],
				ap_y4_left_c,//[4],
				ap_uv_left_c,//[4],
				static_ap_y_left_,//[4],
				static_ap_uv_left_,//[4],
				static_ap_y_top_,
				static_ap_uv_top_, x_mb, y_mb, mb_w);

    ap_uint<WD_PIX*4>                ap_y_top_c_y44[16];
#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_y44 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_top_c_y16[4];
#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_y16 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_left_c_y44[16];
#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_y44 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_left_c_y16[4];
#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_y16 complete dim=1

    for(int i=0;i<4;i++){
#pragma HLS UNROLL
        ap_y_top_c_y44[i] = static_ap_y_top_c[i];
        ap_y_top_c_y16[i] = static_ap_y_top_c[i];
        ap_y_left_c_y44[i<<2] = ap_y_left_c[i];
        ap_y_left_c_y16[i] = ap_y_left_c[i];;
    }
    hls_LoadPre_mode_widen(
    		static_ap_y_top_mode,
			static_ap_y_left_mode,
    		ap_y_top_c_mode,
			ap_y4_top_c_mode,
			ap_y_left_c_mode,
			&ap_y_m_mode,
    		x_mb, y_mb, mb_w);
    //it_m->hls_LoadPre( x_mb, y_mb, mb_w);
    //rd_y4_acc.init();
    int mode_p16;int mode_uv;
    //for 4x4
    int n_sb;
    int mode;
    ap_uint<1>           b_uv=0;
    ap_uint<2>           b_y=0;
    /*******************************/
    /*       Pickup Y44             */
    /*******************************/
	Pickup_dataflow3_widen(
    	// Parameters unParameters changed for one picture/segment
    	tlambda,//                  :ap_uint<WD_LMD>         I__
    	tlambda_m,//                 ap_uint<WD_LMD>         I__
    	lambda_p44,//                ap_uint<WD_LMD> 	     I__
    	lambda_p16,//                ap_uint<WD_LMD>         I__
    	lambda_uv,//                 ap_uint<WD_LMD>         I__
    	hls_qm1,//y44,y16            hls_QMatrix             I__
    	hls_qm2,//y16                hls_QMatrix             I__
    	hls_qm_uv,//                 hls_QMatrix             I__
    	ap_sharpen,//                ap_int<WD_sharpen*16>   I__
    	ap_sharpen_uv,//             ap_int<WD_sharpen*16>   I__
    	                             //Parameters changed for each MB
		ap_y_in_y44,//[16],//         ap_uint<WD_PIX*16>      I__
		ap_y_in_y16,//[16],//         ap_uint<WD_PIX*16>      I__
		ap_uv_in_,//[8]
        istop,//                     ap_uint<1> 		     I__
        isleft,//                    ap_uint<1> 		     I__
        isright,//                   ap_uint<1> 		     I__
    	// image context
		ap_y4_top_c,//ap_y_top_c_y44,//[16],//          ap_uint<WD_PIX*4>       I__
		ap_y_top_c_y16,//ap_y_top_c,//[4],//          ap_uint<WD_PIX*4>       I__
		ap_y4_left_c,//ap_y_left_c_y44,//[16],//         ap_uint<WD_PIX*4>       I__
		ap_y_left_c_y16,//ap_y_left_c,//[4],//         ap_uint<WD_PIX*4>       I__
		static_ap_uv_top_c,//[4],//         ap_uint<WD_PIX*4>       I__
    	ap_uv_left_c,//[4],//        ap_uint<WD_PIX*4>       I__
    	ap_y_m,//                    ap_uint<WD_PIX>         I__
    	ap_u_m,//                    ap_uint<WD_PIX>         I__
    	ap_v_m,//                    ap_uint<WD_PIX>         I__
    	ap_y4_topright_c,//          ap_uint<WD_PIX*4>       I__
    	// mode context
    	ap_y_top_c_mode,//[4],//     ap_uint<WD_MODE>        I__
    	ap_y_left_c_mode,//[4],//    ap_uint<WD_MODE>        I__
    	//OUTPUT
    	ap_y4_out_cb,//[16],//       ap_uint<WD_PIX*16>      O__
    	ap_y_out_cb,//[2][17],//     ap_uint<WD_PIX*16>      O__
    	ap_uv_out_cb,//[2][17],//    ap_uint<WD_PIX*16>      O__
    	ap_y4_level_cb,//[17],//     ap_int<WD_LEVEL*16>     O__
    	ap_y_level_cb,//[2][17],//   ap_int<WD_LEVEL*16>     O__
		ap_y16dc_level_cb,//[2] //   ap_int<WD_LEVEL*16>     O__
    	ap_uv_level_cb,//[2][16],//  ap_int<WD_LEVEL*16>     O__
    	//&rd_y4_acc,//                str_rd_i4*              OP_
	    &rd_y4_acc_score,
	    &rd_y4_acc_nz,
    	rd_y16_cb,//[2],//           str_rd                  O__
    	rd_uv_cb,//[2],//            str_rd                  O__
    	ap_y4_top_c_mode,//[16],//   ap_uint<WD_MODE>        IO_
    	&ap_y16_mode_c,//            ap_uint<WD_MODE>*       OP_
    	&ap_uv_mode_c,//             ap_uint<WD_MODE>*       OP_
    	&b_uv,//                     ap_uint<1>*             OP_
    	&b_y//                       ap_uint<2>*             OP_
    );
    ap_uint<6> ret;
    ap_uint<WD_MODE*16> ap_y_mode_b;
    //ret[5]==1;
    ret[5]  = ret[5]& (rd_uv_cb[b_uv].nz(23,16)==0);
   // ap_nz_all(23,16)    = rd_uv_cb[b_uv].nz(23,16);
/**********************************************/
/* Pickup the best mode for y
 * Set nz, level, out, preds, mb_->type       */
/**********************************************/
		if (rd_y4_acc_score < rd_y16_cb[(1&b_y)].score) {
	    	int x_sb_w = (int)x_mb<<2;
 	    	ret[5]  = ret[5]&(rd_y4_acc_nz(15,0)==0);
	    	hls_SetBestAs4_mode_widen(
	    			static_ap_y_top_mode,
					static_ap_y_left_mode,
					ap_y4_top_c_mode,
					ap_y_left_c_mode,
					&ap_y_mode_b,
					x_sb_w );
	    	b_y = 2;
	    	hls_StoreTopLeft_y(static_ap_y_top_, static_ap_y_left_, ap_y4_out_cb, x_mb);
    		for(int n=0;n<16;n++){
#pragma HLS UNROLL
    	        str_out->write(ap_y4_out_cb[n]);
    	        str_level_y->write( ap_y4_level_cb[n]);
    	    }
	    }else{
	    	int x_sb_w = (int)x_mb<<2;
 	    	ret[5] = ret[5]&(rd_y16_cb[(1&b_y)].nz(15,0)==0);
 	    	ret[5] = ret[5]&(rd_y16_cb[(1&b_y)].nz[24]==0);
	        hls_SetBestAs16_mode_widen(
	        		static_ap_y_top_mode,
					static_ap_y_left_mode,
					ap_y16_mode_c,
					&ap_y_mode_b,
					x_sb_w);
	        b_y &= 1;
	        hls_StoreTopLeft_y(static_ap_y_top_, static_ap_y_left_, ap_y_out_cb[b_y], x_mb);
    		for(int n=0;n<16;n++){
#pragma HLS UNROLL
    	        str_out->write(ap_y_out_cb[b_y][n]);
    	        str_level_y->write( ap_y_level_cb[b_y][n]);
    	    }
	    }
		hls_StoreTopLeft_uv(static_ap_uv_top_,  static_ap_uv_left_, ap_uv_out_cb[b_uv], x_mb);
		str_level_dc->write(ap_y16dc_level_cb[b_y&1]);//[16]);
		//str_level_dc->write(ap_y_level_cb[b_y&1][16]);
	    for(int n = 0; n < 8; n += 1){
#pragma HLS UNROLL
	    	str_out->write(ap_uv_out_cb[b_uv][n]);//b[n]);//,it.yuv_out_ + U_OFF_ENC+ VP8ScanUV[n],32);
	    	str_level_uv->write(ap_uv_level_cb[b_uv][n]);
	    }

    /**********************************************/
    /* write return value                         */
    /**********************************************/
    str_pred->write(ap_y_mode_b);
    ret(3,0) = ap_uv_mode_c(1,0);
    ret(4,4) = ~b_y(1,1);//it.mb_->type_ = 0;
    str_ret->write(ret);
}

void Intraprediction_mb_syn_str2_widen_NoOut (
    ap_uint<LG2_MAX_NUM_MB_W> x_mb,
    ap_uint<LG2_MAX_NUM_MB_W> y_mb,
	ap_uint<LG2_MAX_NUM_MB_W> mb_w,
    hls::stream <ap_uint<WD_PIX*16> >  *str_ap_yuv_in_y,
	hls::stream <ap_uint<WD_PIX*16> >  *str_ap_yuv_in_uv,
    ap_uint<WD_LMD> lambda_p16,
    ap_uint<WD_LMD> lambda_p44,
    ap_uint<WD_LMD> tlambda,
    ap_uint<WD_LMD> lambda_uv,
    ap_uint<WD_LMD> tlambda_m,
    hls_QMatrix hls_qm1,
	hls_QMatrix hls_qm2,
	hls_QMatrix hls_qm_uv,
    ap_int<WD_sharpen*16>   ap_sharpen,
	ap_int<WD_sharpen*16>   ap_sharpen_uv,
	//NoOut: hls::stream<ap_uint<WD_PIX * 16> >* str_out,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y,
	hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv,
	hls::stream< ap_int<64> >* str_pred,
	hls::stream< ap_int<6> >* str_ret
	)
{
	ap_uint<WD_PIX * 16> ap_y_in_y44[16+0];
	ap_uint<WD_PIX * 16> ap_y_in_y16[16+0];
	ap_uint<WD_PIX * 16> ap_uv_in_[8];
	for(int i=0;i<16;i++)
	{
#pragma HLS PIPELINE
		ap_y_in_y44[i] = str_ap_yuv_in_y->read();
		ap_y_in_y16[i] = ap_y_in_y44[i];
		if(i&1)
			ap_uv_in_[i/2] = str_ap_yuv_in_uv->read();
	}

    static ap_uint<WD_PIX*4>   static_ap_y_top_[MAX_NUM_MB_W*4];
#pragma HLS RESOURCE  variable=static_ap_y_top_ core=RAM_2P_LUTRAM
    static ap_uint<WD_PIX*4>   static_ap_uv_top_[MAX_NUM_MB_W*4];
#pragma HLS RESOURCE  variable=static_ap_uv_top_ core=RAM_2P_LUTRAM
    static ap_uint<WD_PIX*4>   static_ap_y_left_[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_ complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_uv_left_[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_left_ complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_y_top_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y_left_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y4_top_c[4];
#pragma HLS ARRAY_PARTITION variable=ap_y4_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_y4_left_c[4];
#pragma HLS ARRAY_PARTITION variable=ap_y4_left_c complete dim=1
    static ap_uint<WD_PIX*4>   static_ap_uv_top_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_top_c complete dim=1
     ap_uint<WD_PIX*4>         ap_uv_left_c[4];
//#pragma HLS ARRAY_PARTITION variable=ap_uv_left_c complete dim=1
    //Variables for mode
    static ap_uint<WD_MODE>  static_ap_y_top_mode[MAX_NUM_MB_W*4];
//storage for past, updated when bottoms are available
    static ap_uint<WD_MODE>  static_ap_y_left_mode[4];//storage for past, updated when right are available
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_mode complete dim=1
    ap_uint<WD_MODE>         ap_y_top_c_mode[4];// at beginning, default is DC
//#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_mode complete dim=1
    ap_uint<WD_MODE>         ap_y_left_c_mode[4];
//#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_mode complete dim=1
    ap_uint<WD_MODE>                 ap_y4_top_c_mode[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_top_c_mode complete dim=1
    ap_uint<WD_MODE>    ap_y16_mode_c;
    ap_uint<WD_MODE>    ap_uv_mode_c;
    ap_uint<WD_MODE>    ap_y_m_mode;

    //Variables for rd and nz
    ap_uint<25> 	ap_nz_all=0;
    str_rd          rd_y16_cb[2];
    str_rd          rd_uv_cb[2];
    //str_rd_i4       rd_y4_acc;
    ap_uint<25>             rd_y4_acc_nz=0;
    ap_uint<WD_RD_SCORE+4>  rd_y4_acc_score=-1;

	ap_uint<1> istop = (y_mb ==0);
	ap_uint<1> isleft = (x_mb ==0);
	ap_uint<1> isright = (x_mb ==mb_w-1);

	ap_int<WD_LEVEL*16>  ap_y_level_cb[2][17];
	ap_int<WD_LEVEL*16>  ap_y16dc_level_cb[2];
	ap_int<WD_LEVEL*16>  ap_y4_level_cb[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_level_cb complete dim=1
	ap_int<WD_LEVEL*16>  ap_uv_level_cb[2][16];
    //it_o->hls_LoadPre( ap_y_top_, ap_uv_top_, x_mb, y_mb, mb_w);
	ap_uint<WD_PIX*16>  ap_y_out_cb[2][17];
    ap_uint<WD_PIX*16>  ap_y4_out_cb[16];
#pragma HLS ARRAY_PARTITION variable=ap_y4_out_cb complete dim=1
	ap_uint<WD_PIX*16>  ap_uv_out_cb[2][17];
    ap_uint<WD_PIX*4>   ap_y4_topright_c;
    ap_uint<WD_PIX>     ap_y_m;
    ap_uint<WD_PIX>     ap_u_m;
    ap_uint<WD_PIX>     ap_v_m;

    hls_LoadPre_out_widen(
    			&ap_y_m,
    			&ap_u_m,
    			&ap_v_m,
				static_ap_y_top_c,//[4],
				ap_y4_top_c,//[4],
				static_ap_uv_top_c,//[4],
    			&ap_y4_topright_c,
				ap_y_left_c,//[4],
				ap_y4_left_c,//[4],
				ap_uv_left_c,//[4],
				static_ap_y_left_,//[4],
				static_ap_uv_left_,//[4],
				static_ap_y_top_,
				static_ap_uv_top_, x_mb, y_mb, mb_w);

    ap_uint<WD_PIX*4>                ap_y_top_c_y44[16];
#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_y44 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_top_c_y16[4];
#pragma HLS ARRAY_PARTITION variable=ap_y_top_c_y16 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_left_c_y44[16];
#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_y44 complete dim=1

    ap_uint<WD_PIX*4>                ap_y_left_c_y16[4];
#pragma HLS ARRAY_PARTITION variable=ap_y_left_c_y16 complete dim=1

    for(int i=0;i<4;i++){
#pragma HLS UNROLL
        ap_y_top_c_y44[i] = static_ap_y_top_c[i];
        ap_y_top_c_y16[i] = static_ap_y_top_c[i];
        ap_y_left_c_y44[i<<2] = ap_y_left_c[i];
        ap_y_left_c_y16[i] = ap_y_left_c[i];;
    }
    hls_LoadPre_mode_widen(
    		static_ap_y_top_mode,
			static_ap_y_left_mode,
    		ap_y_top_c_mode,
			ap_y4_top_c_mode,
			ap_y_left_c_mode,
			&ap_y_m_mode,
    		x_mb, y_mb, mb_w);
    //it_m->hls_LoadPre( x_mb, y_mb, mb_w);
    //rd_y4_acc.init();
    int mode_p16;int mode_uv;
    //for 4x4
    int n_sb;
    int mode;
    ap_uint<1>           b_uv=0;
    ap_uint<2>           b_y=0;
    /*******************************/
    /*       Pickup Y44             */
    /*******************************/
	Pickup_dataflow3_widen(
    	// Parameters unParameters changed for one picture/segment
    	tlambda,//                  :ap_uint<WD_LMD>         I__
    	tlambda_m,//                 ap_uint<WD_LMD>         I__
    	lambda_p44,//                ap_uint<WD_LMD> 	     I__
    	lambda_p16,//                ap_uint<WD_LMD>         I__
    	lambda_uv,//                 ap_uint<WD_LMD>         I__
    	hls_qm1,//y44,y16            hls_QMatrix             I__
    	hls_qm2,//y16                hls_QMatrix             I__
    	hls_qm_uv,//                 hls_QMatrix             I__
    	ap_sharpen,//                ap_int<WD_sharpen*16>   I__
    	ap_sharpen_uv,//             ap_int<WD_sharpen*16>   I__
    	                             //Parameters changed for each MB
		ap_y_in_y44,//[16],//         ap_uint<WD_PIX*16>      I__
		ap_y_in_y16,//[16],//         ap_uint<WD_PIX*16>      I__
		ap_uv_in_,//[8]
        istop,//                     ap_uint<1> 		     I__
        isleft,//                    ap_uint<1> 		     I__
        isright,//                   ap_uint<1> 		     I__
    	// image context
		ap_y4_top_c,//ap_y_top_c_y44,//[16],//          ap_uint<WD_PIX*4>       I__
		ap_y_top_c_y16,//ap_y_top_c,//[4],//          ap_uint<WD_PIX*4>       I__
		ap_y4_left_c,//ap_y_left_c_y44,//[16],//         ap_uint<WD_PIX*4>       I__
		ap_y_left_c_y16,//ap_y_left_c,//[4],//         ap_uint<WD_PIX*4>       I__
		static_ap_uv_top_c,//[4],//         ap_uint<WD_PIX*4>       I__
    	ap_uv_left_c,//[4],//        ap_uint<WD_PIX*4>       I__
    	ap_y_m,//                    ap_uint<WD_PIX>         I__
    	ap_u_m,//                    ap_uint<WD_PIX>         I__
    	ap_v_m,//                    ap_uint<WD_PIX>         I__
    	ap_y4_topright_c,//          ap_uint<WD_PIX*4>       I__
    	// mode context
    	ap_y_top_c_mode,//[4],//     ap_uint<WD_MODE>        I__
    	ap_y_left_c_mode,//[4],//    ap_uint<WD_MODE>        I__
    	//OUTPUT
    	ap_y4_out_cb,//[16],//       ap_uint<WD_PIX*16>      O__
    	ap_y_out_cb,//[2][17],//     ap_uint<WD_PIX*16>      O__
    	ap_uv_out_cb,//[2][17],//    ap_uint<WD_PIX*16>      O__
    	ap_y4_level_cb,//[17],//     ap_int<WD_LEVEL*16>     O__
    	ap_y_level_cb,//[2][17],//   ap_int<WD_LEVEL*16>     O__
		ap_y16dc_level_cb,//[2] //   ap_int<WD_LEVEL*16>     O__
    	ap_uv_level_cb,//[2][16],//  ap_int<WD_LEVEL*16>     O__
    	//&rd_y4_acc,//                str_rd_i4*              OP_
	    &rd_y4_acc_score,
	    &rd_y4_acc_nz,
    	rd_y16_cb,//[2],//           str_rd                  O__
    	rd_uv_cb,//[2],//            str_rd                  O__
    	ap_y4_top_c_mode,//[16],//   ap_uint<WD_MODE>        IO_
    	&ap_y16_mode_c,//            ap_uint<WD_MODE>*       OP_
    	&ap_uv_mode_c,//             ap_uint<WD_MODE>*       OP_
    	&b_uv,//                     ap_uint<1>*             OP_
    	&b_y//                       ap_uint<2>*             OP_
    );
    ap_uint<6> ret;
    ap_uint<WD_MODE*16> ap_y_mode_b;
    //ret[5]==1;
    ret[5]  = ret[5]& (rd_uv_cb[b_uv].nz(23,16)==0);
   // ap_nz_all(23,16)    = rd_uv_cb[b_uv].nz(23,16);
/**********************************************/
/* Pickup the best mode for y
 * Set nz, level, out, preds, mb_->type       */
/**********************************************/
		if (rd_y4_acc_score < rd_y16_cb[(1&b_y)].score) {
	    	int x_sb_w = (int)x_mb<<2;
 	    	ret[5]  = ret[5]&(rd_y4_acc_nz(15,0)==0);
	    	hls_SetBestAs4_mode_widen(
	    			static_ap_y_top_mode,
					static_ap_y_left_mode,
					ap_y4_top_c_mode,
					ap_y_left_c_mode,
					&ap_y_mode_b,
					x_sb_w );
	    	b_y = 2;
	    	hls_StoreTopLeft_y(static_ap_y_top_, static_ap_y_left_, ap_y4_out_cb, x_mb);
    		for(int n=0;n<16;n++){
#pragma HLS UNROLL
    	        //str_out->write(ap_y4_out_cb[n]);
    	        str_level_y->write( ap_y4_level_cb[n]);
    	    }
	    }else{
	    	int x_sb_w = (int)x_mb<<2;
 	    	ret[5] = ret[5]&(rd_y16_cb[(1&b_y)].nz(15,0)==0);
 	    	ret[5] = ret[5]&(rd_y16_cb[(1&b_y)].nz[24]==0);
	        hls_SetBestAs16_mode_widen(
	        		static_ap_y_top_mode,
					static_ap_y_left_mode,
					ap_y16_mode_c,
					&ap_y_mode_b,
					x_sb_w);
	        b_y &= 1;
	        hls_StoreTopLeft_y(static_ap_y_top_, static_ap_y_left_, ap_y_out_cb[b_y], x_mb);
    		for(int n=0;n<16;n++){
#pragma HLS UNROLL
    	        //str_out->write(ap_y_out_cb[b_y][n]);
    	        str_level_y->write( ap_y_level_cb[b_y][n]);
    	    }
	    }
		hls_StoreTopLeft_uv(static_ap_uv_top_,  static_ap_uv_left_, ap_uv_out_cb[b_uv], x_mb);
		str_level_dc->write(ap_y16dc_level_cb[b_y&1]);//[16]);
		//str_level_dc->write(ap_y_level_cb[b_y&1][16]);
	    for(int n = 0; n < 8; n += 1){
#pragma HLS UNROLL
	    	//str_out->write(ap_uv_out_cb[b_uv][n]);//b[n]);//,it.yuv_out_ + U_OFF_ENC+ VP8ScanUV[n],32);
	    	str_level_uv->write(ap_uv_level_cb[b_uv][n]);
	    }

    /**********************************************/
    /* write return value                         */
    /**********************************************/
    str_pred->write(ap_y_mode_b);
    ret(3,0) = ap_uv_mode_c(1,0);
    ret(4,4) = ~b_y(1,1);//it.mb_->type_ = 0;
    str_ret->write(ret);
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_StoreTopLeft_uv(
	    ap_uint<WD_PIX*4>   ap_uv_top_[MAX_NUM_MB_W*4],
	    ap_uint<WD_PIX*4>   ap_uv_left_[4],
		ap_uint<WD_PIX*16>   ap_uv_out_cb[8],
		ap_uint<LG2_MAX_NUM_MB_W> x_mb )
{
	for(int i=0;i<4;i++){
    		int x_sb_w = (int)x_mb<<2;
	#pragma HLS UNROLL
   			VCT_GET(ap_uv_left_[i],0,WD_PIX) = SB_GET(ap_uv_out_cb[i*2+1],0,3,WD_PIX);
            VCT_GET(ap_uv_left_[i],1,WD_PIX) = SB_GET(ap_uv_out_cb[i*2+1],1,3,WD_PIX);
            VCT_GET(ap_uv_left_[i],2,WD_PIX) = SB_GET(ap_uv_out_cb[i*2+1],2,3,WD_PIX);
            VCT_GET(ap_uv_left_[i],3,WD_PIX) = SB_GET(ap_uv_out_cb[i*2+1],3,3,WD_PIX);
            VCT_GET(ap_uv_top_[x_sb_w+0],i,WD_PIX) = SB_GET(ap_uv_out_cb[2],3,i,WD_PIX);
            VCT_GET(ap_uv_top_[x_sb_w+1],i,WD_PIX) = SB_GET(ap_uv_out_cb[3],3,i,WD_PIX);
            VCT_GET(ap_uv_top_[x_sb_w+2],i,WD_PIX) = SB_GET(ap_uv_out_cb[6],3,i,WD_PIX);
            VCT_GET(ap_uv_top_[x_sb_w+3],i,WD_PIX) = SB_GET(ap_uv_out_cb[7],3,i,WD_PIX);
	    }
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_StoreTopLeft_y(
	    ap_uint<WD_PIX*4>   ap_y_top_[MAX_NUM_MB_W*4],
		ap_uint<WD_PIX*4>   ap_y_left_[4],
		ap_uint<WD_PIX*16>   ap_y_out_cb[16],
		ap_uint<LG2_MAX_NUM_MB_W> x_mb )
{
	for(int i=0;i<4;i++){
			int x_sb_w = (int)x_mb<<2;
	#pragma HLS UNROLL
	        VCT_GET(ap_y_left_[i],0,WD_PIX) = SB_GET(ap_y_out_cb[i*4+3],0,3,WD_PIX);
	        VCT_GET(ap_y_left_[i],1,WD_PIX) = SB_GET(ap_y_out_cb[i*4+3],1,3,WD_PIX);
	        VCT_GET(ap_y_left_[i],2,WD_PIX) = SB_GET(ap_y_out_cb[i*4+3],2,3,WD_PIX);
	        VCT_GET(ap_y_left_[i],3,WD_PIX) = SB_GET(ap_y_out_cb[i*4+3],3,3,WD_PIX);

	        VCT_GET(ap_y_top_[x_sb_w+0],i,WD_PIX) = SB_GET(ap_y_out_cb[12],3,i,WD_PIX);
	        VCT_GET(ap_y_top_[x_sb_w+1],i,WD_PIX) = SB_GET(ap_y_out_cb[13],3,i,WD_PIX);
	        VCT_GET(ap_y_top_[x_sb_w+2],i,WD_PIX) = SB_GET(ap_y_out_cb[14],3,i,WD_PIX);
	        VCT_GET(ap_y_top_[x_sb_w+3],i,WD_PIX) = SB_GET(ap_y_out_cb[15],3,i,WD_PIX);


	    }
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

void hls_LoadPre_out_widen(
			ap_uint<WD_PIX>     *ap_y_m,
			ap_uint<WD_PIX>     *ap_u_m,
			ap_uint<WD_PIX>     *ap_v_m,
			ap_uint<WD_PIX*4>   ap_y_top_c[4],
			ap_uint<WD_PIX*4>   ap_y4_top_c[4],
			ap_uint<WD_PIX*4>   ap_uv_top_c[4],
			ap_uint<WD_PIX*4>   *ap_y4_topright_c,
			ap_uint<WD_PIX*4>   ap_y_left_c[4],
			ap_uint<WD_PIX*4>   ap_y4_left_c[4],
			ap_uint<WD_PIX*4>   ap_uv_left_c[4],
		    ap_uint<WD_PIX*4>   ap_y_left_[4],
		    ap_uint<WD_PIX*4>   ap_uv_left_[4],
			ap_uint<WD_PIX*4>   ap_y_top_[MAX_NUM_MB_W*4],
			ap_uint<WD_PIX*4>   ap_uv_top_[MAX_NUM_MB_W*4],
    		ap_uint<LG2_MAX_NUM_MB_W> x_mb,
    		ap_uint<LG2_MAX_NUM_MB_W> y_mb,
    		ap_uint<LG2_MAX_NUM_MB_W> mb_w){
        int x = (int)x_mb << 2;
        int y = (int)y_mb << 2;
    	ap_uint<1> istop 		= (y_mb ==0);
    	ap_uint<1> isleft 		= (x_mb ==0);

    	if(y>0){
            *ap_y_m = VCT_GET(ap_y_top_c[3],3,WD_PIX);
            *ap_u_m = VCT_GET(ap_uv_top_c[1],3,WD_PIX);
            *ap_v_m = VCT_GET(ap_uv_top_c[3],3,WD_PIX);
    	}else{
            *ap_y_m = 0x7f;
            *ap_u_m = 0x7f;
            *ap_v_m = 0x7f;
    	}

        if(y>0){
            ap_y_top_c[0]=ap_y4_top_c[0]=ap_y_top_[x+0];
            ap_y_top_c[1]=ap_y4_top_c[1]=ap_y_top_[x+1];
            ap_y_top_c[2]=ap_y4_top_c[2]=ap_y_top_[x+2];
            ap_y_top_c[3]=ap_y4_top_c[3]=ap_y_top_[x+3];
            if (x_mb < mb_w - 1)
            	(*ap_y4_topright_c) = ap_y_top_[x+4];
            else{
            	VCT_GET((*ap_y4_topright_c),0,WD_PIX) = VCT_GET(ap_y4_top_c[3],3,WD_PIX);
            	VCT_GET((*ap_y4_topright_c),1,WD_PIX) = VCT_GET(ap_y4_top_c[3],3,WD_PIX);
            	VCT_GET((*ap_y4_topright_c),2,WD_PIX) = VCT_GET(ap_y4_top_c[3],3,WD_PIX);
            	VCT_GET((*ap_y4_topright_c),3,WD_PIX) = VCT_GET(ap_y4_top_c[3],3,WD_PIX);
            }
        }else{
            ap_y_top_c[0]=ap_y4_top_c[0]=0x7f7f7f7f;
            ap_y_top_c[1]=ap_y4_top_c[1]=0x7f7f7f7f;
            ap_y_top_c[2]=ap_y4_top_c[2]=0x7f7f7f7f;
            ap_y_top_c[3]=ap_y4_top_c[3]=0x7f7f7f7f;
            (*ap_y4_topright_c) = 0x7f7f7f7f;
        }
        if(x>0){
            ap_y_left_c[0]=ap_y4_left_c[0]  =ap_y_left_[0];
            ap_y_left_c[1]=ap_y4_left_c[1]  =ap_y_left_[1];
            ap_y_left_c[2]=ap_y4_left_c[2]  =ap_y_left_[2];
            ap_y_left_c[3]=ap_y4_left_c[3] =ap_y_left_[3];
        }else{
            ap_y_left_c[0]=ap_y4_left_c[0] = 0x81818181;
            ap_y_left_c[1]=ap_y4_left_c[1] = 0x81818181;
            ap_y_left_c[2]=ap_y4_left_c[2] = 0x81818181;
            ap_y_left_c[3]=ap_y4_left_c[3] = 0x81818181;
        }
            ap_uv_top_c[0]	= ap_uv_top_[x+0];
            ap_uv_top_c[1]	= ap_uv_top_[x+1];
            ap_uv_top_c[2]	= ap_uv_top_[x+2];
            ap_uv_top_c[3]	= ap_uv_top_[x+3];
            ap_uv_left_c[0]	= ap_uv_left_[0];
            ap_uv_left_c[1]	= ap_uv_left_[1];
            ap_uv_left_c[2]	= ap_uv_left_[2];
            ap_uv_left_c[3]	= ap_uv_left_[3];
}

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_LoadPre_mode_widen(
		ap_uint<WD_MODE>    ap_y_top_mode[MAX_NUM_MB_W*4],
		ap_uint<WD_MODE>    ap_y_left_mode[4],
		ap_uint<WD_MODE>      ap_y_top_c_mode[4],
		ap_uint<WD_MODE>      ap_y4_top_c_mode[16],
		ap_uint<WD_MODE>      ap_y_left_c_mode[4],
		ap_uint<WD_MODE>      *ap_y_m_mode,
		ap_uint<LG2_MAX_NUM_MB_W> x_mb,
		ap_uint<LG2_MAX_NUM_MB_W> y_mb,
		ap_uint<LG2_MAX_NUM_MB_W> mb_w)
{

    int x = (int)x_mb << 2;
    int y = (int)y_mb << 2;
	if(y>0){
		*ap_y_m_mode = ap_y_top_c_mode[3];
	}else{
		*ap_y_m_mode = DC_PRED;//0
	}
    if(y>0){
    	ap_y_top_c_mode[0] = ap_y4_top_c_mode[0]  = ap_y_top_mode[x+0];
    	ap_y_top_c_mode[1] = ap_y4_top_c_mode[1]  = ap_y_top_mode[x+1];
    	ap_y_top_c_mode[2] = ap_y4_top_c_mode[2]  = ap_y_top_mode[x+2];
    	ap_y_top_c_mode[3] = ap_y4_top_c_mode[3]  = ap_y_top_mode[x+3];
    }else{
    	ap_y_top_c_mode[0] = ap_y4_top_c_mode[0] = DC_PRED;//0
    	ap_y_top_c_mode[1] = ap_y4_top_c_mode[1] = DC_PRED;//0
    	ap_y_top_c_mode[2] = ap_y4_top_c_mode[2] = DC_PRED;//0
    	ap_y_top_c_mode[3] = ap_y4_top_c_mode[3] = DC_PRED;//0
    }
    if(x>0){
    	ap_y_left_c_mode[0] = ap_y_left_mode[0];
    	ap_y_left_c_mode[1] = ap_y_left_mode[1];
    	ap_y_left_c_mode[2] = ap_y_left_mode[2];
    	ap_y_left_c_mode[3] = ap_y_left_mode[3];
    }else{
    	ap_y_left_c_mode[0] = DC_PRED;
    	ap_y_left_c_mode[1] = DC_PRED;//0
    	ap_y_left_c_mode[2] = DC_PRED;//0
    	ap_y_left_c_mode[3] = DC_PRED;//0
    }
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

ap_uint<12> hls_GetCost_widen(
		ap_uint<4> n_sb,
		ap_uint<4> mode,
	    ap_uint<WD_MODE>    ap_y_top_c_mode[4],// at beginning, default is DC
	    ap_uint<WD_MODE>    ap_y_left_c_mode[4],
	    ap_uint<WD_MODE>    local_mod)
{
#pragma HLS PIPELINE
    const int x_sb = (n_sb & 3), y_sb = n_sb >> 2;
	int left2= ap_y_left_c_mode[y_sb];
	int top2 = (y_sb == 0) ? (int)(ap_y_top_c_mode[x_sb]) : (int)(local_mod);
	return my_VP8FixedCostsI4[top2][left2][mode];
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void Pickup_dataflow3_widen(
	// Parameters unParameters changed for one picture/segment
	ap_uint<WD_LMD>         I__tlambda,//              :
	ap_uint<WD_LMD>         I__tlambda_m,//
	ap_uint<WD_LMD> 	    I__lambda_p44,//
	ap_uint<WD_LMD>         I__lambda_p16,//
	ap_uint<WD_LMD>         I__lambda_uv,//
	hls_QMatrix             I__hls_qm1,//y44,y16
	hls_QMatrix             I__hls_qm2,//y16
	hls_QMatrix             I__hls_qm_uv,//
	ap_int<WD_sharpen*16>   I__ap_sharpen,//
	ap_int<WD_sharpen*16>   I__ap_sharpen_uv,//
	//Parameters changed for each MB
	ap_uint<WD_PIX*16>      I__ap_yuv_in_y44[16],//
	ap_uint<WD_PIX*16>      I__ap_yuv_in_y16[16],//
	ap_uint<WD_PIX*16>      I__ap_uv_in_[8],//
    ap_uint<1> 		        I__istop,//
    ap_uint<1> 		        I__isleft,//
    ap_uint<1> 		        I__isright,//
	// image context
	ap_uint<WD_PIX*4>       I__ap_y_top_c_y44[4],//
	ap_uint<WD_PIX*4>       I__ap_y_top_c_y16[4],//
	ap_uint<WD_PIX*4>       I__ap_y_left_c_y44[4],//
	ap_uint<WD_PIX*4>       I__ap_y_left_c_y16[4],//
	ap_uint<WD_PIX*4>       I__ap_uv_top_c[4],//
	ap_uint<WD_PIX*4>       I__ap_uv_left_c[4],//
	ap_uint<WD_PIX>         I__ap_y_m,//
	ap_uint<WD_PIX>         I__ap_u_m,//
	ap_uint<WD_PIX>         I__ap_v_m,//
	ap_uint<WD_PIX*4>       I__ap_y4_topright_c,//
	// mode context
	ap_uint<WD_MODE>        I__ap_y_top_c_mode[4],//
	ap_uint<WD_MODE>        I__ap_y_left_c_mode[4],//
	//OUTPUT
	ap_uint<WD_PIX*16>      O__ap_y4_out_cb[16],//
	ap_uint<WD_PIX*16>      O__ap_y_out_cb[2][17],//
	ap_uint<WD_PIX*16>      O__ap_uv_out_cb[2][17],//
	ap_int<WD_LEVEL*16>     O__ap_y4_level_cb[17],//
	ap_int<WD_LEVEL*16>     O__ap_y_level_cb[2][17],//
	ap_int<WD_LEVEL*16>     O__ap_y16dc_level_cb[2],//
	ap_int<WD_LEVEL*16>     O__ap_uv_level_cb[2][16],//
	//str_rd_i4*              OP_rd_y4_acc,//
	ap_uint<WD_RD_SCORE+4>* O__score_acc,
	ap_uint<25>*            O__nz_mb,
	str_rd                  O__rd_y16_cb[2],//
	str_rd                  O__rd_uv_cb[2],//
	ap_uint<WD_MODE>        O_ap_y4_top_c_mode[16],//
	ap_uint<WD_MODE>*       OP_ap_y16_mode_c,//
	ap_uint<WD_MODE>*       OP_ap_uv_mode_c,//
	ap_uint<1>*             OP_b_uv,//
	ap_uint<2>*             OP_b_y//
)
{
#pragma HLS DATAFLOW
	Pickup_Y44_widen(
	//OP_rd_y4_acc->nz = Pickup_Y44_new(
        I__istop,
        I__isleft,
        I__ap_y_top_c_y44,//[4],
        I__ap_y_left_c_y44,//[4],
        I__ap_y_top_c_mode,//[4],// at beginning, default is DC
        I__ap_y_left_c_mode,//[4],
      //  IO_ap_y4_top_c_mode,//[16],
        I__ap_y4_topright_c,
        I__ap_y_m,
        I__ap_yuv_in_y44,//[16],
        I__hls_qm1,
        I__ap_sharpen,
        I__lambda_p44,
        I__tlambda,
        I__tlambda_m,
        //OUTPUT
        O__ap_y4_out_cb,
        O__ap_y4_level_cb,
		O__score_acc,
		O__nz_mb,
        O_ap_y4_top_c_mode
        );

    Pickup_Y16(
	        I__tlambda,//     = dqm->tlambda_;
	        I__tlambda_m,//   = dqm->lambda_mode_;
	        I__lambda_p16,//  = dqm->lambda_i16_;
	        I__hls_qm1,//
	        I__hls_qm2,//
	        I__ap_sharpen,
    		//Parameters changed for each MB
	        I__ap_yuv_in_y16,//[16],
	        I__istop,
	        I__isleft,
	        I__isright,
    		// image context
	        I__ap_y_top_c_y16,//[4],
	        I__ap_y_left_c_y16,//[4],
	        I__ap_y_m,
    		//OUTPUT
	        O__ap_y_out_cb,//[2][17],
	        O__ap_y_level_cb,//[2][17],
			O__ap_y16dc_level_cb,//[2],
	        O__rd_y16_cb,//[2],
    		OP_ap_y16_mode_c,//
    		OP_b_y//
        );

    Pickup_UV(
    		    // Parameters unParameters changed for one picture/segment
		        I__tlambda,//     = dqm->tlambda_;
		        I__tlambda_m,//   = dqm->lambda_mode_;
		        I__lambda_uv,//   = dqm->lambda_uv_;
		        I__hls_qm_uv,
		        I__ap_sharpen_uv,
    			// Parameters changed for each MB
		        I__ap_uv_in_,//[8],
		        I__istop,
		        I__isleft,
		        I__isright,
    			// image context
		        I__ap_uv_top_c,//[4],
		        I__ap_uv_left_c,//[4],
		        I__ap_u_m,
		        I__ap_v_m,
		        //OUTPUT
		        O__ap_uv_out_cb,//[2][17],
		        O__ap_uv_level_cb,//[2][16],
		        O__rd_uv_cb,//[2],
				OP_ap_uv_mode_c,
				OP_b_uv);
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<4> Map_k_2_n_sb_0(int k)
{
//0, 1, 2, 3, 4,
#pragma HLS INLINE
	ap_uint<4> ret;
	if(k<=3)
		return k;
	else if(k<=5)
		return k+2;
	else if(k<=7)
		return k+4;
	else
		return k+6;
}
ap_uint<4> Map_k_2_n_sb(int k, ap_uint<1> idx)
{
#pragma HLS INLINE
	ap_uint<4> ret;
	if(k<=1)
		return k;
	else if(k<=7)
		return Map_k_2_n_sb_0(k) + (ap_uint<4> )idx*2;
	else
		return Map_k_2_n_sb_0(k);
};
void Pickup_Y44_widen(
		ap_uint<1>              istop,
		ap_uint<1>              isleft,
		ap_uint<WD_PIX*4>       ap_y_top_c[4],
		ap_uint<WD_PIX*4>       ap_y_left_c[4],
		ap_uint<WD_MODE>        ap_y_top_c_mode[4],// at beginning, default is DC
		ap_uint<WD_MODE>        ap_y_left_c_mode[4],
		//ap_uint<WD_MODE>        ap_y4_top_c_mode[16],
		ap_uint<WD_PIX*4>       ap_y4_topright_c,
		ap_uint<WD_PIX*4>       ap_y_m,
		//ap_uint<4>              MACRO_n_sb,
		ap_uint<WD_PIX*16>      ap_yuv_in[16],
		hls_QMatrix             hls_qm1,
		ap_int<WD_sharpen*16>   ap_sharpen,
	    ap_uint<WD_LMD>         lambda_p44,
	    ap_uint<WD_LMD>         tlambda,
	    ap_uint<WD_LMD>         tlambda_m,
		ap_uint<WD_PIX*16>		ap_y4_out_mb[16],
		ap_int<WD_LEVEL*16>		ap_y4_level_mb[16],
		ap_uint<WD_RD_SCORE+4>  *score_acc,
		ap_uint<25>             *nz_mb,
		ap_uint<WD_MODE>		O__modes_mb[16])
{
//#pragma HLS INLINE
	/////////////////////////////////////////////
	// slow updated in loop without pipeline
    ap_uint<WD_MODE>                 local_mode_array[16];
    ap_uint<WD_PIX*4>                local_y4_top_c[16];
#pragma HLS ARRAY_PARTITION variable=local_y4_top_c complete dim=1
    ap_uint<WD_PIX*4>                local_left_c[16];
#pragma HLS ARRAY_PARTITION variable=local_left_c complete dim=1
    for(int i=0;i<4;i++)
    {
#pragma HLS UNROLL
    	local_y4_top_c[i] = ap_y_top_c[i];
    	local_left_c[i*4]  = ap_y_left_c[i];
    	//local_mode_array[i] = ap_y_top_c_mode[i];
    }
#pragma HLS ARRAY_PARTITION variable=local_mode_array complete dim=1
    //////////////////////////////////////////////
    //fast updated (read and wrote) in pipeline
    ap_uint<WD_RD_SCORE+4>           score_b_array[16];
#pragma HLS ARRAY_PARTITION variable=score_b_array complete dim=1
    /////////////////////////////////////////////
    //simple output, only be wrote in loop
    ap_uint<25> nz_b=0;
	ap_int<WD_LEVEL*16>		         local_level_array[16];
#pragma HLS ARRAY_PARTITION variable=local_level_array complete dim=1
    ap_uint<WD_LEVEL*16>             local_out_array[16];
#pragma HLS ARRAY_PARTITION variable=local_out_array complete dim=1
    for( int k_p44 =0; k_p44<10; k_p44++){
//#pragma HLS PIPELINE
		ap_uint<4>			    mode_b[2];
		ap_uint<4>				n_sb2[2];
#pragma HLS ARRAY_PARTITION variable=mode_b complete dim=1
#pragma HLS ARRAY_PARTITION variable=n_sb2 complete dim=1
		n_sb2[0] = Map_k_2_n_sb(k_p44,0);
		n_sb2[1] = Map_k_2_n_sb(k_p44,1);
		const int loop1 = (k_p44<2 || k_p44>13)?10:20;
		for(int fmod =0; fmod< loop1 ; fmod++ )
		{
	#pragma HLS PIPELINE
//#pragma HLS dependence array inter false
			int set = fmod>=10?1:0;
			ap_uint<4> n_sb = Map_k_2_n_sb(k_p44,set);
			ap_uint<4> MACRO_mode = fmod>=10? fmod-10:fmod;//fmod(4,1);
			ap_uint<WD_PIX*16>      ap_yuv_in_sb = ap_yuv_in[n_sb];
			ap_uint<WD_PIX*4>       abcd,efgh,ijkl;
			ap_uint<WD_PIX>         x44;
			hls_LoadPreds4_ins(
				local_y4_top_c,
				local_left_c,
				ap_y4_topright_c,
				ap_y_m ,
				&abcd, &efgh, &ijkl, &x44,
				isleft, istop, n_sb);

			 ap_uint<1> MACRO_isfirst=  (MACRO_mode==0);
			 ap_uint<WD_PIX*16>      ap_ref_p44;
			 ap_ref_p44  = hls_p4_test(abcd,efgh,ijkl,x44,MACRO_mode);
			 ap_uint<WD_MODE> mode_up;
			 //if(n_sb>3)
				// mode_up = local_mode_array[n_sb-4];
				ap_uint<12> pre_dis_h;
				pre_dis_h = hls_GetCost(n_sb, MACRO_mode,  ap_y_top_c_mode, ap_y_left_c_mode,  local_mode_array);

				ap_uint<WD_PIX*16>      ap_y4_out_tmp;
				ap_int<WD_LEVEL*16>     ap_y4_level_tmp;
				ap_uint<WD_RD_SCORE+4>  score_sb;
				ap_uint<25>             nz_sb;
				ap_uint<4>				mode_out;
				hls_channel_p44(
						MACRO_mode,
						ap_yuv_in_sb,
						ap_ref_p44,
						hls_qm1,
						ap_sharpen,
						lambda_p44,
						tlambda,
						tlambda_m,
						pre_dis_h,
						&ap_y4_out_tmp,
						&ap_y4_level_tmp,
						&score_sb,
						&nz_sb,
						&mode_out
				);
				//if (MACRO_isfirst || score_sb < score_b_tmp)
				if (MACRO_isfirst || score_sb < score_b_array[n_sb])
				{
					score_b_array[n_sb] = score_sb;
					local_out_array[n_sb] = ap_y4_out_tmp;
					local_level_array[n_sb] = ap_y4_level_tmp;
					nz_b[n_sb] = nz_sb;
					mode_b[set] = mode_out;
					//local_mode_array[n_sb] = mode_out;
				}
		}//for mode

		local_mode_array[n_sb2[0]] = mode_b[0];
		local_mode_array[n_sb2[1]] = (loop1==20)?mode_b[1]:mode_b[0];
		if(n_sb2[0]<12)
			local_y4_top_c[n_sb2[0]+4] = VCT_GET(local_out_array[n_sb2[0]],3,WD_PIX*4);
		if(n_sb2[1]<12)
			local_y4_top_c[n_sb2[1]+4] = VCT_GET(local_out_array[n_sb2[1]],3,WD_PIX*4);
		if((n_sb2[0]&3)!=3){//3,7,11,15 //VCT_SET_COL_SB(sb, col, wd, vect)
			VCT_GET(local_left_c[n_sb2[0]+1],0,WD_PIX) = VCT_GET(local_out_array[n_sb2[0]],3,WD_PIX);
			VCT_GET(local_left_c[n_sb2[0]+1],1,WD_PIX) = VCT_GET(local_out_array[n_sb2[0]],7,WD_PIX);
			VCT_GET(local_left_c[n_sb2[0]+1],2,WD_PIX) = VCT_GET(local_out_array[n_sb2[0]],11,WD_PIX);
			VCT_GET(local_left_c[n_sb2[0]+1],3,WD_PIX) = VCT_GET(local_out_array[n_sb2[0]],15,WD_PIX);
		}
		if((n_sb2[1]&3)!=3){//3,7,11,15 //VCT_SET_COL_SB(sb, col, wd, vect)
			VCT_GET(local_left_c[n_sb2[1]+1],0,WD_PIX) = VCT_GET(local_out_array[n_sb2[1]],3,WD_PIX);
			VCT_GET(local_left_c[n_sb2[1]+1],1,WD_PIX) = VCT_GET(local_out_array[n_sb2[1]],7,WD_PIX);
			VCT_GET(local_left_c[n_sb2[1]+1],2,WD_PIX) = VCT_GET(local_out_array[n_sb2[1]],11,WD_PIX);
			VCT_GET(local_left_c[n_sb2[1]+1],3,WD_PIX) = VCT_GET(local_out_array[n_sb2[1]],15,WD_PIX);
		}


    }// for n_sb;
    ap_uint<WD_RD_SCORE+4>  score_acc_tmp= 0;//(1<<WD_RD_SCORE+2);
    for(int i=0;i<16;i++)
    {
#pragma HLS UNROLL
    	O__modes_mb[i] = local_mode_array[i];
    	score_acc_tmp += score_b_array[i];
    	ap_y4_level_mb[i] = local_level_array[i];
    	ap_y4_out_mb[i] = local_out_array[i];
    }
    *nz_mb |= nz_b;
    *score_acc = score_acc_tmp;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<12> hls_GetCost(
		ap_uint<4> n_sb,
		ap_uint<4> mode,
	    ap_uint<WD_MODE>    ap_y_top_c_mode[4],// at beginning, default is DC
	    ap_uint<WD_MODE>    ap_y_left_c_mode[4],
	    ap_uint<WD_MODE>    ap_y4_top_c_mode[16])
{
#pragma HLS PIPELINE
    const int x_sb = (n_sb & 3), y_sb = n_sb >> 2;
	int left2= ap_y_left_c_mode[y_sb];
	int top2 = (y_sb == 0) ? (int)(ap_y_top_c_mode[x_sb]) : (int)(ap_y4_top_c_mode[n_sb - 4]);
	return my_VP8FixedCostsI4[top2][left2][mode];
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_LoadPreds4_ins(
			ap_uint<WD_PIX*4> ap_y4_top_c[16],
			ap_uint<WD_PIX*4> ap_y4_left_c[16],
			ap_uint<WD_PIX*4> ap_y4_topright_c,
			ap_uint<WD_PIX*4> ap_y_m,
            ap_uint<WD_PIX*4>*   abcd,
    	    ap_uint<WD_PIX*4>*   efgh,
    	    ap_uint<WD_PIX*4>*   ijkl,
            ap_uint<WD_PIX>*     x44,
            ap_uint<1>           isleft,
            ap_uint<1>           istop,
    	    ap_uint<4>           n_sb)
{

#pragma HLS PIPELINE
        	*abcd = ap_y4_top_c[n_sb];
        	if((n_sb&3)!=3)//3,7,11,15
        		*efgh = ap_y4_top_c[n_sb+1];
        	else
        	    *efgh = ap_y4_topright_c;
        	*ijkl = ap_y4_left_c[n_sb];
        	if(n_sb==0){
        	    if(!isleft)
        	        *x44 = ap_y_m;
        	    else
        	        if(!istop)
        	            *x44 = 0X81;
        	        else
        	            *x44 = 0X7f;
        	 }else if((n_sb&3)!=0)//!0,4,8,12
        	     *x44 = VCT_GET(ap_y4_top_c[n_sb-1],3, WD_PIX);
        	 else// 4,8,12
        	     *x44 = VCT_GET(ap_y4_left_c[n_sb-4],3, WD_PIX);
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_p4_test(	ap_uint<WD_PIX*4> abcd,
								ap_uint<WD_PIX*4> efgh,
								ap_uint<WD_PIX*4> ijkl,
								ap_uint<WD_PIX> 	x44,
								ap_uint<4> mode)
{
    switch(mode){
        case  B_DC_PRED : return hls_DC4( abcd, ijkl);
		case  B_VE_PRED : return hls_VE4( abcd, efgh, x44);
		case  B_HE_PRED : return hls_HE4( ijkl, x44);
        case  B_RD_PRED : return hls_RD4( abcd, ijkl, x44);
        case  B_LD_PRED : return hls_LD4( abcd, efgh );
        case  B_VR_PRED : return hls_VR4( abcd, ijkl, x44);
        case  B_VL_PRED : return hls_VL4( abcd, efgh );
        case  B_HU_PRED : return hls_HU4( ijkl );
        case  B_HD_PRED : return hls_HD4( abcd, ijkl, x44);//ref:544 vs lut:100 ,3.19+1.25 ,
        default : return hls_TM4( abcd, ijkl, x44);
    }//case
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_channel_p44(
		ap_uint<4>              mode_in,
		ap_uint<WD_PIX*16>      ap_yuv_in_sb,
		ap_uint<WD_PIX*16>      ap_ref_p44,
		hls_QMatrix             hls_qm1,
		ap_int<WD_sharpen*16>   ap_sharpen,
	    ap_uint<WD_LMD>         lambda_p44,
	    ap_uint<WD_LMD>         tlambda,
	    ap_uint<WD_LMD>         tlambda_m,
		ap_uint<12>		        pre_dis_h,
		ap_uint<WD_PIX*16>		*ap_y4_out_cb_n_sb2,
		ap_int<WD_LEVEL*16>		*ap_y4_level_cb_n_sb2,
		ap_uint<WD_RD_SCORE+4>  *score_sb,
		ap_uint<25>             *nz_sb,
		ap_uint<4>				*mode_out)
{
#pragma HLS INLINE
#pragma HLS PIPELINE
	ap_uint<WD_PIX*16>      ap_src_p44  = ap_yuv_in_sb;
    str_dis rd_dis4;
    rd_dis4.init();
    ap_uint<WD_DCT*16>      ap_dct_p44  = hls_FTransform(ap_src_p44,ap_ref_p44);
    ap_int<WD_IQT*16>       ap_iqt_p44;
    ap_uint<5>              ap_nz;
    ap_uint<WD_PIX*16>      ap_y4_out_tmp;
    ap_int<WD_LEVEL*16>     ap_y4_level_tmp;
    ap_nz = hls_QuantizeBlock(ap_dct_p44, &ap_y4_level_tmp,&ap_iqt_p44,
							&hls_qm1,ap_sharpen,0);
    rd_dis4.nz      = (ap_nz!=0);//<<n_sb;
    if(rd_dis4.nz)
        ap_nz -=(VCT_GET(ap_y4_level_tmp,0,WD_LEVEL)!=0);
    ap_y4_out_tmp   = hls_ITransformOne(ap_ref_p44,ap_iqt_p44);
    rd_dis4.d       = hls_SSE4X4(ap_src_p44, ap_y4_out_tmp);
    rd_dis4.sd      = hls_Disto4x4(ap_src_p44, ap_y4_out_tmp );
    rd_dis4.sd      = (rd_dis4.sd* tlambda+128)>>8;
    rd_dis4.h       = pre_dis_h;//hls_GetCost(n_sb, mode,  ap_y_top_c_mode, ap_y_left_c_mode,  ap_y4_top_c_mode);
    if ((mode_in > 0) && (ap_nz <= 3))
        rd_dis4.r   = 140;
    else
        rd_dis4.r   = 0;
    rd_dis4.r +=  hls_fast_cost(ap_y4_level_tmp,0);
    rd_dis4.ca_score(tlambda_m);
    //output
    *score_sb = rd_dis4.score;
    *nz_sb = rd_dis4.nz;
    *ap_y4_out_cb_n_sb2   = ap_y4_out_tmp;
    *ap_y4_level_cb_n_sb2 = ap_y4_level_tmp;
    *mode_out = mode_in;
}

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_TTR> hls_TTransform(ap_uint<WD_PIX*16> in ) {
#pragma HLS INLINE
#pragma HLS PIPELINE
  ap_uint<WD_TTR> sum(0);
  ap_int<WD_PIX+3> tmp[16];
  ap_uint<WD_TTW>  WeightY[16] = {38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2};
#pragma HLS ARRAY_PARTITION variable=WeightY complete dim=1
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_PIX+2> a0 = ((ap_int<WD_PIX+2>)VCT_GET(in,0+i*4,WD_PIX) + (ap_int<WD_PIX+2>)VCT_GET(in,2+i*4,WD_PIX));  // 10b
    ap_int<WD_PIX+2> a1 = ((ap_int<WD_PIX+2>)VCT_GET(in,1+i*4,WD_PIX) + (ap_int<WD_PIX+2>)VCT_GET(in,3+i*4,WD_PIX));
    ap_int<WD_PIX+2> a2 = ((ap_int<WD_PIX+2>)VCT_GET(in,1+i*4,WD_PIX) - (ap_int<WD_PIX+2>)VCT_GET(in,3+i*4,WD_PIX));
    ap_int<WD_PIX+2> a3 = ((ap_int<WD_PIX+2>)VCT_GET(in,0+i*4,WD_PIX) - (ap_int<WD_PIX+2>)VCT_GET(in,2+i*4,WD_PIX));
    tmp[0 + i * 4] = a0 + a1;   // 11b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_PIX+4> a0 = (tmp[0 + i] + tmp[8 + i]);  // 12b
    ap_int<WD_PIX+4> a1 = (tmp[4 + i] + tmp[12+ i]);
    ap_int<WD_PIX+4> a2 = (tmp[4 + i] - tmp[12+ i]);
    ap_int<WD_PIX+4> a3 = (tmp[0 + i] - tmp[8 + i]);
    ap_int<WD_PIX+5> b0 = a0 + a1;    // 13b
    ap_int<WD_PIX+5> b1 = a3 + a2;
    ap_int<WD_PIX+5> b2 = a3 - a2;
    ap_int<WD_PIX+5> b3 = a0 - a1;
    //sum += (ap_uint<WD_TTR>)WeightY[ 0 + i] * b0.abs();     //error: no member named 'abs' in 'ap_int<13>???
    //sum += (ap_uint<WD_TTR>)WeightY[ 4 + i] * b1.abs();
    //sum += (ap_uint<WD_TTR>)WeightY[ 8 + i] * b2.abs();
    //sum += (ap_uint<WD_TTR>)WeightY[12 + i] * b3.abs();
    if(b0<0)b0=-b0;
    if(b1<0)b1=-b1;
    if(b2<0)b2=-b2;
    if(b3<0)b3=-b3;
    sum += (ap_uint<WD_TTR>)WeightY[ 0 + i] * b0;//.abs();     //
    sum += (ap_uint<WD_TTR>)WeightY[ 4 + i] * b1;//.abs();
    sum += (ap_uint<WD_TTR>)WeightY[ 8 + i] * b2;//.abs();
    sum += (ap_uint<WD_TTR>)WeightY[12 + i] * b3;//.abs();
  }
    return sum;
}

/* FOR SD CACULATION */
ap_uint<WD_DISTO>  hls_Disto4x4(ap_uint<WD_PIX*16> a,ap_uint<WD_PIX*16> b) {
#pragma HLS INLINE
    ap_uint<WD_TTR>     sum1    = hls_TTransform(a);
    ap_uint<WD_TTR>     sum2    = hls_TTransform(b);
    ap_int<WD_TTR+2>    tmp     = (ap_int<WD_TTR+2>)sum1-sum2;
 //   ap_uint<WD_DISTO>   val     = (__abs(tmp))>>5;
    //ap_uint<WD_DISTO>   val     = (tmp.abs())>>5; //error: no member named 'abs' in 'ap_int<13>???
    if(tmp<0)tmp=-tmp;
    	ap_uint<WD_DISTO>   val     = (tmp)>>5;
    return val;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

/* FOR R CACULATION */
ap_uint<WD_LEVEL+4> hls_LV0(ap_uint <WD_LEVEL-1> lv) {
    if (lv==0) return 0;
    if (lv==1) return 760;
    if (lv==2) return 1500;
    if (lv==3) return 2000;
    if (lv==4) return 2300;
    if (lv==5) return 2500;
    ap_uint<WD_LEVEL+4> tmp=lv;
    tmp = ((ap_uint<WD_LEVEL+4>)lv)<<7;
    tmp += ((ap_uint<WD_LEVEL+4>)lv)<<6;
    tmp +=((ap_uint<WD_LEVEL+4>)lv)<<3;
    return 1500+tmp;
}
ap_uint<WD_LEVEL> hls_LV1(ap_uint <WD_LEVEL-1> lv) {
    if (lv==0) return 0;
    if (lv==1) return 1000;
    if (lv==2) return 1100;
    return 1200;
}

ap_uint<WD_LEVEL> hls_LV2(ap_uint <WD_LEVEL-1> lv) {
    if (lv==0) return 0;
    if (lv==1) return 1000;
    if (lv==2) return 1100;
    if (lv==3) return 1150;
    return 1180;
}
ap_uint<WD_LEVEL> hls_LVn(ap_uint <WD_LEVEL-1> lv) {
    if (lv==0) return 0;
    return  500;
}

ap_uint<WD_FAST> hls_fast_cost(ap_int<WD_LEVEL*16>  vlevel, ap_uint<2> type){
    ap_uint <WD_FAST*16>    r_fast;
    ap_uint <WD_LEVEL-1>    levels[16];
#pragma HLS INLINE
#pragma HLS pipeline
    for(int i=0;i<13;i++){
#pragma HLS unroll
        ap_int <WD_LEVEL>     alevel= (ap_int<WD_LEVEL>)VCT_GET(vlevel,(i+type[1]),WD_LEVEL);
        if(alevel<0)
            levels[i] = (0-alevel);
        else
            levels[i] = alevel;
    }
    ap_uint<7> offset;
    if(type==0)//NORMAL
        offset =0;
    else if(type==1)//DC
        offset = 96;
    else
        offset = 113;//AC
     ap_uint<WD_FAST> tmp  =  hls_LV0((levels[  0]));// +
      tmp  +=                  hls_LV1((levels[ 1]));// +
      tmp  +=                hls_LV2((levels[ 2]));// +
      tmp  +=              2*hls_LVn((levels[ 3]));// +
      tmp  +=              2*hls_LVn((levels[ 4]));// +
      tmp  +=              3*hls_LVn((levels[ 5]));// +
      tmp  +=              3*hls_LVn((levels[ 6]));// +
                    tmp += 4*hls_LVn((levels[ 7]));// +
                    tmp += 4*hls_LVn((levels[ 8]));// +
                    tmp += 5*hls_LVn((levels[ 9]));// +
                    tmp += 5*hls_LVn((levels[10]));// +
                    tmp += 5*hls_LVn((levels[11]));// +
    if(type[1]!=1)
                    tmp += 5*hls_LVn((levels[12]));// +
  //                  tmp += 5*hls_LVn((levels[13]));// +
  //                  tmp += 5*hls_LVn((levels[14]));// +
  //                  tmp += 5*hls_LVn((levels[15]));//;
    return tmp+offset;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

/* FOR S CACULATION */
ap_uint<WD_SSE4> hls_SSE4X4(ap_uint<WD_PIX*16> src, ap_uint<WD_PIX*16> rec )
{
#pragma HLS INLINE
    ap_uint<WD_SSE4> sse4=0;
#pragma HLS pipeline
    for(int i=0;i<16;i++){
#pragma HLS unroll
        ap_int<WD_PIX+1> sub = (ap_int<WD_PIX+1>)VCT_GET(src,i,WD_PIX) - VCT_GET(rec,i,WD_PIX);
        sse4+= sub * sub;
    }
    return sse4;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

/* NORMAL TRANSFORM */
ap_int<WD_DCT*16> hls_FTransform(ap_uint<WD_PIX*16> src_ap, ap_uint<WD_PIX*16> ref_ap) {
/*FF:1531; LUT:1749; DSP:33;4.89+0.62ns; 7 cycles */
    ap_int<WD_DCT*16> out_ap;
    ap_int<(14)*16> tmp_ap;
#pragma HLS INLINE
#pragma HLS PIPELINE
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_SUB> d0_ap = SB_GET(src_ap,i,0,WD_PIX) - SB_GET(ref_ap,i,0,WD_PIX);   //
    ap_int<WD_SUB> d1_ap = SB_GET(src_ap,i,1,WD_PIX) - SB_GET(ref_ap,i,1,WD_PIX);
    ap_int<WD_SUB> d2_ap = SB_GET(src_ap,i,2,WD_PIX) - SB_GET(ref_ap,i,2,WD_PIX);
    ap_int<WD_SUB> d3_ap = SB_GET(src_ap,i,3,WD_PIX) - SB_GET(ref_ap,i,3,WD_PIX);
    ap_int<WD_SUB+1> a0_ap = (d0_ap + d3_ap); // 10b                      [-510,510]
    ap_int<WD_SUB+1> a1_ap = (d1_ap + d2_ap);
    ap_int<WD_SUB+1> a2_ap = (d1_ap - d2_ap);
    ap_int<WD_SUB+1> a3_ap = (d0_ap - d3_ap);
    VCT_GET(tmp_ap, i*4+0, 14) = (a0_ap + a1_ap) * 8; // 14b   [-8160,8160]
    VCT_GET(tmp_ap, i*4+1, 14) = (a2_ap * 2217 + a3_ap * 5352 + 1812) >> 9;// [-7536,7542]
    VCT_GET(tmp_ap, i*4+2, 14) = (a0_ap - a1_ap) * 8;
    VCT_GET(tmp_ap, i*4+3, 14) = (a3_ap * 2217 - a2_ap * 5352 + 937) >> 9;
  }
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<15> a0_ap = (ap_int<14>)VCT_GET(tmp_ap,(0+i),14) + (ap_int<14>)VCT_GET(tmp_ap,(12+i),14);//(tmp_ap[0 + i] + tmp_ap[12 + i]);  // 15b
    ap_int<15> a1_ap = (ap_int<14>)VCT_GET(tmp_ap,(4+i),14) + (ap_int<14>)VCT_GET(tmp_ap, (8+i),14);//(tmp_ap[4 + i] + tmp_ap[8 + i]);
    ap_int<15> a2_ap = (ap_int<14>)VCT_GET(tmp_ap,(4+i),14) - (ap_int<14>)VCT_GET(tmp_ap, (8+i),14);//(tmp_ap[4 + i] - tmp_ap[8 + i]);
    ap_int<15> a3_ap = (ap_int<14>)VCT_GET(tmp_ap,(0+i),14) - (ap_int<14>)VCT_GET(tmp_ap,(12+i),14);//(tmp_ap[0 + i] - tmp_ap[12 + i]);
    VCT_GET(out_ap, 0 + i,  WD_DCT) = ( a0_ap + a1_ap + 7) >> 4;            // 12b
    VCT_GET(out_ap, 4 + i,  WD_DCT) = ((a2_ap * 2217 + a3_ap * 5352 + 12000) >> 16) + (a3_ap != 0);
    VCT_GET(out_ap, 8 + i,  WD_DCT) = ( a0_ap - a1_ap + 7) >> 4;
    VCT_GET(out_ap,12 + i,  WD_DCT) = ((a3_ap * 2217 - a2_ap * 5352 + 51000) >> 16);
    }
    return out_ap;
}

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////


ap_uint<5> hls_QuantizeBlock( ap_int<WD_DCT*16>     in,
                            ap_int<WD_LEVEL*16>     *out,
                            ap_int<WD_DCT*16>       *out2,
                            hls_QMatrix*            pQM ,// frequency boosters for slight sharpening
                            ap_uint<WD_sharpen*16>  sharpen_,
                            ap_uint<1>              is16)// frequency boosters for slight sharpening
{
#pragma HLS INLINE
    return hls_QuantizeBlock_old(in, out ,out2,
                                pQM->q_0,
                                pQM->q_n,
                                pQM->iq_0,
                                pQM->iq_n,
                                pQM->bias_0,
                                pQM->bias_n,
                                sharpen_,
                                is16);
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

/* QUANTITION FOR NORMAL AND DC */
ap_uint<5> hls_QuantizeBlock_old( ap_int<WD_DCT*16>       in,
                            ap_int<WD_LEVEL*16>        *out,
                            ap_int<WD_DCT*16>       *out2,
                            ap_uint<WD_q>           q_0,        // quantizer steps
                            ap_uint<WD_q>           q_n,
                            ap_uint<WD_iq>          iq_0,       // reciprocals, fixed point.
                            ap_uint<WD_iq>          iq_n,
                            ap_uint<WD_bias>        bias_0,     // rounding bias
                            ap_uint<WD_bias>        bias_n,
                            ap_uint<WD_sharpen*16>  sharpen_,
                            ap_uint<1>              is16) // frequency boosters for slight sharpening
{
#pragma HLS INLINE
#pragma HLS pipeline
    ap_uint<5> last = 0;
    for (int n = 0; n < 16; ++n) {
#pragma HLS unroll
        if(is16 && n==0){
            VCT_GET((*out2),0, WD_DCT)      = 0;
            VCT_GET((*out), 0, WD_LEVEL)    = 0;
            continue;
        }
        const ap_uint<4>        j       = ZIGZAG(n);
        const ap_int<WD_DCT>    coeffs  = (ap_int<WD_DCT>)VCT_GET(in,j,WD_DCT);
        const ap_uint<1>        sign    = coeffs[WD_DCT-1];
        const ap_uint<WD_DCT-1> coeff   = (sign==1 ? (ap_uint<WD_DCT-1>)(-coeffs) :(ap_uint<WD_DCT-1>)( coeffs))+ VCT_GET(sharpen_,j,WD_sharpen);
        const ap_uint<WD_q>     Q       = n==0?q_0:q_n;
        const ap_uint<WD_iq>    iQ      = n==0?iq_0:iq_n;
        const ap_uint<WD_bias>  B       = n==0?bias_0:bias_n;
        ap_uint<WD_MLEVEL>      level   = (ap_uint<WD_MLEVEL>)((coeff * iQ + B) >> 17);
        if (level > MY_MAX_LEVEL) level = MY_MAX_LEVEL;
        if (sign)               level   = -level;
        ap_int<WD_DCT>          rec     = level * Q;
        VCT_GET((*out2),j, WD_DCT)      = level * Q;
        VCT_GET((*out), n, WD_LEVEL)    = level;
        if (level) last += 1;
  }
  return (last);
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////


/*Invers  Transforms */
const ap_uint<17> kC1 = 20091 + (1 << 16);
const ap_uint<17> kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)
ap_uint<WD_PIX*16> hls_ITransformOne(ap_uint<WD_PIX*16> ap_ref,
                                    ap_int<WD_IQT*16> ap_in){
    ap_uint<WD_PIX*16>  ap_des;
    ap_int<WD_IQT+3>    ap_tmp[16];
#pragma HLS INLINE
#pragma HLS pipeline
    for (int i = 0; i < 4; ++i) {    // vertical pass
#pragma HLS unroll
        ap_int<WD_IQT+2> ap_a,ap_b,ap_c,ap_d;
        ap_a =  (ap_int<WD_IQT>)VCT_GET(ap_in, i+0, WD_IQT)+
                (ap_int<WD_IQT>)VCT_GET(ap_in, i+8, WD_IQT);
        ap_b =  (ap_int<WD_IQT>)VCT_GET(ap_in, i+0, WD_IQT)-
                (ap_int<WD_IQT>)VCT_GET(ap_in, i+8, WD_IQT);
        ap_c =  MUL((ap_int<WD_IQT>)VCT_GET(ap_in, i+ 4, WD_IQT),kC2) -
                MUL((ap_int<WD_IQT>)VCT_GET(ap_in, i+12, WD_IQT),kC1);
        ap_d =  MUL((ap_int<WD_IQT>)VCT_GET(ap_in, i+ 4, WD_IQT),kC1) +
                MUL((ap_int<WD_IQT>)VCT_GET(ap_in, i+12, WD_IQT),kC2);
        ap_tmp[i*4+0] = ap_a + ap_d;
        ap_tmp[i*4+1] = ap_b + ap_c;
        ap_tmp[i*4+2] = ap_b - ap_c;
        ap_tmp[i*4+3] = ap_a - ap_d;
    }
    for (int i = 0; i < 4; ++i) {    // horizontal pass
#pragma HLS unroll
        ap_int<WD_IQT+4> ap_dc,ap_a,ap_b,ap_c,ap_d;
        ap_int<WD_IQT+1> s0,s1,s2,s3;
        ap_int<WD_IQT+2> r0,r1,r2,r3;
        ap_dc   =       4 + ap_tmp[i+0];
        ap_a    =   ap_dc + ap_tmp[i+8];
        ap_b    =   ap_dc - ap_tmp[i+8];
        ap_c    =   MUL(ap_tmp[i+4], kC2) - MUL(ap_tmp[i+12], kC1);
        ap_d    =   MUL(ap_tmp[i+4], kC1) + MUL(ap_tmp[i+12], kC2);
        s0      =   (ap_a + ap_d)>>3;
        s1      =   (ap_b + ap_c)>>3;
        s2      =   (ap_b - ap_c)>>3;
        s3      =   (ap_a - ap_d)>>3;
        r0      =   (ap_uint<WD_IQT+2>)VCT_GET(ap_ref, 0+i*4, WD_PIX) + s0;
        r1      =   (ap_uint<WD_IQT+2>)VCT_GET(ap_ref, 1+i*4, WD_PIX) + s1;
        r2      =   (ap_uint<WD_IQT+2>)VCT_GET(ap_ref, 2+i*4, WD_PIX) + s2;
        r3      =   (ap_uint<WD_IQT+2>)VCT_GET(ap_ref, 3+i*4, WD_PIX) + s3;
        VCT_GET(ap_des, 0+i*4, WD_PIX) = (r0<0)? 0 : (r0>255)? 255: r0(WD_PIX-1,0);
        VCT_GET(ap_des, 1+i*4, WD_PIX) = (r1<0)? 0 : (r1>255)? 255: r1(WD_PIX-1,0);
        VCT_GET(ap_des, 2+i*4, WD_PIX) = (r2<0)? 0 : (r2>255)? 255: r2(WD_PIX-1,0);
        VCT_GET(ap_des, 3+i*4, WD_PIX) = (r3<0)? 0 : (r3>255)? 255: r3(WD_PIX-1,0);
    }
    return ap_des;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void Pickup_Y16(
		ap_uint<WD_LMD>         I__tlambda,//              :
		ap_uint<WD_LMD>         I__tlambda_m,//
		ap_uint<WD_LMD>         I__lambda_p16,//
		hls_QMatrix             I__hls_qm1,//y44,y16
		hls_QMatrix             I__hls_qm2,//y16
		ap_int<WD_sharpen*16>   I__ap_sharpen,//
		//Parameters changed for each MB
		ap_uint<WD_PIX*16>      I__ap_y_in_[16],//
	    ap_uint<1> 		        I__istop,//
	    ap_uint<1> 		        I__isleft,//
	    ap_uint<1> 		        I__isright,//
		// image context
		ap_uint<WD_PIX*4>       I__ap_y_top_c[4],//
		ap_uint<WD_PIX*4>       I__ap_y_left_c[4],//
		ap_uint<WD_PIX>         I__ap_y_m,//
		//OUTPUT
		ap_uint<WD_PIX*16>      O__ap_y_out_cb[2][17],//
		ap_int<WD_LEVEL*16>     O__ap_y_level_cb[2][17],//
		ap_int<WD_LEVEL*16>     O__ap_y16dc_level_cb[2],//
		str_rd                  O__rd_y16_cb[2],//
		ap_uint<WD_MODE>*       OP_ap_y16_mode_c,//
		ap_uint<2>*             OP_b_y//
    )
{
//Pickup Best Y16x16, less than 400 cycles
	*OP_b_y=0;
	for(int mode_p16=0; mode_p16< 4; mode_p16++){
	//#pragma HLS DATAFLOW
	#pragma HLS latency max=80
		int mode_uv = mode_p16;
	    ap_uint<25> nz_y16_tmp;
	    O__rd_y16_cb[1-(1&(*OP_b_y))].score = hls_channel_p16(
	    	mode_p16,
			I__istop,
			I__isleft,
			I__ap_y_top_c,
			I__ap_y_left_c,
			I__ap_y_m,
			I__ap_y_in_,
			I__hls_qm1,
			I__hls_qm2,
			I__ap_sharpen,
			I__tlambda,
			I__tlambda_m,
			//OUTPUT
			O__ap_y_level_cb[ 1-(1&*OP_b_y)],
			&O__ap_y16dc_level_cb[ 1-(1&*OP_b_y)],
			O__ap_y_out_cb  [ 1-(1&*OP_b_y)],
	    	&nz_y16_tmp);
	    O__rd_y16_cb[1-(1&(*OP_b_y))].nz = &nz_y16_tmp;
	    if ( mode_p16==0 || O__rd_y16_cb[1-(1&(*OP_b_y))].score < O__rd_y16_cb[(1&*OP_b_y)].score) {
	        *OP_ap_y16_mode_c = mode_p16;
	        *OP_b_y = 1 - (1&*OP_b_y);
	    }
	}
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

ap_uint<WD_RD_SCORE+4>  hls_channel_p16(
		ap_uint<4>              mode_p16,
		ap_uint<1>              istop,
		ap_uint<1>              isleft,
		ap_uint<WD_PIX*4>       ap_y_top_c[4],
		ap_uint<WD_PIX*4>       ap_y_left_c[4],
		ap_uint<WD_PIX>         ap_y_m,
		ap_uint<WD_PIX*16>      ap_yuv_in_[24],
		hls_QMatrix             hls_qm1,
		hls_QMatrix             hls_qm2,
		ap_int<WD_sharpen*16>   ap_sharpen,
	    ap_uint<WD_LMD>         tlambda,//     = dqm->tlambda_;
	    ap_uint<WD_LMD>         tlambda_m,//   = dqm->lambda_mode_;
		ap_int<WD_LEVEL*16>     ap_y16_level_c[17],
		ap_int<WD_LEVEL*16>*    ap_y16dc_level_c,
		ap_uint<WD_PIX*16>      ap_y16_out_c[17],
		ap_uint<25>*            nz
		)
{
#pragma HLS INLINE

    str_dis rd_dis16;
    rd_dis16.init();
    ap_uint<WD_PIX*16>  ap_ref_p16[16];
    ap_int<WD_IWHT*16>  ap_iwht_dc;

    ap_int<WD_DCT*16>  ap_dct_out[16];
    ap_int<WD_DCT*16>  ap_wht_in;
    ap_int<WD_WHT*16>  ap_wht_out;
    ap_int<WD_IQT*16>   ap_iqt_ac[16];
    ap_int<WD_WHT*16>   ap_iqt_dc;

    for (int n = 0; n < 16; n += 1) {
#pragma HLS PIPELINE
    	ap_ref_p16[n] = hls_p16_test( mode_p16, n, istop, isleft, ap_y_top_c, ap_y_left_c, ap_y_m );
        ap_dct_out[n] = hls_FTransform( ap_yuv_in_[n], ap_ref_p16[n]);
        ap_uint<5> score_nz = hls_QuantizeBlock( ap_dct_out[n],
                                                 &ap_y16_level_c[n],
                                                 &ap_iqt_ac[n],
                                                 &hls_qm1,ap_sharpen,1);
        rd_dis16.nz |= (score_nz!=0)<<n;
        VCT_GET(ap_wht_in,n,WD_DCT) = (ap_int<WD_DCT>)VCT_GET(ap_dct_out[n],0,WD_DCT);
    }//for n
    ap_wht_out = hls_FTransformWHT(ap_wht_in);
    ap_int<WD_LEVEL*16>  tmp_level;
    rd_dis16.nz(24,24) = hls_QuantizeBlockWHT(ap_wht_out, &tmp_level, &ap_iqt_dc, &hls_qm2);
    ap_y16_level_c[16] = tmp_level;
    ap_y16dc_level_c[0] = tmp_level;
    ap_iwht_dc = hls_ITransformWHT(ap_iqt_dc);
    for (int n = 0; n < 16; n +=1 ) {
#pragma HLS PIPELINE
        ap_int<WD_IQT*16> ap_dcac = ap_iqt_ac[n];
        VCT_GET(ap_dcac,0,WD_IQT) = (ap_int<WD_IWHT>)VCT_GET(ap_iwht_dc,n,WD_IWHT);
        ap_y16_out_c[n]       = hls_ITransformOne(ap_ref_p16[n],ap_dcac);
        rd_dis16.d  += hls_SSE4X4(ap_yuv_in_[n], ap_y16_out_c[n]);
        rd_dis16.sd += hls_Disto4x4(ap_yuv_in_[n], ap_y16_out_c[n] );
        rd_dis16.r  += hls_fast_cost(ap_y16_level_c[n],2);
    }

    rd_dis16.r += hls_fast_cost(ap_y16dc_level_c[16-16],1);
    rd_dis16.sd = (rd_dis16.sd *tlambda+128)>>8;
    const ap_uint<10> my_VP8FixedCostsI16[4] = { 663, 919, 872, 919 };
    rd_dis16.h  =  my_VP8FixedCostsI16[mode_p16];

    *nz = rd_dis16.nz;
    return hls_ca_score(tlambda_m,&rd_dis16,mode_p16);
}


//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
/*Invers  Transforms */
ap_int<WD_IWHT*16> hls_ITransformWHT(ap_int<WD_WHT*16> in) {
//FF:0, lut:1248; 4.12+0.62ns; Latency:1
  // input is 12b signed
#pragma HLS INLINE
#pragma HLS pipeline
  ap_int<WD_IWHT*16> out;
  ap_int<WD_WHT+2> tmp[16];
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_WHT+1> a0 = ((ap_int<WD_WHT>)VCT_GET(in,0+i,WD_WHT) + (ap_int<WD_WHT>)VCT_GET(in,12 + i,WD_WHT));  // 16b
    ap_int<WD_WHT+1> a1 = ((ap_int<WD_WHT>)VCT_GET(in,4+i,WD_WHT) + (ap_int<WD_WHT>)VCT_GET(in, 8 + i,WD_WHT));
    ap_int<WD_WHT+1> a2 = ((ap_int<WD_WHT>)VCT_GET(in,4+i,WD_WHT) - (ap_int<WD_WHT>)VCT_GET(in, 8 + i,WD_WHT));
    ap_int<WD_WHT+1> a3 = ((ap_int<WD_WHT>)VCT_GET(in,0+i,WD_WHT) - (ap_int<WD_WHT>)VCT_GET(in,12 + i,WD_WHT));
    tmp[0 + i] = a0 + a1;   // 17b
    tmp[8 + i] = a0 - a1;
    tmp[4 + i] = a3 + a2;
    tmp[12+ i] = a3 - a2;
  }
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_WHT+3> dc = (tmp[0 + i * 4]   + 3); // 18b
    ap_int<WD_WHT+3> a0 = (dc               + tmp[3 + i * 4]);  // 18b
    ap_int<WD_WHT+3> a1 = (tmp[1 + i * 4]   + tmp[2 + i * 4]);
    ap_int<WD_WHT+3> a2 = (tmp[1 + i * 4]   - tmp[2 + i * 4]);
    ap_int<WD_WHT+3> a3 = (dc               - tmp[3 + i * 4]);
    ap_int<WD_WHT+4> b0 = a0 + a1;    // 19b
    ap_int<WD_WHT+4> b1 = a3 + a2;
    ap_int<WD_WHT+4> b2 = a0 - a1;
    ap_int<WD_WHT+4> b3 = a3 - a2;
    VCT_GET(out,  0 + i*4, WD_IWHT) = b0 >> 3;     // 16b
    VCT_GET(out,  1 + i*4, WD_IWHT) = b1 >> 3;
    VCT_GET(out,  2 + i*4, WD_IWHT) = b2 >> 3;
    VCT_GET(out,  3 + i*4, WD_IWHT) = b3 >> 3;
  }
    return out;
}

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////


/* QUANTITION FOR DC */
ap_uint<1> hls_QuantizeBlockWHT_old( ap_int<WD_WHT*16>       in,
                            ap_int<WD_LEVEL*16>        *out,
                            ap_int<WD_WHT*16>       *out2,
                            ap_uint<WD_q>           q_0,        // quantizer steps
                            ap_uint<WD_q>           q_n,
                            ap_uint<WD_iq>          iq_0,       // reciprocals, fixed point.
                            ap_uint<WD_iq>          iq_n,
                            ap_uint<WD_bias>        bias_0,     // rounding bias
                            ap_uint<WD_bias>        bias_n) // frequency boosters for slight sharpening
{
    ap_uint<1> last = 0;
    ap_int<WD_LEVEL*16>       xout;
    ap_int<WD_WHT*16>       xout2;
#pragma HLS pipeline
    for (int n = 0; n < 16; ++n) {
#pragma HLS unroll
        const ap_uint<4>        j       = ZIGZAG(n);
        const ap_int<WD_WHT>    coeffs  = (ap_int<WD_WHT>)VCT_GET(in,j,WD_WHT);
        const ap_uint<1>        sign    = coeffs[WD_WHT-1];
        const ap_uint<WD_WHT-1> coeff   = (sign==1 ? (ap_uint<WD_WHT-1>)(-coeffs) :(ap_uint<WD_WHT-1>)( coeffs));
        const ap_uint<WD_q>     Q       = n==0?q_0:q_n;
        const ap_uint<WD_iq>    iQ      = n==0?iq_0:iq_n;
        const ap_uint<WD_bias>  B       = n==0?bias_0:bias_n;
        ap_uint<WD_MLEVEL>      level   = (ap_uint<WD_MLEVEL>)((coeff * iQ + B) >> 17);
        if (level > MY_MAX_LEVEL) level = MY_MAX_LEVEL;
        if (sign)               level   = -level;
        ap_int<WD_WHT>          rec     = level * Q;
        VCT_GET((*out2),j, WD_WHT)      = level * Q;//(ap_int<WD_WHT>)(level * Q);
        VCT_GET((xout2),j, WD_WHT)      = level * Q;//(ap_int<WD_WHT>)(level * Q);
        VCT_GET((xout), n, WD_LEVEL)    = level;//(ap_int<WD_LEVEL>)level;
        VCT_GET((*out), n, WD_LEVEL)    = level;//(ap_int<WD_LEVEL>)level;
        if (level) last = 1;
  }
  return last;
}
ap_uint<1> hls_QuantizeBlockWHT( ap_int<WD_WHT*16>  in,
                            ap_int<WD_LEVEL*16>     *out,
                            ap_int<WD_WHT*16>       *out2,
                            hls_QMatrix*            pQM) // frequency boosters for slight sharpening
{
    return hls_QuantizeBlockWHT_old(in,out,out2,
                                pQM->q_0,
                                pQM->q_n,
                                pQM->iq_0,
                                pQM->iq_n,
                                pQM->bias_0,
                                pQM->bias_n);
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

ap_uint<WD_PIX*16> hls_DC16_4_y( //ref: lut:56, 3.19+1.25
        ap_uint<WD_PIX*4> top0,
		ap_uint<WD_PIX*4> top1,
		ap_uint<WD_PIX*4> top2,
		ap_uint<WD_PIX*4> top3,
        ap_uint<WD_PIX*4> left0,
		ap_uint<WD_PIX*4> left1,
		ap_uint<WD_PIX*4> left2,
		ap_uint<WD_PIX*4> left3,
        ap_uint<1> istop,
        ap_uint<1> isleft
    )
{
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    ap_uint<WD_PIX+5> DC=0;
    ap_uint<WD_PIX>   tmp;

    if(istop == 0){
        DC +=(AP_TREEADD4_VCT(top0, WD_PIX));
        DC +=(AP_TREEADD4_VCT(top1, WD_PIX));
        DC +=(AP_TREEADD4_VCT(top2, WD_PIX));
        DC +=(AP_TREEADD4_VCT(top3, WD_PIX));
        if(isleft==0){
            DC += (AP_TREEADD4_VCT(left0, WD_PIX));
            DC += (AP_TREEADD4_VCT(left1, WD_PIX));
            DC += (AP_TREEADD4_VCT(left2, WD_PIX));
            DC += (AP_TREEADD4_VCT(left3, WD_PIX));
        }else
            DC += DC;
        DC = (DC + (8<<1))>>(4+1);
    }else if(isleft == 0) {
        DC += (AP_TREEADD4_VCT(left0, WD_PIX));
        DC += (AP_TREEADD4_VCT(left1, WD_PIX));
        DC += (AP_TREEADD4_VCT(left2, WD_PIX));
        DC += (AP_TREEADD4_VCT(left3, WD_PIX));
        DC += DC;
        DC = (DC + (8<<1))>>(4+1);
    }else
        DC = 0X80;
    tmp = DC(WD_PIX-1,0);
    SB_SET_LINE_VAL(sb,0,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,1,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,2,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,3,WD_PIX,tmp);
    return sb;
};

ap_uint<WD_PIX*16> hls_p16_test(
		ap_uint<2> mode,
		ap_uint<4> n,
		ap_uint<1> istop,
		ap_uint<1> isleft,
	    ap_uint<WD_PIX*4>   ap_y_top_c[4],
	    ap_uint<WD_PIX*4>   ap_y_left_c[4],
		ap_uint<WD_PIX>     ap_y_m)
{
#pragma HLS PIPELINE
	//ap_uint<WD_PIX*4>* top	= ap_y_top_c;
	//ap_uint<WD_PIX*4>* left	= ap_y_left_c;
	ap_uint<WD_PIX*4>  abcd	= ap_y_top_c[n&3];
	ap_uint<WD_PIX*4>  ijkl	= ap_y_left_c[n>>2];
	ap_uint<WD_PIX>    X44 	= ap_y_m;
    switch(mode){
        case  B_DC_PRED : return hls_DC16_4_y( 	ap_y_top_c[0],ap_y_top_c[1],ap_y_top_c[2],ap_y_top_c[3],
        										ap_y_left_c[0], ap_y_left_c[1], ap_y_left_c[2], ap_y_left_c[3],
												istop, isleft);
		case  B_TM_PRED : return hls_TM16_4( abcd, ijkl, X44,istop, isleft);
		case  B_VE_PRED : return hls_VE16_4( abcd, istop);
        default : return hls_HE16_4( ijkl,isleft);
    }//case
}

ap_uint<WD_RD_SCORE+4> hls_ca_score(ap_uint<WD_LMD> lmbda, str_dis* dis ,ap_uint<4>m)
{
#pragma HLS PIPELINE
    return  (((ap_uint<WD_RD_SCORE+4>)( dis->d + (ap_uint<WD_SSE4+4>)(dis->sd)))<<8)
            +((ap_uint<WD_RD_SCORE+4>) ( dis->r +dis->h))*lmbda;
};//ca_score

//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

void Pickup_UV(
		// Parameters unParameters changed for one picture/segment
			ap_uint<WD_LMD>         I__tlambda,//              :
			ap_uint<WD_LMD>         I__tlambda_m,//
			ap_uint<WD_LMD>         I__lambda_uv,//
			hls_QMatrix             I__hls_qm_uv,//
			ap_int<WD_sharpen*16>   I__ap_sharpen_uv,//
			//Parameters changed for each MB
			ap_uint<WD_PIX*16>      I__ap_uv_in_[8],//
		    ap_uint<1> 		        I__istop,//
		    ap_uint<1> 		        I__isleft,//
		    ap_uint<1> 		        I__isright,//
			// image context
			ap_uint<WD_PIX*4>       I__ap_uv_top_c[4],//
			ap_uint<WD_PIX*4>       I__ap_uv_left_c[4],//
			ap_uint<WD_PIX>         I__ap_u_m,//
			ap_uint<WD_PIX>         I__ap_v_m,//
			ap_uint<WD_PIX*16>      O__ap_uv_out_cb[2][17],//
			ap_int<WD_LEVEL*16>     O__ap_uv_level_cb[2][16],//
			str_rd                  O__rd_uv_cb[2],//
			ap_uint<WD_MODE>*       OP_ap_uv_mode_c,//
			ap_uint<1>*             OP_b_uv//
		)
{
//Pickup Best Y16x16, less than 400 cycles
	*OP_b_uv=0;
	for(int mode_uv=0; mode_uv< 4; mode_uv++){
	    //Pickup Best UV, less than 400 cycles
		ap_uint<25> nz_tmp;
		O__rd_uv_cb[1-*OP_b_uv].score = hls_channel_uv_8(
			mode_uv,
			I__istop,
			I__isleft,
			I__ap_uv_top_c,
			I__ap_uv_left_c,
			I__ap_u_m,
			I__ap_v_m,
			I__ap_uv_in_,
			I__hls_qm_uv,
			I__ap_sharpen_uv,
			I__lambda_uv,
			//OUTPUT
			O__ap_uv_level_cb[1-*OP_b_uv],
			O__ap_uv_out_cb[1-*OP_b_uv],
			&nz_tmp);
            O__rd_uv_cb[1-*OP_b_uv].nz = nz_tmp;
		if (mode_uv==0 || O__rd_uv_cb[1-*OP_b_uv].score < O__rd_uv_cb[*OP_b_uv].score)
		{
		   *OP_ap_uv_mode_c = mode_uv;
		   *OP_b_uv = 1 - *OP_b_uv;
		}

	}
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

ap_uint<WD_RD_SCORE+4>  hls_channel_uv_8(
				ap_uint<4>              mode_uv,
				ap_uint<1>              istop,
				ap_uint<1>              isleft,
				ap_uint<WD_PIX*4>       ap_uv_top_c[4],
				ap_uint<WD_PIX*4>       ap_uv_left_c[4],
				ap_uint<WD_PIX>         ap_u_m,
				ap_uint<WD_PIX>         ap_v_m,
				ap_uint<WD_PIX*16>      ap_uv_in_[8],
				hls_QMatrix             hls_qm_uv,
				ap_int<WD_sharpen*16>   ap_sharpen_uv,
				ap_uint<WD_LMD>         lambda_uv,//     = dqm->tlambda_;
				ap_int<WD_LEVEL*16>     ap_uv_level_c[8],
				ap_uint<WD_PIX*16>      ap_uv_out_c[8],
				ap_uint<25>*            nz)
{
#pragma HLS INLINE
			str_dis rd_dis;
			const ap_uint<10>  my_VP8FixedCostsUV[4] = { 302, 984, 439, 642 };
			ap_uint<WD_PIX*16> ap_ref_uv;
			ap_int<WD_IQT*16>  ap_iqt_uv;
			ap_int<WD_DCT*16>  ap_dct_out;
			ap_uint<5>         score_nz;
			rd_dis.init();
	        rd_dis.h   = my_VP8FixedCostsUV[mode_uv];
	        for (int n = 0; n < 8; n += 1) {
#pragma HLS PIPELINE
	            ap_ref_uv			= hls_p8_test( mode_uv, n , istop, isleft, ap_uv_top_c, ap_uv_left_c, ap_u_m,ap_v_m);
	            ap_dct_out  		= hls_FTransform(ap_uv_in_[n], ap_ref_uv);
	            score_nz 			= hls_QuantizeBlock(ap_dct_out,
	            	                    &ap_uv_level_c[n],&ap_iqt_uv,
	            						&hls_qm_uv,ap_sharpen_uv,0);
	            ap_uv_out_c[n]      = hls_ITransformOne(ap_ref_uv,ap_iqt_uv);
	            rd_dis.nz 	        |= (score_nz!=0)<<(n+16);
	            rd_dis.d 	        += hls_SSE4X4( ap_uv_in_[n], ap_uv_out_c[n] );
	            rd_dis.r 	        += hls_fast_cost(ap_uv_level_c[n],2);
	        }
	        *nz = rd_dis.nz;
	        return hls_ca_score(lambda_uv, &rd_dis,mode_uv);

}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
/*******************************************/
/* Prediction img Generation: Y44          */
/*******************************************/
/* 1 */
ap_uint<WD_PIX*16> hls_DC4( //ref:581  lut 56, 4.46+1.25 ,
        ap_uint<WD_PIX*4> abcd,
		ap_uint<WD_PIX*4> ijkl//,
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    ap_uint<WD_PIX> tmp = ap_uint<WD_PIX>((AP_TREEADD2( (AP_TREEADD4_VCT(abcd, WD_PIX)),(AP_TREEADD4_VCT(ijkl, WD_PIX)),(WD_PIX+2) )+4)>>3);
    SB_SET_LINE_VAL(sb,0,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,1,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,2,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,3,WD_PIX,tmp);
    return sb;
};

/* 2 */
ap_uint<WD_PIX*16> hls_VE4( //ref: lut:56, 3.19+1.25//lut 452vs997, 2.72+1.25ns,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> efgh,
        ap_uint<WD_PIX>     X44
        )
{    // vertical
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    const ap_uint<WD_PIX> val0(ap_uint<WD_PIX>(AP_AVG3(X44, A44, B44, WD_PIX)));
    const ap_uint<WD_PIX> val1(ap_uint<WD_PIX>(AP_AVG3(A44, B44, C44, WD_PIX)));
    const ap_uint<WD_PIX> val2(ap_uint<WD_PIX>(AP_AVG3(B44, C44, D44, WD_PIX)));
    const ap_uint<WD_PIX> val3(ap_uint<WD_PIX>(AP_AVG3(C44, D44, E44, WD_PIX)));

    SB_SET_COL_VAL(sb,0,WD_PIX,val0);
    SB_SET_COL_VAL(sb,1,WD_PIX,val1);
    SB_SET_COL_VAL(sb,2,WD_PIX,val2);
    SB_SET_COL_VAL(sb,3,WD_PIX,val3);
    return sb;
};

/* 3 */
ap_uint<WD_PIX*16> hls_HE4( //ref: lut:56, 3.19+1.25
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>     X44
        )
{
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    SB_SET_LINE_VAL(sb,0,WD_PIX,ap_uint<WD_PIX>(AP_AVG3(X44, I44, J44, WD_PIX)));
    SB_SET_LINE_VAL(sb,1,WD_PIX,ap_uint<WD_PIX>(AP_AVG3(I44, J44, K44, WD_PIX)));
    SB_SET_LINE_VAL(sb,2,WD_PIX,ap_uint<WD_PIX>(AP_AVG3(J44, K44, L44, WD_PIX)));
    SB_SET_LINE_VAL(sb,3,WD_PIX,ap_uint<WD_PIX>(AP_AVG3(K44, L44, L44, WD_PIX)));
    return sb;
};

/* 4 */
ap_uint<WD_PIX*16> hls_RD4( //ref: lut:98  3.19+1.25, ,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>     X44
        )
{
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    AP_DST(sb,0,3, WD_PIX)=AP_AVG3(J44, K44, L44, WD_PIX);
    AP_DST(sb,0, 2, WD_PIX)= AP_DST(sb,1, 3, WD_PIX) = AP_AVG3(I44, J44, K44, WD_PIX);
    AP_DST(sb,0, 1, WD_PIX)= AP_DST(sb,1, 2, WD_PIX) = AP_DST(sb,2, 3, WD_PIX) = AP_AVG3(X44, I44, J44, WD_PIX);
    AP_DST(sb,0, 0, WD_PIX)= AP_DST(sb,1, 1, WD_PIX) = AP_DST(sb,2, 2, WD_PIX) = AP_DST(sb,3, 3, WD_PIX) = AP_AVG3(A44, X44, I44, WD_PIX);
    AP_DST(sb,1, 0, WD_PIX)= AP_DST(sb,2, 1, WD_PIX) = AP_DST(sb,3, 2, WD_PIX) = AP_AVG3(B44, A44, X44, WD_PIX);
    AP_DST(sb,2, 0, WD_PIX)= AP_DST(sb,3, 1, WD_PIX) = AP_AVG3(C44, B44, A44, WD_PIX);
    AP_DST(sb,3, 0, WD_PIX)= AP_AVG3(D44, C44, B44, WD_PIX);
    return sb;
};

/* 5 */
ap_uint<WD_PIX*16> hls_LD4( //ref: lut:98  3.19+1.25  , ,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> efgh
        )
{    //
#pragma HLS PIPELINE

    ap_uint<WD_PIX*16> sb;
    AP_DST(sb, 0, 0, WD_PIX)= AP_AVG3(A44, B44, C44, WD_PIX);
    AP_DST(sb, 1, 0, WD_PIX)= AP_DST(sb, 0, 1, WD_PIX) = AP_AVG3(B44, C44, D44, WD_PIX);
    AP_DST(sb, 2, 0, WD_PIX)= AP_DST(sb, 1, 1, WD_PIX) = AP_DST(sb, 0, 2, WD_PIX) = AP_AVG3(C44, D44, E44, WD_PIX);
    AP_DST(sb, 3, 0, WD_PIX)= AP_DST(sb, 2, 1, WD_PIX) = AP_DST(sb, 1, 2, WD_PIX) = AP_DST(sb, 0, 3, WD_PIX) = AP_AVG3(D44, E44, F44, WD_PIX);
    AP_DST(sb, 3, 1, WD_PIX)= AP_DST(sb, 2, 2, WD_PIX) = AP_DST(sb, 1, 3, WD_PIX) = AP_AVG3(E44, F44, G44, WD_PIX);
    AP_DST(sb, 3, 2, WD_PIX)= AP_DST(sb, 2, 3, WD_PIX) = AP_AVG3(F44, G44, H44, WD_PIX);
    AP_DST(sb, 3, 3, WD_PIX)= AP_AVG3(G44, H44, H44, WD_PIX);
    return sb;
};

/* 6 */
ap_uint<WD_PIX*16> hls_VR4( //ref: lut: 100  3.19+1.25 , ,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>     X44
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    AP_DST(sb, 0, 0, WD_PIX)= AP_DST(sb, 1, 2, WD_PIX) = AP_AVG2(X44, A44, WD_PIX);
    AP_DST(sb, 1, 0, WD_PIX)= AP_DST(sb, 2, 2, WD_PIX) = AP_AVG2(A44, B44, WD_PIX);
    AP_DST(sb, 2, 0, WD_PIX)= AP_DST(sb, 3, 2, WD_PIX) = AP_AVG2(B44, C44, WD_PIX);
    AP_DST(sb, 3, 0, WD_PIX)= AP_AVG2(C44, D44, WD_PIX);

    AP_DST(sb, 0, 3, WD_PIX)= AP_AVG3(K44, J44, I44, WD_PIX);
    AP_DST(sb, 0, 2, WD_PIX)= AP_AVG3(J44, I44, X44, WD_PIX);
    AP_DST(sb, 0, 1, WD_PIX)= AP_DST(sb, 1, 3, WD_PIX) = AP_AVG3(I44, X44, A44, WD_PIX);
    AP_DST(sb, 1, 1, WD_PIX)= AP_DST(sb, 2, 3, WD_PIX) = AP_AVG3(X44, A44, B44, WD_PIX);
    AP_DST(sb, 2, 1, WD_PIX)= AP_DST(sb, 3, 3, WD_PIX) = AP_AVG3(A44, B44, C44, WD_PIX);
    AP_DST(sb, 3, 1, WD_PIX)= AP_AVG3(B44, C44, D44, WD_PIX);
    return sb;
};

/* 7 */
ap_uint<WD_PIX*16> hls_VL4( //ref: lut: 100 3.19+1.25 , ,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> efgh
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    AP_DST(sb, 0, 0, WD_PIX)= AP_AVG2(A44, B44, WD_PIX);
    AP_DST(sb, 1, 0, WD_PIX)= AP_DST(sb, 0, 2, WD_PIX)= AP_AVG2(B44, C44, WD_PIX);
    AP_DST(sb, 2, 0, WD_PIX)= AP_DST(sb, 1, 2, WD_PIX)= AP_AVG2(C44, D44, WD_PIX);
    AP_DST(sb, 3, 0, WD_PIX)= AP_DST(sb, 2, 2, WD_PIX)= AP_AVG2(D44, E44, WD_PIX);

    AP_DST(sb, 0, 1, WD_PIX)= AP_AVG3(A44, B44, C44, WD_PIX);
    AP_DST(sb, 1, 1, WD_PIX)= AP_DST(sb, 0, 3, WD_PIX)= AP_AVG3(B44, C44, D44, WD_PIX);
    AP_DST(sb, 2, 1, WD_PIX)= AP_DST(sb, 1, 3, WD_PIX)= AP_AVG3(C44, D44, E44, WD_PIX);
    AP_DST(sb, 3, 1, WD_PIX)= AP_DST(sb, 2, 3, WD_PIX)= AP_AVG3(D44, E44, F44, WD_PIX);
    AP_DST(sb, 3, 2, WD_PIX)= AP_AVG3(E44, F44, G44, WD_PIX);
    AP_DST(sb, 3, 3, WD_PIX)= AP_AVG3(F44, G44, H44, WD_PIX);
    return sb;
};

/* 8 */
ap_uint<WD_PIX*16> hls_HU4( //ref: lut 54 3.19+1.25 , ,
        ap_uint<WD_PIX*4> ijkl
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    AP_DST(sb, 0, 0, WD_PIX)= AP_AVG2(I44, J44, WD_PIX);
    AP_DST(sb, 2, 0, WD_PIX)= AP_DST(sb, 0, 1, WD_PIX)= AP_AVG2(J44, K44, WD_PIX);
    AP_DST(sb, 2, 1, WD_PIX)= AP_DST(sb, 0, 2, WD_PIX)= AP_AVG2(K44, L44, WD_PIX);
    AP_DST(sb, 1, 0, WD_PIX)= AP_AVG3(I44, J44, K44, WD_PIX);
    AP_DST(sb, 3, 0, WD_PIX)= AP_DST(sb, 1, 1, WD_PIX)= AP_AVG3(J44, K44, L44, WD_PIX);
    AP_DST(sb, 3, 1, WD_PIX)= AP_DST(sb, 1, 2, WD_PIX)= AP_AVG3(K44, L44, L44, WD_PIX);
    AP_DST(sb, 3, 2, WD_PIX)= AP_DST(sb, 2, 2, WD_PIX)=
    AP_DST(sb, 0, 3, WD_PIX)= AP_DST(sb, 1, 3, WD_PIX)= AP_DST(sb, 2, 3, WD_PIX)= AP_DST(sb, 3, 3, WD_PIX)= L44;
    return sb;
};

/* 9 */
ap_uint<WD_PIX*16> hls_HD4( //ref:544 vs lut:100 ,3.19+1.25 ,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>     X44
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    AP_DST(sb, 0, 0, WD_PIX)= AP_DST(sb, 2, 1, WD_PIX)= AP_AVG2(I44, X44, WD_PIX);
    AP_DST(sb, 0, 1, WD_PIX)= AP_DST(sb, 2, 2, WD_PIX)= AP_AVG2(J44, I44, WD_PIX);
    AP_DST(sb, 0, 2, WD_PIX)= AP_DST(sb, 2, 3, WD_PIX)= AP_AVG2(K44, J44, WD_PIX);
    AP_DST(sb, 0, 3, WD_PIX)= AP_AVG2(L44, K44, WD_PIX);

    AP_DST(sb, 3, 0, WD_PIX)= AP_AVG3(A44, B44, C44, WD_PIX);
    AP_DST(sb, 2, 0, WD_PIX)= AP_AVG3(X44, A44, B44, WD_PIX);
    AP_DST(sb, 1, 0, WD_PIX)= AP_DST(sb, 3, 1, WD_PIX)= AP_AVG3(I44, X44, A44, WD_PIX);
    AP_DST(sb, 1, 1, WD_PIX)= AP_DST(sb, 3, 2, WD_PIX)= AP_AVG3(J44, I44, X44, WD_PIX);
    AP_DST(sb, 1, 2, WD_PIX)= AP_DST(sb, 3, 3, WD_PIX)= AP_AVG3(K44, J44, I44, WD_PIX);
    AP_DST(sb, 1, 3, WD_PIX)= AP_AVG3(L44, K44, J44, WD_PIX);

    return sb;
};

/* 10 */
ap_uint<WD_PIX*16> hls_TM4( //ref: lut 516, 4.07+1.25,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>     X44
        )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    ap_int<WD_PIX+2> tmp;
    for(int i=0;i<4;i++)
#pragma HLS unroll
        for(int j=0; j<4; j++){
#pragma HLS unroll
            tmp = AP_TREEADD2(  (VCT_GET(abcd,j,WD_PIX)),  (VCT_GET(ijkl,i,WD_PIX)),WD_PIX  )- X44;
            if(tmp > 255)
                tmp = 255;
            else if(tmp < 0)
                tmp = 0 ;
            SB_GET(sb, i, j, WD_PIX)=tmp;
        }
    return sb;
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_p8_test(
		ap_uint<2> mode,
		ap_uint<3> n,
		ap_uint<1> istop,
		ap_uint<1> isleft,
	    ap_uint<WD_PIX*4>   ap_uv_top_c[4],
	    ap_uint<WD_PIX*4>   ap_uv_left_c[4],
		ap_uint<WD_PIX>     ap_u_m,
		ap_uint<WD_PIX>     ap_v_m)
{
    ap_uint<WD_PIX*4>  top[2] ;
    ap_uint<WD_PIX*4>  left[2];
    ap_uint<WD_PIX*4>   abcd;
    ap_uint<WD_PIX*4>   ijkl;
    ap_uint<WD_PIX>		X44;
	if(n<4){
        top[0]  = ap_uv_top_c[0];
        top[1]  = ap_uv_top_c[1];
        left[0] = ap_uv_left_c[0];
        left[1] = ap_uv_left_c[1];
        abcd = ap_uv_top_c[n&1];
        ijkl = ap_uv_left_c[n>>1];
        X44 = ap_u_m;
	}else{ n-=4;
    	top[0]  = ap_uv_top_c[2];
    	top[1]  = ap_uv_top_c[3];
    	left[0] = ap_uv_left_c[2];
    	left[1] = ap_uv_left_c[3];
        abcd = ap_uv_top_c[2+(n&1)];
        ijkl = ap_uv_left_c[2+(n>>1)];
        X44 = ap_v_m;
    }
    switch(mode){
        case  B_DC_PRED : return hls_DC16_4_uv_old(top,left,istop,isleft);
		case  B_TM_PRED : return hls_TM16_4(abcd,ijkl, X44,istop,isleft);
		case  B_VE_PRED : return hls_VE16_4( abcd,istop);
        default : return hls_HE16_4(ijkl,isleft);
    }//case
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_DC16_4_uv_old( //ref: lut:56, 3.19+1.25
        ap_uint<WD_PIX*4> top[2],
        ap_uint<WD_PIX*4> left[2],
        ap_uint<1> istop,
        ap_uint<1> isleft
    )
{
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    ap_uint<WD_PIX+5> DC=0;
    ap_uint<WD_PIX>   tmp;

    if(istop == 0){
        for (int j = 0; j < (2<<0) ; ++j)
            DC +=(AP_TREEADD4_VCT(top[j], WD_PIX));
        if(isleft==0)
            for (int j = 0; j < (2<<0) ;  ++j)
            DC += (AP_TREEADD4_VCT(left[j], WD_PIX));
        else
            DC += DC;
        DC = (DC + (8<<0))>>(4+0);
    }else if(isleft == 0) {
        for (int j = 0; j < (2<<0);  ++j)
            DC += (AP_TREEADD4_VCT(left[j], WD_PIX));
        DC += DC;
        DC = (DC + (8<<0))>>(4+0);
    }else
        DC = 0X80;
    tmp = DC(WD_PIX-1,0);
    SB_SET_LINE_VAL(sb,0,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,1,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,2,WD_PIX,tmp);
    SB_SET_LINE_VAL(sb,3,WD_PIX,tmp);
    return sb;
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_TM16_4( //ref: lut:56, 3.19+1.25
        ap_uint<WD_PIX*4> abcd,
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<WD_PIX>    X44,
        ap_uint<1> istop,
        ap_uint<1> isleft
    )
{    //
#pragma HLS PIPELINE
    if(isleft==0){
        if(istop==0)
            return hls_TM4(abcd,ijkl,X44);
        else
            return hls_HE16_4(ijkl,isleft);
    }else{
        if(istop==0)
            return hls_VE16_4(abcd,istop);
        else
            return hls_HE16_4(ijkl,isleft);
    }
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_VE16_4( //ref: lut:56, 3.19+1.25//lut 452vs997, 2.72+1.25ns,
        ap_uint<WD_PIX*4> abcd,
        ap_uint<1> istop
        )
{    // vertical
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    const ap_uint<WD_PIX> val0= (istop!=1)?(A44):127;
    const ap_uint<WD_PIX> val1= (istop!=1)?(B44):127;
    const ap_uint<WD_PIX> val2= (istop!=1)?(C44):127;
    const ap_uint<WD_PIX> val3= (istop!=1)?(D44):127;

    SB_SET_COL_VAL(sb,0,WD_PIX,val0);
    SB_SET_COL_VAL(sb,1,WD_PIX,val1);
    SB_SET_COL_VAL(sb,2,WD_PIX,val2);
    SB_SET_COL_VAL(sb,3,WD_PIX,val3);
    return sb;
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_uint<WD_PIX*16> hls_HE16_4( //ref: lut:56, 3.19+1.25
        ap_uint<WD_PIX*4> ijkl,
        ap_uint<1> isleft
    )
{    //
#pragma HLS PIPELINE
    ap_uint<WD_PIX*16> sb;
    const ap_uint<WD_PIX> val0= (isleft!=1)?(I44):129;
    const ap_uint<WD_PIX> val1= (isleft!=1)?(J44):129;
    const ap_uint<WD_PIX> val2= (isleft!=1)?(K44):129;
    const ap_uint<WD_PIX> val3= (isleft!=1)?(L44):129;
    SB_SET_LINE_VAL(sb,0,WD_PIX,val0);
    SB_SET_LINE_VAL(sb,1,WD_PIX,val1);
    SB_SET_LINE_VAL(sb,2,WD_PIX,val2);
    SB_SET_LINE_VAL(sb,3,WD_PIX,val3);
    return sb;
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
ap_int<WD_WHT*16> hls_FTransformWHT(ap_int<WD_DCT*16> in) {
//FF:0, lut:1248; 4.12+0.62ns; Latency:1
  // input is 12b signed
  ap_int<WD_WHT*16> out;
  ap_int<WD_DCT+2> tmp[16];
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_DCT+1> a0 = ((ap_int<WD_DCT>)VCT_GET(in,0+i*4,WD_DCT) + (ap_int<WD_DCT>)VCT_GET(in,2+i*4,WD_DCT));  // 13b
    ap_int<WD_DCT+1> a1 = ((ap_int<WD_DCT>)VCT_GET(in,1+i*4,WD_DCT) + (ap_int<WD_DCT>)VCT_GET(in,3+i*4,WD_DCT));
    ap_int<WD_DCT+1> a2 = ((ap_int<WD_DCT>)VCT_GET(in,1+i*4,WD_DCT) - (ap_int<WD_DCT>)VCT_GET(in,3+i*4,WD_DCT));
    ap_int<WD_DCT+1> a3 = ((ap_int<WD_DCT>)VCT_GET(in,0+i*4,WD_DCT) - (ap_int<WD_DCT>)VCT_GET(in,2+i*4,WD_DCT));
    tmp[0 + i * 4] = a0 + a1;   // 14b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (int i = 0; i < 4; ++i) {
#pragma HLS unroll
    ap_int<WD_DCT+3> a0 = (tmp[0 + i] + tmp[8 + i]);  // 15b
    ap_int<WD_DCT+3> a1 = (tmp[4 + i] + tmp[12+ i]);
    ap_int<WD_DCT+3> a2 = (tmp[4 + i] - tmp[12+ i]);
    ap_int<WD_DCT+3> a3 = (tmp[0 + i] - tmp[8 + i]);
    ap_int<WD_DCT+4> b0 = a0 + a1;    // 16b
    ap_int<WD_DCT+4> b1 = a3 + a2;
    ap_int<WD_DCT+4> b2 = a3 - a2;
    ap_int<WD_DCT+4> b3 = a0 - a1;
    VCT_GET(out,  0 + i, WD_WHT) = b0 >> 1;     // 15b
    VCT_GET(out,  4 + i, WD_WHT) = b1 >> 1;
    VCT_GET(out,  8 + i, WD_WHT) = b2 >> 1;
    VCT_GET(out, 12 + i, WD_WHT) = b3 >> 1;
  }
    return out;
}
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////
void hls_SetBestAs4_mode_widen(
		ap_uint<WD_MODE>    ap_y_top_mode[MAX_NUM_MB_W*4],
		ap_uint<WD_MODE>    ap_y_left_mode[4],
		ap_uint<WD_MODE>      ap_y4_top_c_mode[16],
		ap_uint<WD_MODE>      ap_y_left_c_mode[4],
		ap_uint<WD_MODE*16>   *ap_y_mode_b,
		ap_uint<LG2_MAX_NUM_MB_W+2> x_sb_w
		){

	for(int y=0; y<4 ; y++){
#pragma HLS unroll
	    		for(int x=0; x<4 ; x++){
#pragma HLS unroll
	    			SB_GET((*ap_y_mode_b),y,x,WD_MODE) = ap_y4_top_c_mode[x + y*4];
	    			if(x==3)
	    				ap_y_left_mode[y] = ap_y4_top_c_mode[x + y*4];
	    			if(y==3)
	    				ap_y_top_mode[x_sb_w+x] = ap_y4_top_c_mode[x + y*4];
	    		}
	    	}
};
//////////=====================   TopVp8_compute_NoOut==========================/////////////////////////////

void hls_SetBestAs16_mode_widen(
		ap_uint<WD_MODE>    ap_y_top_mode[MAX_NUM_MB_W*4],
		ap_uint<WD_MODE>    ap_y_left_mode[4],
		ap_uint<WD_MODE>      ap_y16_mode_c,
		ap_uint<WD_MODE*16>   *ap_y_mode_b,
		ap_uint<LG2_MAX_NUM_MB_W+2> x_sb_w){

	for(int y=0; y<4 ; y++){
#pragma HLS unroll
	    		for(int x=0; x<4 ; x++){
#pragma HLS unroll
	    			SB_GET((*ap_y_mode_b),y,x,WD_MODE) = ap_y16_mode_c;//it->ap_rd_y16_b->mode;
	    			if(x==3)
	    				ap_y_left_mode[y] = ap_y16_mode_c;//it->ap_rd_y16_b->mode;
	    			if(y==3)
	    				ap_y_top_mode[x_sb_w+x] = ap_y16_mode_c;//it->ap_rd_y16_b->mode;
	    		}
	    	}
};
//////////======================================================================/////////////////////////////
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
//////////======================================================================/////////////////////////////
void TopVp8_RecordCoeff_hls_cnt(
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv,
		hls::stream<ap_int<64> >* str_pred, hls::stream<ap_int<6> >* str_ret,
		//output
		hls::stream<ap_uint<1> > &str_mb_type,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc2,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y2,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv2,
		hls::stream<ap_int<64> >* str_pred2, hls::stream<ap_int<6> >* str_ret2,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_dc,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_ac,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		hls::stream<ap_uint<8> > &str_cnt_ac,
		hls::stream<ap_uint<8> > &str_cnt_uv
		)
{
	hls::stream<ap_uint<2> > str_dc_ctx;
	hls::stream<ap_uint<2> > str_ac_ctx;
	hls::stream<ap_uint<2> > str_uv_ctx;
	hls::stream<ap_int<5> > str_dc_last;
	hls::stream<ap_int<5> > str_ac_last;
	hls::stream<ap_int<5> > str_uv_last;
	hls::stream<ap_int<WD_LEVEL> > str_dc;
	hls::stream<ap_int<WD_LEVEL> > str_ac;
	hls::stream<ap_int<WD_LEVEL> > str_uv;

	ap_NoneZero ap_nz;
	ap_uint<9> left_nz_dc = 0;
	ap_uint<9> ap_left_nz = 0;
	for (int y_mb = 0; y_mb < mb_h; y_mb++) {//printf("\ny=%2d: ", y_mb);
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
#pragma HLS PIPELINE off
		for (int x_mb = 0; x_mb < mb_w; x_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
#pragma HLS PIPELINE
			if(x_mb==0){
				left_nz_dc = 0;
				ap_left_nz = 0;
			}
			//loading the constx about nz
			ap_uint<9> ap_top_nz = ap_nz.load_top9(x_mb, y_mb);
			//ap_uint<9> ap_left_nz = ap_nz.load_left9(x_mb);

			//ap_uint<9> top_nz_ = ap_top_nz;
			ap_uint<9> top_nz_dc = ap_top_nz;
			ap_uint<9> top_nz_y = ap_top_nz;
			ap_uint<9> top_nz_uv = ap_top_nz;

			//ap_uint<9> left_nz_ = ap_left_nz;
			ap_uint<9> left_nz_y = ap_left_nz;
			ap_uint<9> left_nz_uv = ap_left_nz;


			left_nz_dc = RecordCoeff_dataflow(
					str_level_dc,
					str_level_y,
					str_level_uv,
					str_pred,
					str_ret,
					//output
					str_mb_type,
					str_level_dc2,
					str_level_y2,
					str_level_uv2,
					str_pred2, str_ret2,
					str_rec_dc,
					str_rec_ac,
					str_rec_uv,
					str_cnt_dc,
					str_cnt_ac,
					str_cnt_uv,
					top_nz_dc,//
					left_nz_dc,// = ap_left_nz;
			        top_nz_y,// = ap_top_nz;
					left_nz_y,// = ap_left_nz;
					top_nz_uv,// = ap_top_nz;
					left_nz_uv// = ap_left_nz;
					);
			top_nz_dc[8] = left_nz_dc[0];
			left_nz_dc[0] = 0;
			ap_uint<9> top_nz_;
			top_nz_(3,0) = top_nz_y(3,0);
			top_nz_(7,4) = top_nz_uv(7,4);
			top_nz_[8] = top_nz_dc[8];

			ap_uint<9> left_nz_;
			left_nz_(3,0) = left_nz_y(3,0);
			left_nz_(7,4) = left_nz_uv(7,4);
			ap_uint<25> nz = 0;
			nz |= (top_nz_[0] << 12) | (top_nz_[1] << 13);
			nz |= (top_nz_[2] << 14) | (top_nz_[3] << 15);
			nz |= (top_nz_[4] << 18) | (top_nz_[5] << 19);
			nz |= (top_nz_[6] << 22) | (top_nz_[7] << 23);
			nz |= (top_nz_[8] << 24); // we propagate the _top_ bit, esp. for intra4
			// left
			nz |= (left_nz_[0] << 3) | (left_nz_[1] << 7);
			nz |= (left_nz_[2] << 11);
			nz |= (left_nz_[4] << 17) | (left_nz_[6] << 21);

			ap_nz.left_nz[8] = left_nz_[8];
			ap_nz.nz_current = nz; 	//*it->nz_;
			ap_nz.store_nz(x_mb);

			ap_left_nz[0] = nz(3,  3);
			ap_left_nz[1] = nz(7,  7);
			ap_left_nz[2] = nz(11, 11);
			ap_left_nz[3] = nz(15, 15);
			  // left-U
			ap_left_nz[4] = nz(17, 17);
			ap_left_nz[5] = nz(19, 19);
			  // left-V
			ap_left_nz[6] = nz(21, 21);
			ap_left_nz[7] = nz(23, 23);

		}
	}
}

//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
ap_uint<9>  RecordCoeff_dataflow(
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv,
		hls::stream<ap_int<64> >* str_pred, hls::stream<ap_int<6> >* str_ret,
		//output
		hls::stream<ap_uint<1> > &str_mb_type,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc2,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y2,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv2,
		hls::stream<ap_int<64> >* str_pred2, hls::stream<ap_int<6> >* str_ret2,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_dc,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_ac,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		hls::stream<ap_uint<8> > &str_cnt_ac,
		hls::stream<ap_uint<8> > &str_cnt_uv,
		ap_uint<9> &top_nz_dc,//
		ap_uint<9> left_nz_dc,// = ap_left_nz;
        ap_uint<9> &top_nz_y,// = ap_top_nz;
		ap_uint<9> &left_nz_y,// = ap_left_nz;
		ap_uint<9> &top_nz_uv,// = ap_top_nz;
		ap_uint<9> &left_nz_uv// = ap_left_nz;
		)
{
#pragma HLS DATAFLOW
			// for old  pred pass
			ap_uint<64> pred = str_pred->read();
			str_pred2->write(pred);
			// for old ret pass
			ap_uint<6> ret = str_ret->read();
			str_ret2->write(ret);
			// get mb_type
			ap_uint<1> mb_type = ret(4, 4);
			str_mb_type.write(mb_type);
			ap_uint<9> leftreturn=RecordCoeff_dataflow_dc(
					mb_type,
					str_level_dc,
					str_level_dc2,
					str_rec_dc,
					str_cnt_dc,
					top_nz_dc,//
					left_nz_dc
					);
			RecordCoeff_dataflow_y(
					mb_type,
					str_level_y,
					str_level_y2,
					str_rec_ac,
					str_cnt_ac,
			        top_nz_y,// = ap_top_nz;
					left_nz_y
					);
			RecordCoeff_dataflow_uv(
					str_level_uv,
					str_level_uv2,
					str_rec_uv,
					str_cnt_uv,
					top_nz_uv,// = ap_top_nz;
					left_nz_uv// = ap_left_nz;
					);
			return leftreturn;


}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
ap_uint<9> RecordCoeff_dataflow_dc(
		ap_uint<1> mb_type,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc,
		//output
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_dc2,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_dc,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		ap_uint<9> &top_nz_dc,//
		ap_uint<9> left_nz_dc// = ap_left_nz;
		)
{
			ap_int<WD_LEVEL * 16> tmp16 = str_level_dc->read();
			str_level_dc2->write(tmp16);
			if (mb_type == 1) {   // i16x16
				ap_uint<2> ctx = top_nz_dc[8] + left_nz_dc[8];
				ap_int<5> last = FindLast(tmp16);
				VP8RecordCoeffs_hls_str_w_cnt(ctx, tmp16, 0, last, str_rec_dc, str_cnt_dc);
				top_nz_dc[8] = left_nz_dc[8] = last < 0 ? 0 : 1;
				int b = left_nz_dc[8];
				//printf("%d",b);
			}	//else printf(" ");//int a=top_nz_dc;int b=left_nz_dc;printf("%d",b);
			return  left_nz_dc&256 | top_nz_dc[8];

}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////

void RecordCoeff_dataflow_y(
		ap_uint<1> mb_type,
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y,
		//output
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_y2,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_ac,
		hls::stream<ap_uint<8> > &str_cnt_ac,
        ap_uint<9> &top_nz_y,// = ap_top_nz;
		ap_uint<9> &left_nz_y// = ap_left_nz;
		)
{
			int x, y;
			// luma-AC
			for (y = 0; y < 4; ++y) {
#pragma HLS PIPELINE
				for (x = 0; x < 4; ++x) {
#pragma HLS PIPELINE
					ap_uint<2> ctx = top_nz_y[x] + left_nz_y[y];
					ap_int<WD_LEVEL * 16> tmp = str_level_y->read();
					str_level_y2->write(tmp);   //for old
					ap_int<5> last = FindLast(tmp);
					VP8RecordCoeffs_hls_str_w_cnt(ctx, tmp, mb_type == 1, last, str_rec_ac, str_cnt_ac);
					top_nz_y[x] = left_nz_y[y] = last < 0 ? 0 : 1;
				}
			}

}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////

void RecordCoeff_dataflow_uv(
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv,
		//output
		hls::stream<ap_int<WD_LEVEL * 16> >* str_level_uv2,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_uv,
		ap_uint<9> &top_nz_uv,// = ap_top_nz;
		ap_uint<9> &left_nz_uv// = ap_left_nz;
		)
{
			int x, y, ch;
			// U/V
			for (ch = 0; ch <= 2; ch += 2) {
				for (y = 0; y < 2; ++y) {
					for (x = 0; x < 2; ++x) {
#pragma HLS PIPELINE
						ap_uint<2> ctx = top_nz_uv[4 + ch + x] + left_nz_uv[4 + ch + y];
						ap_int<WD_LEVEL * 16> tmp = str_level_uv->read();
						str_level_uv2->write(tmp);   //for old
						ap_int<5> last = FindLast(tmp);
						VP8RecordCoeffs_hls_str_w_cnt(ctx, tmp, 0, last,str_rec_uv, str_cnt_uv);
						top_nz_uv[4 + ch + x] = left_nz_uv[4 + ch + y] = last < 0 ? 0 : 1;
					}
				}
			}

}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////

ap_int<5> FindLast(ap_int<WD_LEVEL * 16> level)
{
#pragma HLS PIPELINE II=1
	ap_int<5> ret = 15;
	for (ret = 15; ret > -1; ret--) {
		ap_int<WD_LEVEL> tmp = VCT_GET(level, ret, WD_LEVEL);
		if (tmp != 0)
			return ret;
	}
	return ret;
}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
int VP8RecordCoeffs_hls_str_w_cnt(
        ap_uint<2> ctx,
	    ap_int<WD_LEVEL * 16> coeffs,
		ap_uint<1> first,
		ap_int<5> last,
		hls::stream<ap_uint<11> > &str_rec,
		hls::stream<ap_uint<8> > &str_cnt
		)
{
	ap_uint<8> cnt=0;
	int n = first;
	ap_uint<3> band_a = first;
	ap_uint<2> ctx_a = ctx;
	ap_uint<4> off_a = 0;
	if (last < 0) {
		Record_str(str_rec, 1, 0, band_a, ctx_a, 0);
		cnt++;//printf("cnt=%d \n",cnt.VAL );
		str_cnt.write(cnt);
		return 0;
	}
	ap_uint<1> isEealy_0 = 0;
	for (; n <= last; n++) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=16
#pragma HLS PIPELINE
		ap_int<WD_LEVEL> v;
		if (isEealy_0 == 0){
			Record_str(str_rec, 0, 1, band_a, ctx_a, 0);
			cnt++;//printf("cnt=%d \n;",cnt.VAL );
		}
		v = (ap_int<WD_LEVEL> ) VCT_GET(coeffs, n, WD_LEVEL);
		if (v == 0) {
			isEealy_0 = 1;
			Record_str(str_rec, 0, 0, band_a, ctx_a, 1);
			cnt++;//printf("cnt=%d\n ;",cnt.VAL );
			band_a = VP8EncBands_hls(n + 1);
			ctx_a = 0;
			continue;
		}
		isEealy_0 = 0;
		Record_str(str_rec, 0, 1, band_a, ctx_a, 1);
		cnt++;//printf("cnt=%d \n;",cnt.VAL );
		Record_str(str_rec, 0, 2u < (unsigned int) (v + 1), band_a, ctx_a, 2);
		cnt++;//printf("cnt=%d \n;",cnt.VAL );
		if (!(2u < (unsigned int) (v + 1))) {  // v = -1 or 1
			band_a = VP8EncBands_hls(n + 1);
			ctx_a = 1;
		} else {
			if (v < 0)
				v = -v;
			if (v > 67)
				v = 67;

			ap_uint<9> bits = VP8LevelCodes_hls[v - 1][1];
			int pattern = VP8LevelCodes_hls[v - 1][0];
			int i;
			for (i = 0; (pattern >>= 1) != 0; ++i) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=8
#pragma HLS PIPELINE
				const int mask = 2 << i;
				if (pattern & 1) {
					Record_str(str_rec, 0, !!(bits & mask), band_a, ctx_a,3 + i);
					cnt++;//printf("cnt=%d\n ;",cnt.VAL );
				}
			}
			band_a = VP8EncBands_hls(n + 1);
			ctx_a = 2;
		}
	} //while
	if (n < 16) {
		Record_str(str_rec, 1, 0, band_a, ctx_a, 0);
		cnt++;//printf("cnt=%d \n",cnt.VAL );
	}
	str_cnt.write(cnt);
	return 1;
}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
void Record_str(
		hls::stream<ap_uint<11> > &str_rec,
		ap_uint<1> isEnd,
		ap_uint<1> bit,
		ap_uint<3> band,
		ap_uint<2> ctx,
		ap_uint<4> off)
{
//#pragma HLS PIPELINE
	ap_uint<11> tmp;
	tmp(10, 10) = isEnd;
	tmp(9, 9) = bit;
	tmp(8, 6) = band;
	tmp(5, 4) = ctx;
	tmp(3, 0) = off;
	str_rec.write(tmp);
}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////
ap_uint<3> VP8EncBands_hls(ap_uint<5> n)
{
	/*const uint8_t VP8EncBands[16 + 1] = { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0  };*/
#pragma HLS INLINE
	if (n < 4)
		return n;
	else if (n == 4)
		return 6;
	else if (n == 5)
		return 4;
	else if (n == 6)
		return 5;
	else if (n == 15)
		return 7;
	else if (n == 16)
		return 0;
	else
		return 6;
}
//////////=====================   TopVp8_RecordCoeff_hls_cnt          ===========/////////////////////////////

//////////======================================================================/////////////////////////////
//////////============  TopVp8_RecordProb_hls_cnt                    ===========/////////////////////////////
//////////======================================================================/////////////////////////////
int TopVp8_RecordProb_hls_cnt(
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		hls::stream<ap_uint<1> > &str_mb_type,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_dc,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_ac,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		hls::stream<ap_uint<8> > &str_cnt_ac,
		hls::stream<ap_uint<8> > &str_cnt_uv,
		uint8_t* pout_prob //4, 8, 3,11
		)
{
//#pragma HLS INTERFACE m_axi port=pout_prob offset=slave bundle=gmem depth=4*8*3*11
//#pragma HLS INTERFACE s_axilite port=pout_prob bundle=control
//#pragma HLS INTERFACE s_axilite port=return bundle=control
	uint32_t stats[4][8][3][11];
#pragma HLS ARRAY_PARTITION variable=stats complete dim=1
	uint8_t p_coeffs[4][8][3][11];
#pragma HLS ARRAY_PARTITION variable=p_coeffs complete dim=1
	int t, b, c, p;
	for (t = 0; t < 4; ++t)
#pragma HLS UNROLL
		for (b = 0; b < 8; ++b)
			for (c = 0; c < 3; ++c)
				for (p = 0; p < 11; ++p) {
#pragma HLS PIPELINE
					stats[t][b][c][p] = 0;
					p_coeffs[t][b][c][p] = hls_VP8CoeffsProba0[t][b][c][p];
				}

	for (int y_mb = 0; y_mb < mb_h; y_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
		for (int x_mb = 0; x_mb < mb_w; x_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
//#pragma HLS PIPELINE
			ap_uint<1> type_mb = str_mb_type.read();
			RecordPorb_ReadCoeff_dataflow2_cnt(type_mb,
					str_rec_dc, str_rec_ac, str_rec_uv,
					str_cnt_dc, str_cnt_ac, str_cnt_uv,
					stats[1], stats[0],
					stats[3], stats[2]);
		}
	}
	int dirty = 1;
	int size = sizeof(p_coeffs);
	FinalizeTokenProbas_hls(stats, p_coeffs, &dirty);
	memcpy(pout_prob, p_coeffs, size);
	pout_prob[SIZE8_MEM_PROB-1] = dirty;
	return dirty;
}

void TopVp8_RecordProb_hls_cnt_HideDirty(
		ap_uint<LG2_MAX_NUM_MB_W> mb_w,
		ap_uint<LG2_MAX_NUM_MB_H> mb_h,
		hls::stream<ap_uint<1> > &str_mb_type,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_dc,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_ac,
		hls::stream<ap_uint<1 + 1 + 3 + 2 + 4> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		hls::stream<ap_uint<8> > &str_cnt_ac,
		hls::stream<ap_uint<8> > &str_cnt_uv,
		uint8_t* pout_prob //4, 8, 3,11
		)
{
//#pragma HLS INTERFACE m_axi port=pout_prob offset=slave bundle=gmem depth=4*8*3*11
//#pragma HLS INTERFACE s_axilite port=pout_prob bundle=control
//#pragma HLS INTERFACE s_axilite port=return bundle=control
	uint32_t stats[4][8][3][11];
#pragma HLS ARRAY_PARTITION variable=stats complete dim=1
	uint8_t p_coeffs[4][8][3][11];
#pragma HLS ARRAY_PARTITION variable=p_coeffs complete dim=1
	int t, b, c, p;
	for (t = 0; t < 4; ++t)
#pragma HLS UNROLL
		for (b = 0; b < 8; ++b)
			for (c = 0; c < 3; ++c)
				for (p = 0; p < 11; ++p) {
#pragma HLS PIPELINE
					stats[t][b][c][p] = 0;
					p_coeffs[t][b][c][p] = hls_VP8CoeffsProba0[t][b][c][p];
				}

	for (int y_mb = 0; y_mb < mb_h; y_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=68 max=256
		for (int x_mb = 0; x_mb < mb_w; x_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=120 max=256
//#pragma HLS PIPELINE
			ap_uint<1> type_mb = str_mb_type.read();
			RecordPorb_ReadCoeff_dataflow2_cnt(type_mb,
					str_rec_dc, str_rec_ac, str_rec_uv,
					str_cnt_dc, str_cnt_ac, str_cnt_uv,
					stats[1], stats[0],
					stats[3], stats[2]);
		}
	}
	int dirty = 1;
	int size = sizeof(p_coeffs);
	FinalizeTokenProbas_hls(stats, p_coeffs, &dirty);
	memcpy(pout_prob, p_coeffs, size);
	pout_prob[SIZE8_MEM_PROB-1] = dirty;
}

//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////
void RecordPorb_ReadCoeff_dataflow2_cnt(ap_uint<1> mb_type,
		hls::stream<ap_uint<11> > &str_rec_dc,
		hls::stream<ap_uint<11> > &str_rec_ac,
		hls::stream<ap_uint<11> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt_dc,
		hls::stream<ap_uint<8> > &str_cnt_ac,
		hls::stream<ap_uint<8> > &str_cnt_uv,
		uint32_t stats_dc[8][3][11],
		uint32_t stats_ac0_dc[8][3][11],
		uint32_t stats_ac3[8][3][11],
		uint32_t stats_uv[8][3][11])
{
#pragma HLS DATAFLOW
	RecordPorb_ReadCoeff_dataflow_dc_cnt(mb_type, str_rec_dc, str_cnt_dc, stats_dc);
	RecordPorb_ReadCoeff_dataflow_ac_cnt(mb_type, str_rec_ac, str_cnt_ac, stats_ac0_dc, stats_ac3);
	RecordPorb_ReadCoeff_dataflow_uv_cnt(str_rec_uv, str_cnt_uv,  stats_uv);
}

//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////
////RecordPorb_ReadCoeff_dataflow_dc_cnt//////////////////////////
void RecordPorb_ReadCoeff_dataflow_dc_cnt(
		ap_uint<1> mb_type,
		hls::stream<ap_uint<11> > &str_rec_dc,
		hls::stream<ap_uint<8> > &str_cnt,
		uint32_t stats_dc[8][3][11])
{
	if (mb_type == 1)
		VP8RecordCoeffs_hls_str_r_cnt(str_rec_dc, str_cnt, stats_dc);
}

////RecordPorb_ReadCoeff_dataflow_ac_cnt//////////////////////////
void RecordPorb_ReadCoeff_dataflow_ac_cnt(
		ap_uint<1> mb_type,
		hls::stream<ap_uint<11> > &str_rec_ac,
		hls::stream<ap_uint<8> > &str_cnt,
		uint32_t stats_ac0_dc[8][3][11],
		uint32_t stats_ac3[8][3][11])
{
//#pragma HLS PIPELINE
	for (int i = 0; i < 16; i++)
		if (mb_type)
			VP8RecordCoeffs_hls_str_r_cnt(str_rec_ac, str_cnt, stats_ac0_dc);
		else
			VP8RecordCoeffs_hls_str_r_cnt(str_rec_ac, str_cnt, stats_ac3);
}
////RecordPorb_ReadCoeff_dataflow_uv_cnt//////////////////////////
void RecordPorb_ReadCoeff_dataflow_uv_cnt(
		hls::stream<ap_uint<11> > &str_rec_uv,
		hls::stream<ap_uint<8> > &str_cnt,
		uint32_t stats_uv[8][3][11])
{
//#pragma HLS PIPELINE
	for (int i = 0; i < 8; i++)
		VP8RecordCoeffs_hls_str_r_cnt(str_rec_uv, str_cnt, stats_uv);
}

//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////

//////VP8RecordCoeffs_hls_str_r_cnt//////////////////
void VP8RecordCoeffs_hls_str_r_cnt(
		hls::stream<ap_uint<11> > &str_rec,
		hls::stream<ap_uint<8> > &str_cnt,
		uint32_t stats[8][3][11])
{
	uint32_t state, state0;
	ap_uint<1> bit, bit0;
	ap_uint<9> addr,addr0;
	addr0 = 0x1ff;// seens never be access
	addr = 0;

	ap_uint<8> cnt = str_cnt.read();
	for(int i=0; i < cnt; i++){
#pragma HLS dependence array inter false
#pragma HLS LOOP_TRIPCOUNT min=18 max=127
#pragma HLS PIPELINE II=1
		ap_uint<11> tmp = str_rec.read();
		addr = tmp(8,0);
		ap_uint<1> bit = tmp(9,9);

		ap_uint<3> band = addr(8, 6);
		ap_uint<2> ctx = addr(5, 4);
		ap_uint<4> off = addr(3, 0);
		if(addr!=addr0)
		{
			state = stats[band][ctx][off];
		}else{
			state = state0;

		}
		ap_uint<3> band0 = addr0(8, 6);
		ap_uint<2> ctx0 = addr0(5, 4);
		ap_uint<4> off0 = addr0(3, 0);
		if(i!=0)
			stats[band0][ctx0][off0] = state0;

		state0 = Record_hls(bit,state );
		addr0 = addr;
	}
	ap_uint<3> band0 = addr0(8, 6);
	ap_uint<2> ctx0 = addr0(5, 4);
	ap_uint<4> off0 = addr0(3, 0);
	stats[band0][ctx0][off0] = state0;
}
void VP8RecordCoeffs_hls_str_r_cnt_old(
		hls::stream<ap_uint<11> > &str_rec,
		hls::stream<ap_uint<8> > &str_cnt,
		uint32_t stats[8][3][11])
{
	ap_uint<8> cnt = str_cnt.read();
	for(int i=0; i < cnt; i++){
#pragma HLS LOOP_TRIPCOUNT min=18 max=127
#pragma HLS PIPELINE II=2
		ap_uint<11> tmp = str_rec.read();
		ap_uint<1> isEnd = tmp(10, 10);               // = isEnd;
		ap_uint<1> bit = tmp(9, 9);               // = bit;
		ap_uint<3> band = tmp(8, 6);               // = band;
		ap_uint<2> ctx = tmp(5, 4);               // = ctx;
		ap_uint<4> off = tmp(3, 0);               // = off;
		uint32_t state_old = stats[band][ctx][off];
		stats[band][ctx][off] = Record_hls(bit,state_old );
	}
}
//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////
ap_uint<32> Record_hls(ap_uint<1> bit, ap_uint<32> p)
{
#pragma HLS PIPELINE
	//ap_uint<32> p = *stats;
	ap_uint<16> p_h = p(31, 16);
	ap_uint<16> p_l = p(15, 0);
	if (p_h == 0xffff) {               // an overflow is inbound.
		p_h = 0x7fff;
		p_l = (p_l + 1 + (bit << 1)) >> 1;
	} else {
		p_h += 1;
		p_l += bit;
	}
	p(31, 16) = p_h;
	p(15, 0) = p_l;
	//*stats = p;
	return p;
}
//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////
static uint8_t hls_CalcTokenProba(int nb, int total) {// in fact return value range from 0~255, only needs  8 bits
  return nb ? (255 - nb * 255 / total) : 255;
}

static int hls_VP8BitCost(int bit, uint8_t proba) {
  return !bit ? hls_VP8EntropyCost[proba] : hls_VP8EntropyCost[255 - proba];
}

static int hls_BranchCost(int nb, int total, int proba) {
  return nb * hls_VP8BitCost(1, proba) + (total - nb) * hls_VP8BitCost(0, proba);
}
int FinalizeTokenProbas_hls(
		uint32_t p_stats[4][8][3][11],
		uint8_t p_coeffs_[4][8][3][11],
		int* dirty)
{
	int has_changed = 0;
	int size = 0;
	int t, b, c, p;
	for (t = 0; t < 4; ++t) {
//#pragma HLS UNROLL
		for (b = 0; b < 8; ++b) {
//#pragma HLS PIPELINE
			for (c = 0; c < 3; ++c) {
//#pragma HLS PIPELINE
				for (p = 0; p < 11; ++p) {
#pragma HLS PIPELINE
					uint32_t stats = p_stats[t][b][c][p];
					//wr//if(stats!=0)// printf("%s [%d][%d][%d][%d]stats:%d\n",
					//wr//  printf("t=%d, b=%d, c=%d, p= %d, stats=%x\n",t,b,c,p,stats);//     __FUNCTION__, t, b, c, p, stats);
					const int nb = (stats >> 0) & 0xffff;
					const int total = (stats >> 16) & 0xffff;
					const int update_proba = hls_VP8CoeffsUpdateProba[t][b][c][p];
					const int old_p = hls_VP8CoeffsProba0[t][b][c][p];
					const int new_p = hls_CalcTokenProba(nb, total);
					const int old_cost = hls_BranchCost(nb, total, old_p)
							+ hls_VP8BitCost(0, update_proba);
					const int new_cost = hls_BranchCost(nb, total, new_p)
							+ hls_VP8BitCost(1, update_proba) + 8 * 256;
					const int use_new_p = (old_cost > new_cost);
					// printf("%s use_new_p:%d old_cost:%d new_cost:%d\n",
					//     __FUNCTION__, use_new_p, old_cost, new_cost);
					size += hls_VP8BitCost(use_new_p, update_proba);
					if (use_new_p) { // only use proba that seem meaningful enough.
						p_coeffs_[t][b][c][p] = new_p;
						has_changed |= (new_p != old_p);
						// printf("%s has_changed:%d new_p:%d old_p:%d\n",
						//   __FUNCTION__, has_changed, new_p, old_p);
						size += 8 * 256;
					} else {
						p_coeffs_[t][b][c][p] = old_p;
					}
				}
			}
		}
	}
	// printf("%d %d==========================\n", __LINE__, has_changed);
	*dirty = has_changed;
	return size;
}
//////////============  TopVp8_RecordProb_hls_cnt_HideDirty          ===========/////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//==================================kernel_2_ArithmeticCoding===========================================//
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//kernel_2_ArithmeticCoding
//|-memcpy
//|-Kernel2_top_read
//|-kernel_2_RecordTokens_pre
//|-kernel_2_CreateTokens_with_isFinal
//|-VP8EmitTokens_str_hls_4stages
//|-PackStr2Mem32_t_NoLast
//|-PackWideStr2Mem32_t_NoLast

void kernel_2_ArithmeticCoding(
		uint32_t pin_level[SIZE32_MEM_BW],
		uint8_t* pin_prob,//2048 instead of [4 * 8 * 3 * 11],
		uint32_t pout_bw[SIZE32_MEM_BW],
		uint32_t pout_ret[SIZE32_MEM_RET],
		uint32_t pout_pred[SIZE32_MEM_PRED])
{
#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pin_prob     offset=slave bundle=gmem1 depth=2048      num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_bw      offset=slave bundle=gmem2 depth=65536*384/4/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_ret     offset=slave bundle=gmem3 depth=65536*1/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_pred    offset=slave bundle=gmem4 depth=65536*16/2/4  num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb

#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
#pragma HLS INTERFACE s_axilite port=pin_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_bw   bundle=control
#pragma HLS INTERFACE s_axilite port=pout_ret  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_pred bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW

	uint8_t prob[4 * 8 * 3 * 11];
	memcpy(prob, pin_prob, sizeof(prob));

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
#pragma HLS STREAM variable=str_level_dc depth=4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_ac;
#pragma HLS STREAM variable=str_level_ac depth=8*16
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
#pragma HLS STREAM variable=str_level_uv depth=8*8
	hls::stream<ap_uint<64> > str_pred;
#pragma HLS STREAM variable=str_pred depth=64
	hls::stream<ap_uint<6> > str_ret;
#pragma HLS STREAM variable=str_ret depth=64
	hls::stream<ap_uint<1> > str_type_mb;
#pragma HLS STREAM variable=str_type_mb depth=64
	hls::stream<uint16_t> str_mb_h;
#pragma HLS STREAM variable=str_mb_h depth=64
	hls::stream<uint16_t> str_mb_w;
#pragma HLS STREAM variable=str_mb_w depth=64
	Kernel2_top_read(pin_level,

			str_level_dc, str_level_ac, str_level_uv, str_pred, str_ret,
			str_type_mb, str_mb_h, str_mb_w);
	hls::stream<ap_uint<64> > str_0_dc;
#pragma HLS STREAM    variable=str_0_dc depth=64
	hls::stream<ap_uint<64> > str_1_dc;
#pragma HLS STREAM    variable=str_1_dc depth=64
	hls::stream<ap_uint<64> > str_2_dc;
#pragma HLS STREAM    variable=str_2_dc depth=64
	hls::stream<ap_uint<64> > str_3_dc;
#pragma HLS STREAM    variable=str_3_dc depth=64
	hls::stream<ap_uint<64> > str_0_ac;
#pragma HLS STREAM    variable=str_0_ac depth=64
	hls::stream<ap_uint<64> > str_1_ac;
#pragma HLS STREAM    variable=str_1_ac depth=64
	hls::stream<ap_uint<64> > str_2_ac;
#pragma HLS STREAM    variable=str_2_ac depth=64
	hls::stream<ap_uint<64> > str_3_ac;
#pragma HLS STREAM    variable=str_3_ac depth=64
	hls::stream<ap_uint<64> > str_0_uv;
#pragma HLS STREAM    variable=str_0_uv depth=64
	hls::stream<ap_uint<64> > str_1_uv;
#pragma HLS STREAM    variable=str_1_uv depth=64
	hls::stream<ap_uint<64> > str_2_uv;
#pragma HLS STREAM    variable=str_2_uv depth=64
	hls::stream<ap_uint<64> > str_3_uv;
#pragma HLS STREAM    variable=str_3_uv depth=64
	hls::stream<ap_uint<1> > str_type_mb_out;
#pragma HLS STREAM    variable=str_type_mb_out depth=64

	hls::stream<uint16_t> str_mb_h_out;
#pragma HLS STREAM variable=str_mb_h_out depth=64
	hls::stream<uint16_t> str_mb_w_out;
#pragma HLS STREAM variable=str_mb_w_out depth=64
	kernel_2_RecordTokens_pre(str_mb_h, str_mb_w, str_type_mb, str_level_dc,
			str_level_ac, str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
			str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
			str_2_uv, str_3_uv, str_mb_h_out, str_mb_w_out, str_type_mb_out);

	hls::stream<uint16_t> str_mb_h_out2;
#pragma HLS STREAM variable=str_mb_h_out2 depth=64
	hls::stream<uint16_t> str_mb_w_out2;
#pragma HLS STREAM variable=str_mb_w_out2 depth=64
	hls::stream<ap_uint<16> > tokens_str_final;
#pragma HLS STREAM variable=tokens_str_final depth=1024
	kernel_2_CreateTokens_with_isFinal(
			str_mb_h_out, str_mb_w_out, str_type_mb_out, str_0_dc, str_1_dc,
			str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
			str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_mb_h_out2,
			str_mb_w_out2, tokens_str_final);

	uint16_t mb_h = str_mb_h_out2.read();
	uint16_t mb_w = str_mb_w_out2.read();
	VP8EmitTokens_str_hls_4stages(pout_bw, tokens_str_final, (uint8_t*) prob); //VP8EmitTokens_hls(pout_bw, &tokens, (uint8_t*)prob);
	PackStr2Mem32_t_NoLast<6, 256>(pout_ret, str_ret, mb_h * mb_w);
	PackWideStr2Mem32_t_NoLast<64, 256>(pout_pred, str_pred, mb_h * mb_w);
}

//==================================kernel_2_ArithmeticCoding===========================================//
void Kernel2_top_read(
		uint32_t pin_level[SIZE32_MEM_LEVEL],
		//output
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_ac,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_uv,
		hls::stream<ap_uint<64> > &str_pred,
		hls::stream<ap_uint<6> > &str_ret,
		hls::stream<ap_uint<1> > &str_type_mb,
		hls::stream<uint16_t> &str_mb_h,
		hls::stream<uint16_t> &str_mb_w)
{
//#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
//#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
//#pragma HLS INTERFACE s_axilite port=return bundle=control
	uint16_t y_mb = 0;
	uint16_t x_mb = 0;
	uint16_t mb_h = 1;
	uint16_t mb_w = 1;
	uint32_t tmp_arr[256];
	uint32_t* psrc = pin_level;
	uint32_t num_mb = 0;
	do {
#pragma HLS LOOP_TRIPCOUNT min=120*68 max=256*256
#pragma HLS PIPELINE
		memcpy(tmp_arr, psrc, 256 * sizeof(uint32_t));
		psrc += 256;
		if (num_mb == 0) {
			mb_h = tmp_arr[420 / 2] >> 16;
			mb_w = tmp_arr[420 / 2] & 0xffff;
			str_mb_h.write(mb_h);
			str_mb_w.write(mb_w);
		}
		Kernel2_read__array_to_str(tmp_arr, str_level_dc, str_level_ac,
				str_level_uv, str_pred, str_ret, str_type_mb);
		if (x_mb != (mb_w - 1))
			x_mb++;
		else {
			x_mb = 0;
			y_mb++;
		}
		num_mb++;
	} while (y_mb != (mb_h) || x_mb != 0);
}
//==================================kernel_2_ArithmeticCoding===========================================//
void Kernel2_read__array_to_str(
		uint32_t pin[256],
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_ac,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_uv,
		hls::stream<ap_uint<64> > &str_pred,
		hls::stream<ap_uint<6> > &str_ret,
		hls::stream<ap_uint<1> > &str_type_mb)
{
#pragma HLS PIPELINE
	uint32_t* plevel = pin;
	int x, y, ch;
	ap_int<WD_LEVEL * 16> tmp = SetVectFrom32bit(plevel);
	str_level_dc.write(tmp);
	plevel += 16 / 2;

	// luma-AC
	for (y = 0; y < 4; ++y) {
		for (x = 0; x < 4; ++x) {
#pragma HLS PIPELINE
			ap_int<WD_LEVEL * 16> tmp = SetVectFrom32bit(plevel);
			str_level_ac.write(tmp);
			int16_t test[16];
			CPY16(test, tmp, WD_LEVEL);
			plevel += 16 / 2;
		}
	}

	// U/V
	for (ch = 0; ch <= 2; ch += 2) {
		for (y = 0; y < 2; ++y) {
			for (x = 0; x < 2; ++x) {
#pragma HLS PIPELINE
				ap_int<WD_LEVEL * 16> tmp = SetVectFrom32bit(plevel);
				str_level_uv.write(tmp);
				plevel += 16 / 2;
			}
		}
	}

	ap_uint<64> vct_pred = SetVect64From32bit(pin + 200);
	str_pred.write(vct_pred);
	ap_uint<6> ret = (ap_uint<6> ) pin[416 / 2];
	str_ret.write(ret);
	str_type_mb.write(ret(4, 4));
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_int<WD_LEVEL * 16> SetVectFrom32bit(uint32_t* pin)
{
#pragma HLS INLINE
	ap_int<WD_LEVEL * 16> ret;
	for (int i = 0; i < 8; i++) {
#pragma HLS PIPELINE
		ap_int<32> tmp32 = pin[i];
		ap_int<WD_LEVEL> tmp_l = tmp32( WD_LEVEL - 1, 0);
		ap_int<WD_LEVEL> tmp_h = tmp32( WD_LEVEL - 1 + 16, 16);
		ret((i * 2 + 1) * WD_LEVEL - 1, (i * 2 + 0) * WD_LEVEL) = tmp_l(
				WD_LEVEL - 1, 0);
		ret((i * 2 + 2) * WD_LEVEL - 1, (i * 2 + 1) * WD_LEVEL) = tmp_h(
				WD_LEVEL - 1, 0);
	}
	return ret;
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<4 * 16> SetVect64From32bit(uint32_t* pin)
{
#pragma HLS INLINE
	ap_uint<4 * 16> ret;
	for (int i = 0; i < 8; i++) {
#pragma HLS PIPELINE
		ap_uint<32> tmp32 = pin[i];
		ap_uint<4> tmp_l = tmp32(4 - 1, 0);
		ap_uint<4> tmp_h = tmp32(4 - 1 + 16, 16);
		ret((i * 2 + 1) * 4 - 1, i * 2 * 4) = tmp_l(4 - 1, 0);
		ret((i * 2 + 2) * 4 - 1, (i * 2 + 1) * 4) = tmp_h(4 - 1, 0);
	}
	return ret;
}
//==================================kernel_2_ArithmeticCoding===========================================//
void kernel_2_RecordTokens_pre(
		hls::stream<uint16_t> &str_mb_h,
		hls::stream<uint16_t> &str_mb_w,
		hls::stream<ap_uint<1> > &str_type_mb,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_ac,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_uv,
		hls::stream<ap_uint<64> > &str_0_dc,
		hls::stream<ap_uint<64> > &str_1_dc,
		hls::stream<ap_uint<64> > &str_2_dc,
		hls::stream<ap_uint<64> > &str_3_dc,
		hls::stream<ap_uint<64> > &str_0_ac,
		hls::stream<ap_uint<64> > &str_1_ac,
		hls::stream<ap_uint<64> > &str_2_ac,
		hls::stream<ap_uint<64> > &str_3_ac,
		hls::stream<ap_uint<64> > &str_0_uv,
		hls::stream<ap_uint<64> > &str_1_uv,
		hls::stream<ap_uint<64> > &str_2_uv,
		hls::stream<ap_uint<64> > &str_3_uv,
		hls::stream<uint16_t> &str_mb_h_out,
		hls::stream<uint16_t> &str_mb_w_out,
		hls::stream<ap_uint<1> > &str_type_mb_out)
{
	static ap_NoneZero ap_nz;
	uint16_t mb_h = str_mb_h.read();
	uint16_t mb_w = str_mb_w.read();
	str_mb_h_out.write(mb_h);
	str_mb_w_out.write(mb_w);
	for (uint16_t y_mb = 0; y_mb < mb_h; y_mb++)
#pragma HLS LOOP_TRIPCOUNT min=16 max=68
		for (uint16_t x_mb = 0; x_mb < mb_w; x_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=120
			RecordTokens_nrd2_mb_w(
					&ap_nz, x_mb, y_mb, str_type_mb, str_level_dc, str_level_ac,
					str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
					str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
					str_2_uv, str_3_uv, str_type_mb_out);
		}
}
//==================================kernel_2_ArithmeticCoding===========================================//
/////RecordTokens_nrd2_mb_w////////////////////////////////
void RecordTokens_nrd2_mb_w(
		ap_NoneZero *ap_nz,
		int x_, int y_,
		hls::stream<ap_uint<1> > &str_type_mb,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_dc,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_ac,
		hls::stream<ap_int<WD_LEVEL * 16> > &str_level_uv,
		hls::stream<ap_uint<64> > &str_0_dc,
		hls::stream<ap_uint<64> > &str_1_dc,
		hls::stream<ap_uint<64> > &str_2_dc,
		hls::stream<ap_uint<64> > &str_3_dc,
		hls::stream<ap_uint<64> > &str_0_ac,
		hls::stream<ap_uint<64> > &str_1_ac,
		hls::stream<ap_uint<64> > &str_2_ac,
		hls::stream<ap_uint<64> > &str_3_ac,
		hls::stream<ap_uint<64> > &str_0_uv,
		hls::stream<ap_uint<64> > &str_1_uv,
		hls::stream<ap_uint<64> > &str_2_uv,
		hls::stream<ap_uint<64> > &str_3_uv,
		hls::stream<ap_uint<1> > &str_type_mb_out)
{
	int x, y, ch;
	ap_uint<9> ap_top_nz = ap_nz->load_top9(x_, y_);
	ap_uint<9> ap_left_nz = ap_nz->load_left9(x_);
	ap_uint<9> top_nz_ = ap_top_nz;  //=ap_top_nz[i];f
	ap_uint<9> left_nz_ = ap_left_nz;  //= ap_left_nz[i];

	ap_uint<1> type_ = str_type_mb.read();
	str_type_mb_out.write(type_);
	ap_int<WD_LEVEL * 16> c_hls = str_level_dc.read();
	if (type_ == 1) {   // i16x16
		const int ctx = top_nz_[8] + left_nz_[8];
		int last = FindLast(c_hls);
		top_nz_[8] = left_nz_[8] = last < 0 ? 0 : 1;
		VP8RecordCoeffTokens_hls_w(ctx, 1, last, c_hls, str_0_dc, str_1_dc,
				str_2_dc, str_3_dc);
	}

	// luma-AC
	for (y = 0; y < 4; ++y) {
		for (x = 0; x < 4; ++x) {
			const int ctx = top_nz_[x] + left_nz_[y];
			ap_int<WD_LEVEL * 16> c_hls = str_level_ac.read();
			int16_t test[16];
			CPY16(test, c_hls, WD_LEVEL);
			int last = FindLast(c_hls);
			int coeff_type = type_ == 1 ? 0 : 3;
			top_nz_[x] = left_nz_[y] = last < 0 ? 0 : 1;
			VP8RecordCoeffTokens_hls_w(ctx, coeff_type, last, c_hls, str_0_ac,
					str_1_ac, str_2_ac, str_3_ac);
		}
	}

	// U/V
	for (ch = 0; ch <= 2; ch += 2) {
		for (y = 0; y < 2; ++y) {
			for (x = 0; x < 2; ++x) {
				const int ctx = top_nz_[4 + ch + x] + left_nz_[4 + ch + y];
				ap_int<WD_LEVEL * 16> c_hls = str_level_uv.read();
				int last = FindLast(c_hls);
				top_nz_[4 + ch + x] = left_nz_[4 + ch + y] = last < 0 ? 0 : 1;
				VP8RecordCoeffTokens_hls_w(ctx, 2, last, c_hls, str_0_uv,
						str_1_uv, str_2_uv, str_3_uv);
			}
		}
	}

	uint32_t nz = 0;
	nz |= (top_nz_[0] << 12) | (top_nz_[1] << 13);
	nz |= (top_nz_[2] << 14) | (top_nz_[3] << 15);
	nz |= (top_nz_[4] << 18) | (top_nz_[5] << 19);
	nz |= (top_nz_[6] << 22) | (top_nz_[7] << 23);
	nz |= (top_nz_[8] << 24);  // we propagate the _top_ bit, esp. for intra4
	// left
	nz |= (left_nz_[0] << 3) | (left_nz_[1] << 7);
	nz |= (left_nz_[2] << 11);
	nz |= (left_nz_[4] << 17) | (left_nz_[6] << 21);

	ap_nz->left_nz[8] = left_nz_[8];
	ap_nz->nz_current = nz; 	//*it->nz_;
	ap_nz->store_nz(x_);

}
//==================================kernel_2_ArithmeticCoding===========================================//
int VP8RecordCoeffTokens_hls_w(
		ap_uint<2> ctx,
		ap_uint<2> coeff_type,
		ap_int<5> last,
		ap_int<WD_LEVEL * 16> coeffs,
		hls::stream<ap_uint<64> > &str_0,
		hls::stream<ap_uint<64> > &str_1,
		hls::stream<ap_uint<64> > &str_2,
		hls::stream<ap_uint<64> > &str_3)
{

	TokensStr0_hls(ctx, coeff_type, last, str_0);
	ap_uint<11> base_id_last =
			TOKEN_ID2((ap_uint<11> )coeff_type, (coeff_type == 0 ? 1 : 0))
					+ ctx * 11;	//TOKEN_ID0(coeff_type, coeff_type==0, ctx);
	ap_uint<11> base_id = base_id_last;
	for (int i = 0; i <= last; i++) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=16
#pragma HLS PIPELINE
		if (i == 0 && coeff_type == 0)	//first==1)
			continue;
		ap_int<WD_LEVEL> c = (ap_int<WD_LEVEL> ) VCT_GET(coeffs, i, WD_LEVEL);//coeffs[i];
		ap_uint<1> sign = c < 0;
		ap_uint<WD_LEVEL> v;
		if (c < 0)
			v = (-c);
		else
			v = c;
		//str_1-----------------------------
		//sign
		ap_uint<1> isV_N0 = v != 0;
		ap_uint<1> isLastBEi = i < last;
		ap_uint<1> isV_B1 = v > 1;
		//str_2-------------------------------------------
		ap_uint<1> isV_B4 = v > 4;
		ap_uint<1> isV_N2 = v != 2;
		ap_uint<1> isV_4 = v == 4;
		ap_uint<1> isV_B10 = v > 10;
		//str_3-------------------------------------------
		ap_uint<1> isV_B6 = v > 6;
		ap_uint<1> isV_6 = v == 6;
		ap_uint<1> isV_BE9 = v >= 9;
		ap_uint<1> isV_even = 1 - v & 1;		 //!(v & 1)
		//-------------------------------------------------

		ap_uint<11> base_id_next;
		const uint8_t VP8EncBands[16 + 1] = { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6,
				6, 6, 6, 6, 7, 0 };
		uint8_t VP8EncBands_ = VP8EncBands_hls(i + 1);
		if (v == 0)
			base_id_next = TOKEN_ID2((ap_uint<11> )coeff_type, VP8EncBands_);//VP8EncBands[i + 1]);//
		else if (v == 1)
			base_id_next = TOKEN_ID2((ap_uint<11> )coeff_type, VP8EncBands_)
					+ 11;
		else
			base_id_next = TOKEN_ID2((ap_uint<11> )coeff_type, VP8EncBands_)
					+ 22;

		TokensStr1_hls(isV_N0, isV_B1, sign, isLastBEi, base_id, base_id_next,
				v, str_1);
		TokensStr2_hls(isV_B4, isV_N2, isV_4, isV_B10, base_id, str_2);
		TokensStr3_hls(isV_B6, isV_6, isV_BE9, isV_even, base_id, str_3);
		base_id = base_id_next;
	}
}
//==================================kernel_2_ArithmeticCoding===========================================//

//==================================kernel_2_ArithmeticCoding===========================================//
void PackToken_hls(
		ap_uint<64> &w,
		ap_uint<2> be,
		uint32_t bit,
		uint32_t proba_idx)
{
	ap_uint<16> tmp = (bit << 15) | proba_idx;
	w(be * 16 + 15, be * 16) = tmp(15, 0);
}
/////PackConstantToken_hls////////////////
void PackConstantToken_hls(
		ap_uint<64> &w,
		ap_uint<2> be,
		uint32_t bit,
		uint32_t proba_idx)
{
	ap_uint<16> tmp = (bit << 15) | (1u << 14) | proba_idx;
	w(be * 16 + 15, be * 16) = tmp(15, 0);
}
/////TokensStr0_hls////////////////////////
void TokensStr0_hls(
		ap_uint<2> ctx,
		ap_uint<2> coeff_type,
		ap_int<5> last,
		hls::stream<ap_uint<64> > &str_0)
{
#pragma HLS PIPELINE II=1
	ap_uint<64> w = 0;
	ap_uint<1> isLastN = last < 0;
	ap_uint<11> base_id_last = TOKEN_ID2((ap_uint<11> )coeff_type, (coeff_type == 0 ? 1 : 0)) + ctx * 11;
	w(16 + 4, 16) = last;
	w(16 + 8 + 2, 16 + 8) = coeff_type;
	PackToken_hls(w, 0, isLastN, base_id_last);
	str_0.write(w);
}
/////TokensStr1_hls//////////////////////////////////////

int TokensStr1_hls(
		ap_uint<1> isV_N0,	 // = v!=0,
		ap_uint<1> isV_B1,	 // = v>1
		ap_uint<1> sign,
		ap_uint<1> isLastBEi,	 // = i<last,
		ap_uint<11> base_id,
		ap_uint<11> base_id_next,
		ap_uint<11> v,
		hls::stream<ap_uint<64> > &str_1)
{
#pragma HLS PIPELINE II=1
	ap_uint<64> w = 0;
	PackToken_hls(w, 0, isV_N0, base_id + 1);
	PackToken_hls(w, 1, isV_B1, base_id + 2);
	PackConstantToken_hls(w, 2, sign, v);
	PackToken_hls(w, 3, isLastBEi, base_id_next);
	str_1.write(w);
}
/////TokensStr2_hls/////////////////////////////////////////
int TokensStr2_hls(
		ap_uint<1> isV_B4,	 // = v>4;
		ap_uint<1> isV_N2,	 // = v!=2;
		ap_uint<1> isV_4,	 // = v==4;
		ap_uint<1> isV_B10,	 // = v>10;
		ap_uint<11> base_id,
		hls::stream<ap_uint<64> > &str_2)
{
#pragma HLS PIPELINE II=1
	ap_uint<64> w = 0;
	PackToken_hls(w, 0, isV_B4, base_id + 3);
	PackToken_hls(w, 1, isV_N2, base_id + 4);
	PackToken_hls(w, 2, isV_4, base_id + 5);
	PackToken_hls(w, 3, isV_B10, base_id + 6);
	str_2.write(w);
}
/////TokensStr3_hls//////////////////////////////////////
int TokensStr3_hls(
		ap_uint<1> isV_B6,	 // = v>6;
		ap_uint<1> isV_6,	 // = v==6;
		ap_uint<1> isV_BE9,	 // = v>=9;
		ap_uint<1> isV_even,	 // = 1-v&1;//!(v & 1)
		ap_uint<11> base_id,
		hls::stream<ap_uint<64> > &str_3)
{
#pragma HLS PIPELINE II=1
	ap_uint<64> w = 0;
	PackToken_hls(w, 0, isV_B6, base_id + 7);
	PackToken_hls(w, 1, isV_6, 159);
	PackToken_hls(w, 2, isV_BE9, 165);
	PackToken_hls(w, 3, isV_even, 145);
	str_3.write(w);
}
//==================================kernel_2_ArithmeticCoding===========================================//

//==================================kernel_2_ArithmeticCoding===========================================//
void kernel_2_CreateTokens_with_isFinal(
		hls::stream<uint16_t> &str_mb_h,
		hls::stream<uint16_t> &str_mb_w,
		hls::stream<ap_uint<1> > &str_type_mb,
		hls::stream<ap_uint<64> > &str_0_dc,
		hls::stream<ap_uint<64> > &str_1_dc,
		hls::stream<ap_uint<64> > &str_2_dc,
		hls::stream<ap_uint<64> > &str_3_dc,
		hls::stream<ap_uint<64> > &str_0_ac,
		hls::stream<ap_uint<64> > &str_1_ac,
		hls::stream<ap_uint<64> > &str_2_ac,
		hls::stream<ap_uint<64> > &str_3_ac,
		hls::stream<ap_uint<64> > &str_0_uv,
		hls::stream<ap_uint<64> > &str_1_uv,
		hls::stream<ap_uint<64> > &str_2_uv,
		hls::stream<ap_uint<64> > &str_3_uv,
		hls::stream<uint16_t> &str_mb_h_out,
		hls::stream<uint16_t> &str_mb_w_out,
		hls::stream<ap_uint<16> > &str_tokens_final)
{
	uint16_t mb_h = str_mb_h.read();
	uint16_t mb_w = str_mb_w.read();
	str_mb_h_out.write(mb_h);
	str_mb_w_out.write(mb_w);
	for (uint16_t y_mb = 0; y_mb < mb_h; y_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=68
		for (uint16_t x_mb = 0; x_mb < mb_w; x_mb++) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=120
			RecordTokens_nrd2_mb_r_str_AddFinal(str_type_mb, str_0_dc, str_1_dc,
					str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
					str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_tokens_final,
					y_mb == (mb_h - 1) && (x_mb == (mb_w - 1)));   //&tokens);
		}
	}
}
//==================================kernel_2_ArithmeticCoding===========================================//
void RecordTokens_nrd2_mb_r_str_AddFinal(
		hls::stream<ap_uint<1> > &str_type_mb,
		hls::stream<ap_uint<64> > &str_0_dc,
		hls::stream<ap_uint<64> > &str_1_dc,
		hls::stream<ap_uint<64> > &str_2_dc,
		hls::stream<ap_uint<64> > &str_3_dc,
		hls::stream<ap_uint<64> > &str_0_ac,
		hls::stream<ap_uint<64> > &str_1_ac,
		hls::stream<ap_uint<64> > &str_2_ac,
		hls::stream<ap_uint<64> > &str_3_ac,
		hls::stream<ap_uint<64> > &str_0_uv,
		hls::stream<ap_uint<64> > &str_1_uv,
		hls::stream<ap_uint<64> > &str_2_uv,
		hls::stream<ap_uint<64> > &str_3_uv,
		hls::stream<ap_uint<16> > &tokens,
		bool isFinal)
{
	int x, y, ch;
	ap_uint<1> type_mb = str_type_mb.read();
//#pragma HLS DATAFLOW
	if (type_mb == 1) {
		VP8RecordCoeffTokens_hls_r_str_AddFanel(str_0_dc, str_1_dc, str_2_dc,
				str_3_dc, tokens, false);
	}
	for (y = 0; y < 4; ++y)
		for (x = 0; x < 4; ++x) {
			VP8RecordCoeffTokens_hls_r_str_AddFanel(str_0_ac, str_1_ac,
					str_2_ac, str_3_ac, tokens, false);
		}
	for (ch = 0; ch <= 2; ch += 2)
		for (y = 0; y < 2; ++y)
			for (x = 0; x < 2; ++x) {
				VP8RecordCoeffTokens_hls_r_str_AddFanel(str_0_uv, str_1_uv,
						str_2_uv, str_3_uv, tokens,
						isFinal && (ch == 2) && (y == 1) && (x == 1));
			}
}
//==================================kernel_2_ArithmeticCoding===========================================//
////////VP8RecordCoeffTokens_hls_r_str_AddFanel///////////////////////////////
#define GET_isLastN_B(w) (w(15 + 0*16,15 + 0*16 ))
#define GET_isLastN_P(w) (w(10 + 0*16, 0 + 0*16 ))
#define GET_last(w) ((ap_int<5>)w(16+4, 16 ))
#define GET_coeff_type(w) (w(16+8+2, 16+8 ))
#define GET_isV_N0_B(w)    (w(15 + 0*16,15 + 0*16 ))
#define GET_isV_N0_P(w)    (w(10 + 0*16, 0 + 0*16 ))
#define GET_isV_B1_B(w)    (w(15 + 1*16,15 + 1*16 ))
#define GET_isV_B1_P(w)    (w(10 + 1*16, 0 + 1*16 ))
#define GET_sign_B(w)      (w(15 + 2*16,15 + 2*16 ))
#define GET_sign_v(w)      (w(10 + 2*16, 0 + 2*16 ))
#define GET_isLastBEi_B(w) (w(15 + 3*16,15 + 3*16 ))
#define GET_isLastBEi_P(w) (w(10 + 3*16, 0 + 3*16 ))
#define GET_isV_B4_B(w)  (w(15 + 0*16,15 + 0*16 ))
#define GET_isV_B4_P(w)  (w(10 + 0*16, 0 + 0*16 ))
#define GET_isV_N2_B(w)  (w(15 + 1*16,15 + 1*16 ))
#define GET_isV_N2_P(w)  (w(10 + 1*16, 0 + 1*16 ))
#define GET_isV_4_B(w)   (w(15 + 2*16,15 + 2*16 ))
#define GET_isV_4_P(w)   (w(10 + 2*16, 0 + 2*16 ))
#define GET_isV_B10_B(w) (w(15 + 3*16,15 + 3*16 ))
#define GET_isV_B10_P(w) (w(10 + 3*16, 0 + 3*16 ))
#define GET_isV_B6_B(w)  (w(15 + 0*16,15 + 0*16 ))
#define GET_isV_B6_P(w)  (w(10 + 0*16, 0 + 0*16 ))
#define GET_isV_6_B(w)   (w(15 + 1*16,15 + 1*16 ))
#define GET_isV_6_P(w)   (w(10 + 1*16, 0 + 1*16 ))
#define GET_isV_BE9_B(w) (w(15 + 2*16,15 + 2*16 ))
#define GET_isV_BE9_P(w) (w(10 + 2*16, 0 + 2*16 ))
#define GET_isV_even_B(w) (w(15 + 3*16,15 + 3*16 ))
#define GET_isV_even_P(w) (w(10 + 3*16, 0 + 3*16 ))
int VP8RecordCoeffTokens_hls_r_str_AddFanel(
		hls::stream<ap_uint<64> > &str_0,
		hls::stream<ap_uint<64> > &str_1,
		hls::stream<ap_uint<64> > &str_2,
		hls::stream<ap_uint<64> > &str_3,
		hls::stream<ap_uint<16> > &tokens,
		bool isFinal)
{

	ap_uint<64> w0 = str_0.read();
	ap_uint<1> b_w0 = GET_isLastN_B(w0);
	ap_uint<11> p_w0 = GET_isLastN_P(w0);
	ap_uint<5> last_w0 = GET_last(w0);
	ap_uint<5> type_w0 = GET_coeff_type(w0);
	ap_uint<11> base_id = p_w0;

	if (!AddToken_hls_AddFanel(tokens, !b_w0, p_w0, isFinal & b_w0)) {// last==-1
		return 0;
	}

	for (int i = 0; i <= last_w0; i++) {
#pragma HLS LOOP_TRIPCOUNT min=0 max=16
#pragma HLS PIPELINE off
		if (i == 0 && type_w0 == 0)		//first==1)
			continue;
		ap_uint<64> w1 = str_1.read();
		ap_uint<64> w2 = str_2.read();
		ap_uint<64> w3 = str_3.read();
		ap_uint<1> sign_b = GET_sign_B(w1);		//c < 0;
		ap_uint<WD_LEVEL-1> v = GET_sign_v(w1);	//c < 0;w4s(WD_LEVEL-2,0) ;
		ap_uint<1> isV_N0 = GET_isV_N0_B(w1);		//v!=0;//
		ap_uint<1> isLastBEi = GET_isLastBEi_B(w1);	//  i<last_w0;//;i<last;//
		ap_uint<1> isV_B1 = GET_isV_B1_B(w1);		//v>1;//
		ap_uint<11> isV_N0_p = GET_isV_N0_P(w1);		//v!=0;//
		ap_uint<11> isLastBEi_p = GET_isLastBEi_P(w1);//  i<last_w0;//;i<last;//
		ap_uint<11> isV_B1_p = GET_isV_B1_P(w1);		//v>1;//
		//str_2-------------------------------------------
		ap_uint<1> isV_B4 = GET_isV_B4_B(w2);	 // v>4;
		ap_uint<1> isV_N2 = GET_isV_N2_B(w2);	 // v!=2;
		ap_uint<1> isV_4 = GET_isV_4_B(w2);	 // v==4;
		ap_uint<1> isV_B10 = GET_isV_B10_B(w2);	 // v>10;
		ap_uint<11> isV_B4_p = GET_isV_B4_P(w2);	 // v>4;
		ap_uint<11> isV_N2_p = GET_isV_N2_P(w2);	 // v!=2;
		ap_uint<11> isV_4_p = GET_isV_4_P(w2);	 // v==4;
		ap_uint<11> isV_B10_p = GET_isV_B10_P(w2);	 // v>10;
		//str_3-------------------------------------------
		ap_uint<1> isV_B6 = GET_isV_B6_B(w3);	 // v>6;
		ap_uint<1> isV_6 = GET_isV_6_B(w3);	 //v==6;
		ap_uint<1> isV_BE9 = GET_isV_BE9_B(w3);	 //v>=9;
		ap_uint<1> isV_even = GET_isV_even_B(w3);	 //1-v&1;//!(v & 1)
		ap_uint<11> isV_B6_p = GET_isV_B6_P(w3);	 // v>6;
		ap_uint<11> isV_6_p = GET_isV_6_P(w3);	 //v==6;
		ap_uint<11> isV_BE9_p = GET_isV_BE9_P(w3);	 //v>=9;
		ap_uint<11> isV_even_p = GET_isV_even_P(w3);	 //1-v&1;//!(v & 1)
		//-------------------------------------------------
		ap_uint<1> isV_S19 = v < 19;	 //residue < (8 << 1)
		ap_uint<1> isV_S35 = v < 35;	 //residue < (8 << 2)
		ap_uint<1> isV_S67 = v < 67;	 //residue < (8 << 2)

		AddToken_hls_AddFanel(tokens, isV_N0, isV_N0_p, 0);
		base_id = isV_N0_p - 1;
		if (v != 0) {
			if (AddToken_hls_AddFanel(tokens, isV_B1, isV_B1_p, 0)) { //v=[2,2047]
				if (!AddToken_hls_AddFanel(tokens, isV_B4, isV_B4_p, 0)) { //v=[2,4]
					if (AddToken_hls_AddFanel(tokens, isV_N2, isV_N2_p, 0)) //v=[3,4]
						AddToken_hls_AddFanel(tokens, isV_4, isV_4_p, 0); //v=[4,4]
				} else if (!AddToken_hls_AddFanel(tokens, isV_B10, isV_B10_p,
						0)) { // base_id + 6)) {//v=[5,10]//GET__B, GET__P
					if (!AddToken_hls_AddFanel(tokens, isV_B6, isV_B6_p, 0)) { //base_id + 7)) {//v=[5,6]//GET__B, GET__P
						AddConstantToken_hls_AddFanel(tokens, isV_6, 159, 0); //v=[6]//GET__B, GET__P
					} else { //v=[7,10]
						AddConstantToken_hls_AddFanel(tokens, isV_BE9, 165, 0); //v=[9,10]//GET__B, GET__P
						AddConstantToken_hls_AddFanel(tokens, isV_even, 145, 0); //v=[8,10]//GET__B, GET__P
					}
				} else { //v=[11~2047]
					const uint8_t* tab;
					uint16_t residue = v - 3; //[8~2044]
					if (isV_S19) {  //[8 15]        // VP8Cat3  (3b)
						AddToken_hls_AddFanel(tokens, 0, base_id + 8, 0);
						AddToken_hls_AddFanel(tokens, 0, base_id + 9, 0);
						residue -= (8 << 0);
						const uint8_t VP8Cat3[] = { 173, 148, 140 };
						tab = VP8Cat3;
						AddConstantToken_hls_AddFanel(tokens, !!(v - 11 & 4), 173, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 11 & 2), 148, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 11 & 1), 140, 0);
					} else if (isV_S35) {   //[16,31]// VP8Cat4  (4b)
						AddToken_hls_AddFanel(tokens, 0, base_id + 8, 0);
						AddToken_hls_AddFanel(tokens, 1, base_id + 9, 0);
						residue -= (8 << 1);
						const uint8_t VP8Cat4[] = { 176, 155, 140, 135 };
						tab = VP8Cat4;
						AddConstantToken_hls_AddFanel(tokens, !!(v - 19 & 8), 176, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 19 & 4), 155, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 19 & 2), 140, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 19 & 1), 135, 0);
					} else if (isV_S67) {   // [32,63] VP8Cat5  (5b)
						AddToken_hls_AddFanel(tokens, 1, base_id + 8, 0);
						AddToken_hls_AddFanel(tokens, 0, base_id + 10, 0);
						residue -= (8 << 2);
						const uint8_t VP8Cat5[] = { 180, 157, 141, 134, 130 };
						tab = VP8Cat5;
						AddConstantToken_hls_AddFanel(tokens, !!(v - 35 & 16), 180, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 35 & 8), 157, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 35 & 4), 141, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 35 & 2), 134, 0);
						AddConstantToken_hls_AddFanel(tokens, !!(v - 35 & 1), 130, 0);
					} else {                         // [64,2048)VP8Cat6 (11b)
						AddToken_hls_AddFanel(tokens, 1, base_id + 8, 0);
						AddToken_hls_AddFanel(tokens, 1, base_id + 10, 0);
						residue -= (8 << 3);
						const uint8_t VP8Cat6[] = { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 };
						tab = VP8Cat6;
						for (int k = 10; k >= 0; k--)
							AddConstantToken_hls_AddFanel(tokens,!!(v - 67 & (1 << k)), *tab++, 0);
					}                         //[64,2048)
				}                         //v=[11~2047]
			}                         //v=[2~2047]
		}//v!=0
		if (v == 0)
			continue;
		AddConstantToken_hls_AddFanel(tokens, sign_b, 128, isFinal & (15 == i));
		if (i == 15 || !AddToken_hls_AddFanel(tokens, isLastBEi, isLastBEi_p, isFinal&&(!isLastBEi))) {
			return 1;   // EOB
		}
	}//for
	return 1;
}

//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<1> AddConstantToken_hls_AddFanel(
		hls::stream<ap_uint<16> > &str_tokens,
		ap_uint<1> bit,
		ap_uint<11> proba_idx,
		ap_uint<1> isFinal)
{
#pragma HLS PIPELINE
	ap_uint<16> tmp;
	tmp[15] = bit;
	tmp[14] = 1;
	tmp[13] = 0;
	tmp[12] = isFinal;
	tmp[11] = 0;
	tmp(10, 0) = proba_idx;
	str_tokens.write(tmp);
	return bit;
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<1> AddToken_hls_AddFanel(
		hls::stream<ap_uint<16> > &str_tokens,
		ap_uint<1> bit,
		ap_uint<11> proba_idx,
		ap_uint<1> isFinal)
{
#pragma HLS PIPELINE
	ap_uint<16> tmp;
	tmp[15] = bit;
	tmp[14] = 0;
	tmp[13] = 0;
	tmp[12] = isFinal;
	tmp[11] = 0;
	tmp(10, 0) = proba_idx;
	str_tokens.write(tmp);
	return bit;
}
//==================================kernel_2_ArithmeticCoding===========================================//
//==================================kernel_2_ArithmeticCoding===========================================//

void VP8EmitTokens_str_hls_4stages(
		uint32_t pout_bw[SIZE32_MEM_BW],
		hls::stream<ap_uint<16> > &str_token,
		uint8_t probas[4 * 8 * 3 * 11])
{
	hls::stream<ap_uint<18> > str_Last_isBit_Bits;
#pragma HLS STREAM variable=str_Last_isBit_Bits depth=64
	ap_uint<8> bw_range;   // = 254;      // range-1
	ap_uint<24> bw_value;   // = 0;
	ap_int<4> bw_nb_bits;   // = -8;
	ap_uint<32> bw_pos;   // = 0;
	ap_uint<16> bw_run;   // = 0;

	VP8EmitTokens_allstr_hls_dataflow_4stages(pout_bw, str_token, probas, bw_range,
			bw_value, bw_nb_bits, bw_pos, bw_run);

	uint32_t* p_bw = pout_bw + SIZE32_MEM_BW - SIZE32_AC_STATE;
	p_bw[0] = bw_range;
	p_bw[1] = bw_value;
	p_bw[2] = bw_nb_bits;
	p_bw[3] = bw_pos;
	p_bw[4] = bw_run;
	p_bw[5] = MAX_NUM_MB_W * MAX_NUM_MB_H * 384 / SYSTEM_MIN_COMP_RATIO - 1; //max_pos
	p_bw[6] = 0;   //error
	p_bw[7] = 0;   //index_ac_encoder / num_segment
}

//==================================kernel_2_ArithmeticCoding===========================================//
void VP8EmitTokens_allstr_hls_dataflow_4stages(
		uint32_t pout_bw[SIZE32_MEM_BW],
		hls::stream<ap_uint<16> > &str_token,
		uint8_t probas[4 * 8 * 3 * 11],
		ap_uint<8> &bw_range,   // = 254;      // range-1
		ap_uint<24> &bw_value,   //= 0;
		ap_int<4> &bw_nb_bits,   // = -8;
		ap_uint<32> &bw_pos,   //= 0
		ap_uint<16> &bw_run)   // = 0,
{
#pragma HLS DATAFLOW
	// range loop (a loop)
	hls::stream<ap_uint<2 + 3 + 8> > str_fnl_bit_shift_split_1;
#pragma HLS STREAM variable=str_fnl_bit_shift_split_1 depth=64
	bw_range = hls_AC_range_str(str_token, probas, str_fnl_bit_shift_split_1);

	// Value loop (c loop)
	hls::stream<ap_uint<18> > str_Last_isBit_Bits;
#pragma HLS STREAM variable=str_Last_isBit_Bits depth=64
	ap_uint<4 + 24> nb_value = hls_AC_value_str(str_fnl_bit_shift_split_1,
			str_Last_isBit_Bits);
	bw_nb_bits = nb_value(27, 24);
	bw_value = nb_value(23, 0);

	//Package loop-1
	hls::stream<ap_uint<26> > str_isFinal_run_cy_pre;
#pragma HLS STREAM variable=str_isFinal_run_cy_pre depth=1024
	ap_uint<16> run = VP8PutBit_hls_BytePackage_str_run(
			str_Last_isBit_Bits,
			str_isFinal_run_cy_pre);

	//Package loop-2
	hls::stream<ap_uint<9> > str_Last_byte;
#pragma HLS STREAM variable=str_Last_byte depth=1024
	ap_uint<32> pos = VP8PutBit_hls_BytePackage_str_pos(
			str_isFinal_run_cy_pre,
			str_Last_byte);

	bw_run = run(15, 0);
	bw_pos = pos(31, 0);
	PackStr2Mem_t<9, 8, 256>(pout_bw, str_Last_byte);
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<8> hls_AC_range_str(
    hls::stream<ap_uint<16> > &str_token,
    uint8_t probas[4 * 8 * 3 * 11],
    hls::stream<ap_uint<2 + 3 + 8> > &str_fnl_bit_shift_split_1)
 {
	ap_uint<8> range_old = 254;
	ap_uint<8> split_1;
	ap_uint<3> shift;
	ap_uint<2 + 3 + 8> tmp;
	ap_uint<1> isFinal = 0;
	do {
#pragma HLS LOOP_TRIPCOUNT min=1920*1088/256*384*2 max=4096*4096/256*384*2
#pragma HLS PIPELINE II=1
		ap_uint<16> token = str_token.read();		//[i];
		isFinal = token[12];
		ap_uint<1> bit = token[15];
		ap_uint<1> isFix = token[14];
		ap_uint<8> p;
		if (isFix)
			p = token(7, 0);
		else
			p = probas[token(10, 0)];
		ap_uint<8> tmp_p = (range_old * p) >> 8;
		split_1 = tmp_p + 1;

		ap_uint<8> range_new;
		ap_uint<8> range_nor1 = range_old - tmp_p;
		ap_uint<8> range_nor2 = tmp_p + 1;

		if (bit) {
			shift = range_nor1.countLeadingZeros();
			range_new = (range_nor1 << range_nor1.countLeadingZeros()) - 1;
		} else {
			shift = range_nor2.countLeadingZeros();
			range_new = (range_nor2 << range_nor2.countLeadingZeros()) - 1;
		}
		tmp[12] = isFinal;
		tmp[11] = bit;
		tmp(10, 8) = shift;
		tmp(7, 0) = split_1;
		str_fnl_bit_shift_split_1.write(tmp);
		range_old = range_new;
	} while (isFinal == 0);
	return range_old;
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<4 + 24> hls_AC_value_str(
		hls::stream<ap_uint<2 + 3 + 8> > &str_fnl_bit_shift_split_1,
		hls::stream<ap_uint<18> > &str_fnl_isBit_Bits) {

	ap_uint<24> v_old = 0;
	ap_int<4> nb_old = -8;

	ap_uint<1> isFinal = 0;
	ap_uint<1> bit;
	ap_uint<3> shift;
	ap_uint<8> split_1;

	ap_uint<16> bits;
	ap_uint<1> isBits;

	do {
#pragma HLS LOOP_TRIPCOUNT min=1920*1088/256*384*2 max=4096*4096/256*384*2
#pragma HLS PIPELINE II=1
		ap_uint<2 + 3 + 8> fnl_bit_shift_split_1 = str_fnl_bit_shift_split_1.read();
		isFinal = fnl_bit_shift_split_1[12];
		bit = fnl_bit_shift_split_1[11];
		shift = fnl_bit_shift_split_1(10, 8);
		split_1 = fnl_bit_shift_split_1(7, 0);
		isBits = 0;
		ap_uint<24> v_new = v_old;//
		ap_int<4> nb_new = nb_old;//
		if (bit)
			//v_old += split_1;
			v_new +=split_1;
		v_new <<= shift;
		nb_new += shift;
		if (nb_new > 0) {
			isBits = 1;
			ap_uint<4> s = 8 + nb_new;
			bits = v_new(23, s);
			v_new(23, s) = 0;  //v_old -= bits << s;
			nb_new -= 8;
		}
		ap_uint<18> Last_isBit_Bits;
		Last_isBit_Bits(17, 17) = isFinal;
		Last_isBit_Bits(16, 16) = isBits;
		Last_isBit_Bits(15, 0) = bits;
		if (isBits || isFinal)
			str_fnl_isBit_Bits.write(Last_isBit_Bits);
		v_old = v_new;
		nb_old = nb_new;
	} while (isFinal == 0);
	ap_uint<4 + 24> ret;
	ret(27, 24) = nb_old;
	ret(23, 0) = v_old;
	return ret;
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<16> VP8PutBit_hls_BytePackage_str_run(
		hls::stream<ap_uint<18> > &str_Last_isBit_Bits,
		hls::stream<ap_uint<26> > &str_isFinal_run_cy_pre) {

	//hls::stream<ap_uint<9+16> > str_isFinal_run_cy_pre;
	ap_uint<26> isFinal_run_cy_pre;//1+16+1+8
	ap_uint<16> p_run_ = 0;
	ap_uint<8> byte_pre = 0xff;//0xff is the initial value that means byte_pre is never used
	ap_uint<1> isLast;
	do {/*This loop iterates p_run_ and byte_pre*/
#pragma HLS PIPELINE
		ap_uint<18> Last_isBit_Bits = str_Last_isBit_Bits.read();
		isLast = Last_isBit_Bits(17, 17);
		ap_uint<1> isBits = Last_isBit_Bits(16, 16);
		ap_uint<16> bits = Last_isBit_Bits(15, 0);

		if (isBits) {
			if(byte_pre==0xff){
				byte_pre(7,0) = bits(7,0);
			}else if ((bits & 0xff) != 0xff) {
				isFinal_run_cy_pre( 7,0)   = byte_pre(7, 0);
				isFinal_run_cy_pre( 8,8)   = bits(8,8);
				isFinal_run_cy_pre(16+8,9) = p_run_;
				isFinal_run_cy_pre[25]     = 0;
				p_run_=0;
				byte_pre(7, 0) = bits(7,0);
				str_isFinal_run_cy_pre.write(isFinal_run_cy_pre);
			} else {
				p_run_++;
			}
		}
	} while (isLast == 0);

	if(isLast && byte_pre!=0xff){
		isFinal_run_cy_pre( 7,0)   = byte_pre(7, 0);
		isFinal_run_cy_pre( 8,8)   = 0;//cy
		isFinal_run_cy_pre(16+8,9) = 0;//run
		isFinal_run_cy_pre[25]     = 1;//Final
		str_isFinal_run_cy_pre.write(isFinal_run_cy_pre);
	}

	return p_run_;
}
//==================================kernel_2_ArithmeticCoding===========================================//
ap_uint<32> VP8PutBit_hls_BytePackage_str_pos(
		hls::stream<ap_uint<26> > &str_isFinal_run_cy_pre,
		hls::stream<ap_uint<9> > &str_Last_byte) {

	ap_uint<32> p_pos_ = 0;
	ap_uint<1> isLast;
	do {
		ap_uint<1+16+9> isFinal_run_cy_pre  = str_isFinal_run_cy_pre.read();
		isLast = isFinal_run_cy_pre[25];
		ap_uint<16> run = isFinal_run_cy_pre(24,9);
		ap_uint<1> cy = isFinal_run_cy_pre[8];
		ap_uint<9> byte = isFinal_run_cy_pre(7,0) + cy;
		byte[8] = isLast;
		str_Last_byte.write(byte);
		p_pos_++;
		ap_uint<9> stuff;
		if(cy)
			stuff=0;
		else
			stuff=0x0ff;
		for(int i=0;i<run;i++){
#pragma HLS PIPELINE
			str_Last_byte.write(stuff);
			p_pos_++;
		}
	} while (isLast == 0);

	return p_pos_;
}
//==================================kernel_2_ArithmeticCoding===========================================//
/*
 * //Other used for host convenience
 */
void  set_vect_to(ap_uint<8*16> src,unsigned char* des, int strip)
{
    ap_uint<8*16> sb;
    for(int i=0;i<4 ;i++)
        for(int j=0; j<4; j++){
            des[j+strip*i]=SB_GET(src,i,j,8);
    }
}
//////////////////////////////////////////////////////////////////////////////
void  kernel_IntraPredLoop2_NoOut_1(
		int32_t* p_info,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob) {
#pragma HLS INTERFACE m_axi port=pout_level offset=slave bundle=gmem1 depth=65536*512/2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=p_info     offset=slave bundle=gmem0 depth=64          num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=ysrc       offset=slave bundle=gmem2 depth=4096*4096/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=usrc       offset=slave bundle=gmem3 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=vsrc       offset=slave bundle=gmem4 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_prob  offset=slave bundle=gmem5 depth=2048        num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16

#pragma HLS INTERFACE s_axilite port=p_info     bundle=control
#pragma HLS INTERFACE s_axilite port=ysrc       bundle=control
#pragma HLS INTERFACE s_axilite port=usrc       bundle=control
#pragma HLS INTERFACE s_axilite port=vsrc       bundle=control
#pragma HLS INTERFACE s_axilite port=pout_level bundle=control
#pragma HLS INTERFACE s_axilite port=pout_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int p_readinfo[64];
	memcpy(p_readinfo, p_info, 64 * sizeof(int));
	ap_uint<32> id_pic;
	ap_uint<32> mb_line;
	ap_uint<LG2_MAX_W_PIX> y_stride;
	ap_uint<LG2_MAX_W_PIX> uv_stride;
	ap_uint<LG2_MAX_W_PIX> width;
	ap_uint<LG2_MAX_W_PIX> height;
	ap_uint<LG2_MAX_NUM_MB_W> mb_w;
	ap_uint<LG2_MAX_NUM_MB_H> mb_h;
	ap_uint<WD_LMD> lambda_p16;
	ap_uint<WD_LMD> lambda_p44;
	ap_uint<WD_LMD> tlambda;
	ap_uint<WD_LMD> lambda_uv;
	ap_uint<WD_LMD> tlambda_m;
	hls_QMatrix hls_qm1, hls_qm2, hls_qm_uv;
	ap_int<WD_sharpen * 16> ap_sharpen, ap_sharpen_uv;

	//Initializing image variables, once for one picture
	{// For convenience, extend the code at top module to show all parameters used by kernel of intra-prediction
		id_pic = p_readinfo[0];//reserved for future
		mb_line = p_readinfo[1];// reserved for future, to show current line number of mb
		y_stride = p_readinfo[2];
		uv_stride = p_readinfo[3];
		width = p_readinfo[4];
		height = p_readinfo[5];
		mb_w = p_readinfo[2 + 2 + 2];
		mb_h = p_readinfo[3 + 2 + 2];
		lambda_p16 = p_readinfo[4 + 2 + 2];
		lambda_p44 = p_readinfo[5 + 2 + 2];
		tlambda = p_readinfo[6 + 2 + 2];
		lambda_uv = p_readinfo[7 + 2 + 2];
		tlambda_m = p_readinfo[8 + 2 + 2];

		hls_qm1.q_0 = p_readinfo[11 + 2];     // quantizer steps
		hls_qm1.q_n = p_readinfo[12 + 2];
		hls_qm1.iq_0 = p_readinfo[13 + 2];    // reciprocals fixed point.
		hls_qm1.iq_n = p_readinfo[14 + 2];
		hls_qm1.bias_0 = p_readinfo[15 + 2];  // rounding bias
		hls_qm1.bias_n = p_readinfo[16 + 2];

		hls_qm2.q_0 = p_readinfo[17 + 2];     // quantizer steps
		hls_qm2.q_n = p_readinfo[18 + 2];
		hls_qm2.iq_0 = p_readinfo[19 + 2];    // reciprocals fixed point.
		hls_qm2.iq_n = p_readinfo[20 + 2];
		hls_qm2.bias_0 = p_readinfo[21 + 2];  // rounding bias
		hls_qm2.bias_n = p_readinfo[22 + 2];

		hls_qm_uv.q_0 = p_readinfo[23 + 2];   // quantizer steps
		hls_qm_uv.q_n = p_readinfo[24 + 2];
		hls_qm_uv.iq_0 = p_readinfo[25 + 2];  // reciprocals fixed point.
		hls_qm_uv.iq_n = p_readinfo[26 + 2];
		hls_qm_uv.bias_0 = p_readinfo[27 + 2];// rounding bias
		hls_qm_uv.bias_n = p_readinfo[28 + 2];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen,i,WD_sharpen) = p_info[29 + 2 + i];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen_uv,i,WD_sharpen) = p_readinfo[29 + 2 + 16 + i];
	}//end of initialization
	int dirty = 0;
	TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(id_pic,  //p_info[0],
				mb_line,  //p_info[1],
				y_stride,  //p_info[2],  // ,//pic->y_stride,
				uv_stride,  //p_info[3], // ,//pic->uv_stride
				width,  //p_info[4],  // ,//pic->width
				height,  //p_info[5],  // ,//pic->height
				mb_w,  //p_info[2+2+2],///,
				mb_h,  //p_info[3+2+2],//,
				lambda_p16,  //p_info[4+2+2],//dqm->lambda_i16_,
				lambda_p44,  //p_info[5+2+2],//dqm->lambda_i4_,
				tlambda,  //p_info[6+2+2],//dqm->tlambda_,
				lambda_uv,  //p_info[7+2+2],//dqm->lambda_uv_,
				tlambda_m,  //p_info[8+2+2],//dqm->lambda_mode_,
				hls_qm1, hls_qm2, hls_qm_uv, ap_sharpen, ap_sharpen_uv, ysrc, //4096x4096
				usrc, //2048x2048
				vsrc, //2048x2048
				pout_level, //65536*512
				pout_prob, &dirty);
}
void  kernel_IntraPredLoop2_NoOut_2(
		int32_t* p_info,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob) {
#pragma HLS INTERFACE m_axi port=pout_level offset=slave bundle=gmem1 depth=65536*512/2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=p_info     offset=slave bundle=gmem0 depth=64          num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=ysrc       offset=slave bundle=gmem2 depth=4096*4096/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=usrc       offset=slave bundle=gmem3 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=vsrc       offset=slave bundle=gmem4 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_prob  offset=slave bundle=gmem5 depth=2048        num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16

#pragma HLS INTERFACE s_axilite port=p_info     bundle=control
#pragma HLS INTERFACE s_axilite port=ysrc       bundle=control
#pragma HLS INTERFACE s_axilite port=usrc       bundle=control
#pragma HLS INTERFACE s_axilite port=vsrc       bundle=control
#pragma HLS INTERFACE s_axilite port=pout_level bundle=control
#pragma HLS INTERFACE s_axilite port=pout_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int p_readinfo[64];
	memcpy(p_readinfo, p_info, 64 * sizeof(int));
	ap_uint<32> id_pic;
	ap_uint<32> mb_line;
	ap_uint<LG2_MAX_W_PIX> y_stride;
	ap_uint<LG2_MAX_W_PIX> uv_stride;
	ap_uint<LG2_MAX_W_PIX> width;
	ap_uint<LG2_MAX_W_PIX> height;
	ap_uint<LG2_MAX_NUM_MB_W> mb_w;
	ap_uint<LG2_MAX_NUM_MB_H> mb_h;
	ap_uint<WD_LMD> lambda_p16;
	ap_uint<WD_LMD> lambda_p44;
	ap_uint<WD_LMD> tlambda;
	ap_uint<WD_LMD> lambda_uv;
	ap_uint<WD_LMD> tlambda_m;
	hls_QMatrix hls_qm1, hls_qm2, hls_qm_uv;
	ap_int<WD_sharpen * 16> ap_sharpen, ap_sharpen_uv;

	//Initializing image variables, once for one picture
	{// For convenience, extend the code at top module to show all parameters used by kernel of intra-prediction
		id_pic = p_readinfo[0];//reserved for future
		mb_line = p_readinfo[1];// reserved for future, to show current line number of mb
		y_stride = p_readinfo[2];
		uv_stride = p_readinfo[3];
		width = p_readinfo[4];
		height = p_readinfo[5];
		mb_w = p_readinfo[2 + 2 + 2];
		mb_h = p_readinfo[3 + 2 + 2];
		lambda_p16 = p_readinfo[4 + 2 + 2];
		lambda_p44 = p_readinfo[5 + 2 + 2];
		tlambda = p_readinfo[6 + 2 + 2];
		lambda_uv = p_readinfo[7 + 2 + 2];
		tlambda_m = p_readinfo[8 + 2 + 2];

		hls_qm1.q_0 = p_readinfo[11 + 2];     // quantizer steps
		hls_qm1.q_n = p_readinfo[12 + 2];
		hls_qm1.iq_0 = p_readinfo[13 + 2];    // reciprocals fixed point.
		hls_qm1.iq_n = p_readinfo[14 + 2];
		hls_qm1.bias_0 = p_readinfo[15 + 2];  // rounding bias
		hls_qm1.bias_n = p_readinfo[16 + 2];

		hls_qm2.q_0 = p_readinfo[17 + 2];     // quantizer steps
		hls_qm2.q_n = p_readinfo[18 + 2];
		hls_qm2.iq_0 = p_readinfo[19 + 2];    // reciprocals fixed point.
		hls_qm2.iq_n = p_readinfo[20 + 2];
		hls_qm2.bias_0 = p_readinfo[21 + 2];  // rounding bias
		hls_qm2.bias_n = p_readinfo[22 + 2];

		hls_qm_uv.q_0 = p_readinfo[23 + 2];   // quantizer steps
		hls_qm_uv.q_n = p_readinfo[24 + 2];
		hls_qm_uv.iq_0 = p_readinfo[25 + 2];  // reciprocals fixed point.
		hls_qm_uv.iq_n = p_readinfo[26 + 2];
		hls_qm_uv.bias_0 = p_readinfo[27 + 2];// rounding bias
		hls_qm_uv.bias_n = p_readinfo[28 + 2];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen,i,WD_sharpen) = p_info[29 + 2 + i];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen_uv,i,WD_sharpen) = p_readinfo[29 + 2 + 16 + i];
	}//end of initialization
	int dirty = 0;
	TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(id_pic,  //p_info[0],
				mb_line,  //p_info[1],
				y_stride,  //p_info[2],  // ,//pic->y_stride,
				uv_stride,  //p_info[3], // ,//pic->uv_stride
				width,  //p_info[4],  // ,//pic->width
				height,  //p_info[5],  // ,//pic->height
				mb_w,  //p_info[2+2+2],///,
				mb_h,  //p_info[3+2+2],//,
				lambda_p16,  //p_info[4+2+2],//dqm->lambda_i16_,
				lambda_p44,  //p_info[5+2+2],//dqm->lambda_i4_,
				tlambda,  //p_info[6+2+2],//dqm->tlambda_,
				lambda_uv,  //p_info[7+2+2],//dqm->lambda_uv_,
				tlambda_m,  //p_info[8+2+2],//dqm->lambda_mode_,
				hls_qm1, hls_qm2, hls_qm_uv, ap_sharpen, ap_sharpen_uv, ysrc, //4096x4096
				usrc, //2048x2048
				vsrc, //2048x2048
				pout_level, //65536*512
				pout_prob, &dirty);
}
void  kernel_IntraPredLoop2_NoOut_3(
		int32_t* p_info,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob) {
#pragma HLS INTERFACE m_axi port=pout_level offset=slave bundle=gmem1 depth=65536*512/2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=p_info     offset=slave bundle=gmem0 depth=64          num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=ysrc       offset=slave bundle=gmem2 depth=4096*4096/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=usrc       offset=slave bundle=gmem3 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=vsrc       offset=slave bundle=gmem4 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_prob  offset=slave bundle=gmem5 depth=2048        num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16

#pragma HLS INTERFACE s_axilite port=p_info     bundle=control
#pragma HLS INTERFACE s_axilite port=ysrc       bundle=control
#pragma HLS INTERFACE s_axilite port=usrc       bundle=control
#pragma HLS INTERFACE s_axilite port=vsrc       bundle=control
#pragma HLS INTERFACE s_axilite port=pout_level bundle=control
#pragma HLS INTERFACE s_axilite port=pout_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int p_readinfo[64];
	memcpy(p_readinfo, p_info, 64 * sizeof(int));
	ap_uint<32> id_pic;
	ap_uint<32> mb_line;
	ap_uint<LG2_MAX_W_PIX> y_stride;
	ap_uint<LG2_MAX_W_PIX> uv_stride;
	ap_uint<LG2_MAX_W_PIX> width;
	ap_uint<LG2_MAX_W_PIX> height;
	ap_uint<LG2_MAX_NUM_MB_W> mb_w;
	ap_uint<LG2_MAX_NUM_MB_H> mb_h;
	ap_uint<WD_LMD> lambda_p16;
	ap_uint<WD_LMD> lambda_p44;
	ap_uint<WD_LMD> tlambda;
	ap_uint<WD_LMD> lambda_uv;
	ap_uint<WD_LMD> tlambda_m;
	hls_QMatrix hls_qm1, hls_qm2, hls_qm_uv;
	ap_int<WD_sharpen * 16> ap_sharpen, ap_sharpen_uv;

	//Initializing image variables, once for one picture
	{// For convenience, extend the code at top module to show all parameters used by kernel of intra-prediction
		id_pic = p_readinfo[0];//reserved for future
		mb_line = p_readinfo[1];// reserved for future, to show current line number of mb
		y_stride = p_readinfo[2];
		uv_stride = p_readinfo[3];
		width = p_readinfo[4];
		height = p_readinfo[5];
		mb_w = p_readinfo[2 + 2 + 2];
		mb_h = p_readinfo[3 + 2 + 2];
		lambda_p16 = p_readinfo[4 + 2 + 2];
		lambda_p44 = p_readinfo[5 + 2 + 2];
		tlambda = p_readinfo[6 + 2 + 2];
		lambda_uv = p_readinfo[7 + 2 + 2];
		tlambda_m = p_readinfo[8 + 2 + 2];

		hls_qm1.q_0 = p_readinfo[11 + 2];     // quantizer steps
		hls_qm1.q_n = p_readinfo[12 + 2];
		hls_qm1.iq_0 = p_readinfo[13 + 2];    // reciprocals fixed point.
		hls_qm1.iq_n = p_readinfo[14 + 2];
		hls_qm1.bias_0 = p_readinfo[15 + 2];  // rounding bias
		hls_qm1.bias_n = p_readinfo[16 + 2];

		hls_qm2.q_0 = p_readinfo[17 + 2];     // quantizer steps
		hls_qm2.q_n = p_readinfo[18 + 2];
		hls_qm2.iq_0 = p_readinfo[19 + 2];    // reciprocals fixed point.
		hls_qm2.iq_n = p_readinfo[20 + 2];
		hls_qm2.bias_0 = p_readinfo[21 + 2];  // rounding bias
		hls_qm2.bias_n = p_readinfo[22 + 2];

		hls_qm_uv.q_0 = p_readinfo[23 + 2];   // quantizer steps
		hls_qm_uv.q_n = p_readinfo[24 + 2];
		hls_qm_uv.iq_0 = p_readinfo[25 + 2];  // reciprocals fixed point.
		hls_qm_uv.iq_n = p_readinfo[26 + 2];
		hls_qm_uv.bias_0 = p_readinfo[27 + 2];// rounding bias
		hls_qm_uv.bias_n = p_readinfo[28 + 2];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen,i,WD_sharpen) = p_info[29 + 2 + i];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen_uv,i,WD_sharpen) = p_readinfo[29 + 2 + 16 + i];
	}//end of initialization
	int dirty = 0;
	TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(id_pic,  //p_info[0],
				mb_line,  //p_info[1],
				y_stride,  //p_info[2],  // ,//pic->y_stride,
				uv_stride,  //p_info[3], // ,//pic->uv_stride
				width,  //p_info[4],  // ,//pic->width
				height,  //p_info[5],  // ,//pic->height
				mb_w,  //p_info[2+2+2],///,
				mb_h,  //p_info[3+2+2],//,
				lambda_p16,  //p_info[4+2+2],//dqm->lambda_i16_,
				lambda_p44,  //p_info[5+2+2],//dqm->lambda_i4_,
				tlambda,  //p_info[6+2+2],//dqm->tlambda_,
				lambda_uv,  //p_info[7+2+2],//dqm->lambda_uv_,
				tlambda_m,  //p_info[8+2+2],//dqm->lambda_mode_,
				hls_qm1, hls_qm2, hls_qm_uv, ap_sharpen, ap_sharpen_uv, ysrc, //4096x4096
				usrc, //2048x2048
				vsrc, //2048x2048
				pout_level, //65536*512
				pout_prob, &dirty);
}
void  kernel_IntraPredLoop2_NoOut_4(
		int32_t* p_info,
		uint32_t* ysrc,
		uint32_t* usrc,
		uint32_t* vsrc,
		int32_t* pout_level,
		uint8_t* pout_prob) {
#pragma HLS INTERFACE m_axi port=pout_level offset=slave bundle=gmem1 depth=65536*512/2 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=p_info     offset=slave bundle=gmem0 depth=64          num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=ysrc       offset=slave bundle=gmem2 depth=4096*4096/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=usrc       offset=slave bundle=gmem3 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=vsrc       offset=slave bundle=gmem4 depth=2048*2048/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_prob  offset=slave bundle=gmem5 depth=2048        num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16

#pragma HLS INTERFACE s_axilite port=p_info     bundle=control
#pragma HLS INTERFACE s_axilite port=ysrc       bundle=control
#pragma HLS INTERFACE s_axilite port=usrc       bundle=control
#pragma HLS INTERFACE s_axilite port=vsrc       bundle=control
#pragma HLS INTERFACE s_axilite port=pout_level bundle=control
#pragma HLS INTERFACE s_axilite port=pout_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	int p_readinfo[64];
	memcpy(p_readinfo, p_info, 64 * sizeof(int));
	ap_uint<32> id_pic;
	ap_uint<32> mb_line;
	ap_uint<LG2_MAX_W_PIX> y_stride;
	ap_uint<LG2_MAX_W_PIX> uv_stride;
	ap_uint<LG2_MAX_W_PIX> width;
	ap_uint<LG2_MAX_W_PIX> height;
	ap_uint<LG2_MAX_NUM_MB_W> mb_w;
	ap_uint<LG2_MAX_NUM_MB_H> mb_h;
	ap_uint<WD_LMD> lambda_p16;
	ap_uint<WD_LMD> lambda_p44;
	ap_uint<WD_LMD> tlambda;
	ap_uint<WD_LMD> lambda_uv;
	ap_uint<WD_LMD> tlambda_m;
	hls_QMatrix hls_qm1, hls_qm2, hls_qm_uv;
	ap_int<WD_sharpen * 16> ap_sharpen, ap_sharpen_uv;

	//Initializing image variables, once for one picture
	{// For convenience, extend the code at top module to show all parameters used by kernel of intra-prediction
		id_pic = p_readinfo[0];//reserved for future
		mb_line = p_readinfo[1];// reserved for future, to show current line number of mb
		y_stride = p_readinfo[2];
		uv_stride = p_readinfo[3];
		width = p_readinfo[4];
		height = p_readinfo[5];
		mb_w = p_readinfo[2 + 2 + 2];
		mb_h = p_readinfo[3 + 2 + 2];
		lambda_p16 = p_readinfo[4 + 2 + 2];
		lambda_p44 = p_readinfo[5 + 2 + 2];
		tlambda = p_readinfo[6 + 2 + 2];
		lambda_uv = p_readinfo[7 + 2 + 2];
		tlambda_m = p_readinfo[8 + 2 + 2];

		hls_qm1.q_0 = p_readinfo[11 + 2];     // quantizer steps
		hls_qm1.q_n = p_readinfo[12 + 2];
		hls_qm1.iq_0 = p_readinfo[13 + 2];    // reciprocals fixed point.
		hls_qm1.iq_n = p_readinfo[14 + 2];
		hls_qm1.bias_0 = p_readinfo[15 + 2];  // rounding bias
		hls_qm1.bias_n = p_readinfo[16 + 2];

		hls_qm2.q_0 = p_readinfo[17 + 2];     // quantizer steps
		hls_qm2.q_n = p_readinfo[18 + 2];
		hls_qm2.iq_0 = p_readinfo[19 + 2];    // reciprocals fixed point.
		hls_qm2.iq_n = p_readinfo[20 + 2];
		hls_qm2.bias_0 = p_readinfo[21 + 2];  // rounding bias
		hls_qm2.bias_n = p_readinfo[22 + 2];

		hls_qm_uv.q_0 = p_readinfo[23 + 2];   // quantizer steps
		hls_qm_uv.q_n = p_readinfo[24 + 2];
		hls_qm_uv.iq_0 = p_readinfo[25 + 2];  // reciprocals fixed point.
		hls_qm_uv.iq_n = p_readinfo[26 + 2];
		hls_qm_uv.bias_0 = p_readinfo[27 + 2];// rounding bias
		hls_qm_uv.bias_n = p_readinfo[28 + 2];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen,i,WD_sharpen) = p_info[29 + 2 + i];
		for (int i = 0; i < 16; i++)
#pragma HLS UNROLL
			VCT_GET(ap_sharpen_uv,i,WD_sharpen) = p_readinfo[29 + 2 + 16 + i];
	}//end of initialization
	int dirty = 0;
	TopVp8_top_dataflow_32bit_k1NoStruct_cnt_DeepFIFO(id_pic,  //p_info[0],
				mb_line,  //p_info[1],
				y_stride,  //p_info[2],  // ,//pic->y_stride,
				uv_stride,  //p_info[3], // ,//pic->uv_stride
				width,  //p_info[4],  // ,//pic->width
				height,  //p_info[5],  // ,//pic->height
				mb_w,  //p_info[2+2+2],///,
				mb_h,  //p_info[3+2+2],//,
				lambda_p16,  //p_info[4+2+2],//dqm->lambda_i16_,
				lambda_p44,  //p_info[5+2+2],//dqm->lambda_i4_,
				tlambda,  //p_info[6+2+2],//dqm->tlambda_,
				lambda_uv,  //p_info[7+2+2],//dqm->lambda_uv_,
				tlambda_m,  //p_info[8+2+2],//dqm->lambda_mode_,
				hls_qm1, hls_qm2, hls_qm_uv, ap_sharpen, ap_sharpen_uv, ysrc, //4096x4096
				usrc, //2048x2048
				vsrc, //2048x2048
				pout_level, //65536*512
				pout_prob, &dirty);
}

void kernel_2_ArithmeticCoding_1(
		uint32_t pin_level[SIZE32_MEM_BW],
		uint8_t* pin_prob,//2048 instead of [4 * 8 * 3 * 11],
		uint32_t pout_bw[SIZE32_MEM_BW],
		uint32_t pout_ret[SIZE32_MEM_RET],
		uint32_t pout_pred[SIZE32_MEM_PRED])
{
#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pin_prob     offset=slave bundle=gmem1 depth=2048      num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_bw      offset=slave bundle=gmem2 depth=65536*384/4/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_ret     offset=slave bundle=gmem3 depth=65536*1/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_pred    offset=slave bundle=gmem4 depth=65536*16/2/4  num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb

#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
#pragma HLS INTERFACE s_axilite port=pin_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_bw   bundle=control
#pragma HLS INTERFACE s_axilite port=pout_ret  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_pred bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW

	uint8_t prob[4 * 8 * 3 * 11];
	memcpy(prob, pin_prob, sizeof(prob));

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
#pragma HLS STREAM variable=str_level_dc depth=4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_ac;
#pragma HLS STREAM variable=str_level_ac depth=8*16
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
#pragma HLS STREAM variable=str_level_uv depth=8*8
	hls::stream<ap_uint<64> > str_pred;
#pragma HLS STREAM variable=str_pred depth=64
	hls::stream<ap_uint<6> > str_ret;
#pragma HLS STREAM variable=str_ret depth=64
	hls::stream<ap_uint<1> > str_type_mb;
#pragma HLS STREAM variable=str_type_mb depth=64
	hls::stream<uint16_t> str_mb_h;
#pragma HLS STREAM variable=str_mb_h depth=64
	hls::stream<uint16_t> str_mb_w;
#pragma HLS STREAM variable=str_mb_w depth=64
	Kernel2_top_read(pin_level,

			str_level_dc, str_level_ac, str_level_uv, str_pred, str_ret,
			str_type_mb, str_mb_h, str_mb_w);
	hls::stream<ap_uint<64> > str_0_dc;
#pragma HLS STREAM    variable=str_0_dc depth=64
	hls::stream<ap_uint<64> > str_1_dc;
#pragma HLS STREAM    variable=str_1_dc depth=64
	hls::stream<ap_uint<64> > str_2_dc;
#pragma HLS STREAM    variable=str_2_dc depth=64
	hls::stream<ap_uint<64> > str_3_dc;
#pragma HLS STREAM    variable=str_3_dc depth=64
	hls::stream<ap_uint<64> > str_0_ac;
#pragma HLS STREAM    variable=str_0_ac depth=64
	hls::stream<ap_uint<64> > str_1_ac;
#pragma HLS STREAM    variable=str_1_ac depth=64
	hls::stream<ap_uint<64> > str_2_ac;
#pragma HLS STREAM    variable=str_2_ac depth=64
	hls::stream<ap_uint<64> > str_3_ac;
#pragma HLS STREAM    variable=str_3_ac depth=64
	hls::stream<ap_uint<64> > str_0_uv;
#pragma HLS STREAM    variable=str_0_uv depth=64
	hls::stream<ap_uint<64> > str_1_uv;
#pragma HLS STREAM    variable=str_1_uv depth=64
	hls::stream<ap_uint<64> > str_2_uv;
#pragma HLS STREAM    variable=str_2_uv depth=64
	hls::stream<ap_uint<64> > str_3_uv;
#pragma HLS STREAM    variable=str_3_uv depth=64
	hls::stream<ap_uint<1> > str_type_mb_out;
#pragma HLS STREAM    variable=str_type_mb_out depth=64

	hls::stream<uint16_t> str_mb_h_out;
#pragma HLS STREAM variable=str_mb_h_out depth=64
	hls::stream<uint16_t> str_mb_w_out;
#pragma HLS STREAM variable=str_mb_w_out depth=64
	kernel_2_RecordTokens_pre(str_mb_h, str_mb_w, str_type_mb, str_level_dc,
			str_level_ac, str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
			str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
			str_2_uv, str_3_uv, str_mb_h_out, str_mb_w_out, str_type_mb_out);

	hls::stream<uint16_t> str_mb_h_out2;
#pragma HLS STREAM variable=str_mb_h_out2 depth=64
	hls::stream<uint16_t> str_mb_w_out2;
#pragma HLS STREAM variable=str_mb_w_out2 depth=64
	hls::stream<ap_uint<16> > tokens_str_final;
#pragma HLS STREAM variable=tokens_str_final depth=1024
	kernel_2_CreateTokens_with_isFinal(
			str_mb_h_out, str_mb_w_out, str_type_mb_out, str_0_dc, str_1_dc,
			str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
			str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_mb_h_out2,
			str_mb_w_out2, tokens_str_final);

	uint16_t mb_h = str_mb_h_out2.read();
	uint16_t mb_w = str_mb_w_out2.read();
	VP8EmitTokens_str_hls_4stages(pout_bw, tokens_str_final, (uint8_t*) prob); //VP8EmitTokens_hls(pout_bw, &tokens, (uint8_t*)prob);
	PackStr2Mem32_t_NoLast<6, 256>(pout_ret, str_ret, mb_h * mb_w);
	PackWideStr2Mem32_t_NoLast<64, 256>(pout_pred, str_pred, mb_h * mb_w);
}
void kernel_2_ArithmeticCoding_2(
		uint32_t pin_level[SIZE32_MEM_BW],
		uint8_t* pin_prob,//2048 instead of [4 * 8 * 3 * 11],
		uint32_t pout_bw[SIZE32_MEM_BW],
		uint32_t pout_ret[SIZE32_MEM_RET],
		uint32_t pout_pred[SIZE32_MEM_PRED])
{
#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pin_prob     offset=slave bundle=gmem1 depth=2048      num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_bw      offset=slave bundle=gmem2 depth=65536*384/4/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_ret     offset=slave bundle=gmem3 depth=65536*1/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_pred    offset=slave bundle=gmem4 depth=65536*16/2/4  num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb

#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
#pragma HLS INTERFACE s_axilite port=pin_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_bw   bundle=control
#pragma HLS INTERFACE s_axilite port=pout_ret  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_pred bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW

	uint8_t prob[4 * 8 * 3 * 11];
	memcpy(prob, pin_prob, sizeof(prob));

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
#pragma HLS STREAM variable=str_level_dc depth=4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_ac;
#pragma HLS STREAM variable=str_level_ac depth=8*16
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
#pragma HLS STREAM variable=str_level_uv depth=8*8
	hls::stream<ap_uint<64> > str_pred;
#pragma HLS STREAM variable=str_pred depth=64
	hls::stream<ap_uint<6> > str_ret;
#pragma HLS STREAM variable=str_ret depth=64
	hls::stream<ap_uint<1> > str_type_mb;
#pragma HLS STREAM variable=str_type_mb depth=64
	hls::stream<uint16_t> str_mb_h;
#pragma HLS STREAM variable=str_mb_h depth=64
	hls::stream<uint16_t> str_mb_w;
#pragma HLS STREAM variable=str_mb_w depth=64
	Kernel2_top_read(pin_level,

			str_level_dc, str_level_ac, str_level_uv, str_pred, str_ret,
			str_type_mb, str_mb_h, str_mb_w);
	hls::stream<ap_uint<64> > str_0_dc;
#pragma HLS STREAM    variable=str_0_dc depth=64
	hls::stream<ap_uint<64> > str_1_dc;
#pragma HLS STREAM    variable=str_1_dc depth=64
	hls::stream<ap_uint<64> > str_2_dc;
#pragma HLS STREAM    variable=str_2_dc depth=64
	hls::stream<ap_uint<64> > str_3_dc;
#pragma HLS STREAM    variable=str_3_dc depth=64
	hls::stream<ap_uint<64> > str_0_ac;
#pragma HLS STREAM    variable=str_0_ac depth=64
	hls::stream<ap_uint<64> > str_1_ac;
#pragma HLS STREAM    variable=str_1_ac depth=64
	hls::stream<ap_uint<64> > str_2_ac;
#pragma HLS STREAM    variable=str_2_ac depth=64
	hls::stream<ap_uint<64> > str_3_ac;
#pragma HLS STREAM    variable=str_3_ac depth=64
	hls::stream<ap_uint<64> > str_0_uv;
#pragma HLS STREAM    variable=str_0_uv depth=64
	hls::stream<ap_uint<64> > str_1_uv;
#pragma HLS STREAM    variable=str_1_uv depth=64
	hls::stream<ap_uint<64> > str_2_uv;
#pragma HLS STREAM    variable=str_2_uv depth=64
	hls::stream<ap_uint<64> > str_3_uv;
#pragma HLS STREAM    variable=str_3_uv depth=64
	hls::stream<ap_uint<1> > str_type_mb_out;
#pragma HLS STREAM    variable=str_type_mb_out depth=64

	hls::stream<uint16_t> str_mb_h_out;
#pragma HLS STREAM variable=str_mb_h_out depth=64
	hls::stream<uint16_t> str_mb_w_out;
#pragma HLS STREAM variable=str_mb_w_out depth=64
	kernel_2_RecordTokens_pre(str_mb_h, str_mb_w, str_type_mb, str_level_dc,
			str_level_ac, str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
			str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
			str_2_uv, str_3_uv, str_mb_h_out, str_mb_w_out, str_type_mb_out);

	hls::stream<uint16_t> str_mb_h_out2;
#pragma HLS STREAM variable=str_mb_h_out2 depth=64
	hls::stream<uint16_t> str_mb_w_out2;
#pragma HLS STREAM variable=str_mb_w_out2 depth=64
	hls::stream<ap_uint<16> > tokens_str_final;
#pragma HLS STREAM variable=tokens_str_final depth=1024
	kernel_2_CreateTokens_with_isFinal(
			str_mb_h_out, str_mb_w_out, str_type_mb_out, str_0_dc, str_1_dc,
			str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
			str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_mb_h_out2,
			str_mb_w_out2, tokens_str_final);

	uint16_t mb_h = str_mb_h_out2.read();
	uint16_t mb_w = str_mb_w_out2.read();
	VP8EmitTokens_str_hls_4stages(pout_bw, tokens_str_final, (uint8_t*) prob); //VP8EmitTokens_hls(pout_bw, &tokens, (uint8_t*)prob);
	PackStr2Mem32_t_NoLast<6, 256>(pout_ret, str_ret, mb_h * mb_w);
	PackWideStr2Mem32_t_NoLast<64, 256>(pout_pred, str_pred, mb_h * mb_w);
}
void kernel_2_ArithmeticCoding_3(
		uint32_t pin_level[SIZE32_MEM_BW],
		uint8_t* pin_prob,//2048 instead of [4 * 8 * 3 * 11],
		uint32_t pout_bw[SIZE32_MEM_BW],
		uint32_t pout_ret[SIZE32_MEM_RET],
		uint32_t pout_pred[SIZE32_MEM_PRED])
{
#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pin_prob     offset=slave bundle=gmem1 depth=2048      num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_bw      offset=slave bundle=gmem2 depth=65536*384/4/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_ret     offset=slave bundle=gmem3 depth=65536*1/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_pred    offset=slave bundle=gmem4 depth=65536*16/2/4  num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb

#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
#pragma HLS INTERFACE s_axilite port=pin_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_bw   bundle=control
#pragma HLS INTERFACE s_axilite port=pout_ret  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_pred bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW

	uint8_t prob[4 * 8 * 3 * 11];
	memcpy(prob, pin_prob, sizeof(prob));

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
#pragma HLS STREAM variable=str_level_dc depth=4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_ac;
#pragma HLS STREAM variable=str_level_ac depth=8*16
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
#pragma HLS STREAM variable=str_level_uv depth=8*8
	hls::stream<ap_uint<64> > str_pred;
#pragma HLS STREAM variable=str_pred depth=64
	hls::stream<ap_uint<6> > str_ret;
#pragma HLS STREAM variable=str_ret depth=64
	hls::stream<ap_uint<1> > str_type_mb;
#pragma HLS STREAM variable=str_type_mb depth=64
	hls::stream<uint16_t> str_mb_h;
#pragma HLS STREAM variable=str_mb_h depth=64
	hls::stream<uint16_t> str_mb_w;
#pragma HLS STREAM variable=str_mb_w depth=64
	Kernel2_top_read(pin_level,

			str_level_dc, str_level_ac, str_level_uv, str_pred, str_ret,
			str_type_mb, str_mb_h, str_mb_w);
	hls::stream<ap_uint<64> > str_0_dc;
#pragma HLS STREAM    variable=str_0_dc depth=64
	hls::stream<ap_uint<64> > str_1_dc;
#pragma HLS STREAM    variable=str_1_dc depth=64
	hls::stream<ap_uint<64> > str_2_dc;
#pragma HLS STREAM    variable=str_2_dc depth=64
	hls::stream<ap_uint<64> > str_3_dc;
#pragma HLS STREAM    variable=str_3_dc depth=64
	hls::stream<ap_uint<64> > str_0_ac;
#pragma HLS STREAM    variable=str_0_ac depth=64
	hls::stream<ap_uint<64> > str_1_ac;
#pragma HLS STREAM    variable=str_1_ac depth=64
	hls::stream<ap_uint<64> > str_2_ac;
#pragma HLS STREAM    variable=str_2_ac depth=64
	hls::stream<ap_uint<64> > str_3_ac;
#pragma HLS STREAM    variable=str_3_ac depth=64
	hls::stream<ap_uint<64> > str_0_uv;
#pragma HLS STREAM    variable=str_0_uv depth=64
	hls::stream<ap_uint<64> > str_1_uv;
#pragma HLS STREAM    variable=str_1_uv depth=64
	hls::stream<ap_uint<64> > str_2_uv;
#pragma HLS STREAM    variable=str_2_uv depth=64
	hls::stream<ap_uint<64> > str_3_uv;
#pragma HLS STREAM    variable=str_3_uv depth=64
	hls::stream<ap_uint<1> > str_type_mb_out;
#pragma HLS STREAM    variable=str_type_mb_out depth=64

	hls::stream<uint16_t> str_mb_h_out;
#pragma HLS STREAM variable=str_mb_h_out depth=64
	hls::stream<uint16_t> str_mb_w_out;
#pragma HLS STREAM variable=str_mb_w_out depth=64
	kernel_2_RecordTokens_pre(str_mb_h, str_mb_w, str_type_mb, str_level_dc,
			str_level_ac, str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
			str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
			str_2_uv, str_3_uv, str_mb_h_out, str_mb_w_out, str_type_mb_out);

	hls::stream<uint16_t> str_mb_h_out2;
#pragma HLS STREAM variable=str_mb_h_out2 depth=64
	hls::stream<uint16_t> str_mb_w_out2;
#pragma HLS STREAM variable=str_mb_w_out2 depth=64
	hls::stream<ap_uint<16> > tokens_str_final;
#pragma HLS STREAM variable=tokens_str_final depth=1024
	kernel_2_CreateTokens_with_isFinal(
			str_mb_h_out, str_mb_w_out, str_type_mb_out, str_0_dc, str_1_dc,
			str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
			str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_mb_h_out2,
			str_mb_w_out2, tokens_str_final);

	uint16_t mb_h = str_mb_h_out2.read();
	uint16_t mb_w = str_mb_w_out2.read();
	VP8EmitTokens_str_hls_4stages(pout_bw, tokens_str_final, (uint8_t*) prob); //VP8EmitTokens_hls(pout_bw, &tokens, (uint8_t*)prob);
	PackStr2Mem32_t_NoLast<6, 256>(pout_ret, str_ret, mb_h * mb_w);
	PackWideStr2Mem32_t_NoLast<64, 256>(pout_pred, str_pred, mb_h * mb_w);
}
void kernel_2_ArithmeticCoding_4(
		uint32_t pin_level[SIZE32_MEM_BW],
		uint8_t* pin_prob,//2048 instead of [4 * 8 * 3 * 11],
		uint32_t pout_bw[SIZE32_MEM_BW],
		uint32_t pout_ret[SIZE32_MEM_RET],
		uint32_t pout_pred[SIZE32_MEM_PRED])
{
#pragma HLS INTERFACE m_axi port=pin_level    offset=slave bundle=gmem0 depth=65536*512/2   num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pin_prob     offset=slave bundle=gmem1 depth=2048      num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16
#pragma HLS INTERFACE m_axi port=pout_bw      offset=slave bundle=gmem2 depth=65536*384/4/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_ret     offset=slave bundle=gmem3 depth=65536*1/4 num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb
#pragma HLS INTERFACE m_axi port=pout_pred    offset=slave bundle=gmem4 depth=65536*16/2/4  num_read_outstanding=32 num_write_outstanding=32 max_read_burst_length=16 max_write_burst_length=16//32bb

#pragma HLS INTERFACE s_axilite port=pin_level bundle=control
#pragma HLS INTERFACE s_axilite port=pin_prob  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_bw   bundle=control
#pragma HLS INTERFACE s_axilite port=pout_ret  bundle=control
#pragma HLS INTERFACE s_axilite port=pout_pred bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW

	uint8_t prob[4 * 8 * 3 * 11];
	memcpy(prob, pin_prob, sizeof(prob));

	hls::stream<ap_int<WD_LEVEL * 16> > str_level_dc;
#pragma HLS STREAM variable=str_level_dc depth=4
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_ac;
#pragma HLS STREAM variable=str_level_ac depth=8*16
	hls::stream<ap_int<WD_LEVEL * 16> > str_level_uv;
#pragma HLS STREAM variable=str_level_uv depth=8*8
	hls::stream<ap_uint<64> > str_pred;
#pragma HLS STREAM variable=str_pred depth=64
	hls::stream<ap_uint<6> > str_ret;
#pragma HLS STREAM variable=str_ret depth=64
	hls::stream<ap_uint<1> > str_type_mb;
#pragma HLS STREAM variable=str_type_mb depth=64
	hls::stream<uint16_t> str_mb_h;
#pragma HLS STREAM variable=str_mb_h depth=64
	hls::stream<uint16_t> str_mb_w;
#pragma HLS STREAM variable=str_mb_w depth=64
	Kernel2_top_read(pin_level,

			str_level_dc, str_level_ac, str_level_uv, str_pred, str_ret,
			str_type_mb, str_mb_h, str_mb_w);
	hls::stream<ap_uint<64> > str_0_dc;
#pragma HLS STREAM    variable=str_0_dc depth=64
	hls::stream<ap_uint<64> > str_1_dc;
#pragma HLS STREAM    variable=str_1_dc depth=64
	hls::stream<ap_uint<64> > str_2_dc;
#pragma HLS STREAM    variable=str_2_dc depth=64
	hls::stream<ap_uint<64> > str_3_dc;
#pragma HLS STREAM    variable=str_3_dc depth=64
	hls::stream<ap_uint<64> > str_0_ac;
#pragma HLS STREAM    variable=str_0_ac depth=64
	hls::stream<ap_uint<64> > str_1_ac;
#pragma HLS STREAM    variable=str_1_ac depth=64
	hls::stream<ap_uint<64> > str_2_ac;
#pragma HLS STREAM    variable=str_2_ac depth=64
	hls::stream<ap_uint<64> > str_3_ac;
#pragma HLS STREAM    variable=str_3_ac depth=64
	hls::stream<ap_uint<64> > str_0_uv;
#pragma HLS STREAM    variable=str_0_uv depth=64
	hls::stream<ap_uint<64> > str_1_uv;
#pragma HLS STREAM    variable=str_1_uv depth=64
	hls::stream<ap_uint<64> > str_2_uv;
#pragma HLS STREAM    variable=str_2_uv depth=64
	hls::stream<ap_uint<64> > str_3_uv;
#pragma HLS STREAM    variable=str_3_uv depth=64
	hls::stream<ap_uint<1> > str_type_mb_out;
#pragma HLS STREAM    variable=str_type_mb_out depth=64

	hls::stream<uint16_t> str_mb_h_out;
#pragma HLS STREAM variable=str_mb_h_out depth=64
	hls::stream<uint16_t> str_mb_w_out;
#pragma HLS STREAM variable=str_mb_w_out depth=64
	kernel_2_RecordTokens_pre(str_mb_h, str_mb_w, str_type_mb, str_level_dc,
			str_level_ac, str_level_uv, str_0_dc, str_1_dc, str_2_dc, str_3_dc,
			str_0_ac, str_1_ac, str_2_ac, str_3_ac, str_0_uv, str_1_uv,
			str_2_uv, str_3_uv, str_mb_h_out, str_mb_w_out, str_type_mb_out);

	hls::stream<uint16_t> str_mb_h_out2;
#pragma HLS STREAM variable=str_mb_h_out2 depth=64
	hls::stream<uint16_t> str_mb_w_out2;
#pragma HLS STREAM variable=str_mb_w_out2 depth=64
	hls::stream<ap_uint<16> > tokens_str_final;
#pragma HLS STREAM variable=tokens_str_final depth=1024
	kernel_2_CreateTokens_with_isFinal(
			str_mb_h_out, str_mb_w_out, str_type_mb_out, str_0_dc, str_1_dc,
			str_2_dc, str_3_dc, str_0_ac, str_1_ac, str_2_ac, str_3_ac,
			str_0_uv, str_1_uv, str_2_uv, str_3_uv, str_mb_h_out2,
			str_mb_w_out2, tokens_str_final);

	uint16_t mb_h = str_mb_h_out2.read();
	uint16_t mb_w = str_mb_w_out2.read();
	VP8EmitTokens_str_hls_4stages(pout_bw, tokens_str_final, (uint8_t*) prob); //VP8EmitTokens_hls(pout_bw, &tokens, (uint8_t*)prob);
	PackStr2Mem32_t_NoLast<6, 256>(pout_ret, str_ret, mb_h * mb_w);
	PackWideStr2Mem32_t_NoLast<64, 256>(pout_pred, str_pred, mb_h * mb_w);
}
