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
#include "lz77.h"

uint32_t lz77_decode(uint8_t *in, uint8_t *out, uint32_t size) 
{
    int32_t cntr = 0;
    uint32_t i = 0; 
    while (i < size) {

        if(in[i] != 255) {
            out[cntr++] = in[i];
            i++;
        }    
        else {

            //2nd byte after marker - length + offset
            uint8_t res = in[i + 1];

            uint8_t length_extract = res >> 4;
            uint8_t offset_extract = res << 4;
            uint8_t res_dec = offset_extract >> 4;
            uint16_t left_offset = res_dec << 8; 

            //3rd byte
            uint8_t temp2 = in[i + 2];
            uint16_t offset = left_offset | temp2;
           
            if(offset >= 4096){
                printf("Error >4K offset \n");
                exit(1);
            }
                
            int32_t temp = cntr - offset;

            if(temp < 0) {
                printf("\n");   
                printf("Error negative offset \n");
                exit(1);
            }
            
            if (length_extract) {
                for(int j = 0; j < length_extract + 1; j++) {
                    out[cntr++] = out[temp + j];
                }
            }else {
                out[cntr++] = 255;
            }
            
            i += 3;
        }
        
    }
    
    return cntr++;
}
