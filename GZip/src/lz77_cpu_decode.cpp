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
