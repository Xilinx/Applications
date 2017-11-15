#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <assert.h>
#include <stdint.h>
#include <vector>
#include <queue>

/* Function to reverse bits of num */
uint8_t reverseBits(uint8_t num)
{
    uint8_t  NO_OF_BITS = sizeof(num) * 8;
    uint8_t reverse_num = 0, i, temp;
 
    for (i = 0; i < NO_OF_BITS; i++)
    {
        temp = (num & (1 << i));
        if(temp)
            reverse_num |= (1 << ((NO_OF_BITS - 1) - i));
    }
  
    return reverse_num;
}


int main(int argc, char *argv[])
{
    FILE *inFile  = NULL;
    FILE *outFile = NULL;    
    
    std::string inFile_name = argv[1];   
    std::string outFile_name = inFile_name;
    outFile_name = outFile_name + "encode.deflate";
 
    inFile  = fopen(inFile_name.c_str(), "rb");
    outFile = fopen(outFile_name.c_str(), "wb");

    // Error checking
    assert(inFile && "Encode inFile fails to open");
    assert(outFile && "Encode outFile fails to open");
    
    // Find file size
    fseek(inFile, 0, SEEK_END);
    long input_size = ftell(inFile);
    rewind(inFile);

    std::vector<uint8_t> in(input_size);
    std::vector<uint8_t> out(input_size * 2); 
    
    uint8_t c = 0;
    for(int i = 0; i < input_size; i++){
        c = getc(inFile);
        in[i] = c;
    } 

    int i = 0;
    int cntr = 0;
    uint8_t byte_flag = 0;
    int output_cntr = 0;
    
    // Data structure to hold current
    // Elements 
    std::queue<uint8_t> lcl_input;
   
    while(i < input_size){

        
        if(cntr == 8) {
            uint8_t rev = reverseBits(byte_flag);
            out[output_cntr++] = rev;
            while(!lcl_input.empty()) {
                uint8_t temp = lcl_input.front();
                out[output_cntr++] = temp;
                lcl_input.pop();
            }
            cntr = 0;
            byte_flag = 0;
        }

        if(in[i] != 255) {
            cntr++;
            lcl_input.push(in[i]); i++;
        }
        else {
            cntr++;
            
            if(in[i + 1] == 0) {
                lcl_input.push(in[i]); i++;
                continue;       
            }
            else {
                
                printf("length %d offset %d \n", in[i+1], in[i+2]);
                // 2nd byte
                uint8_t res = in[i+1]; 
                uint8_t length_extract = res >> 4;
                uint8_t offset_extract = res << 4;
                uint8_t res_dec = offset_extract >> 4;
                uint16_t left_offset = res_dec << 8; 
                // 3rd byte
                uint8_t temp2 = in[i + 2]; 
                printf("tmp2 %d \n", temp2);
                uint16_t offset = left_offset | temp2;
               
                printf("length_extract %d offset %d \n", length_extract + 1, offset);
                lcl_input.push(length_extract + 1);
                lcl_input.push(offset);
                uint8_t num = 1;
                uint8_t shift = cntr;
                uint8_t temp1 = num << shift;
                        byte_flag |= temp1;           
                i+=3;
            }
        }// Else part
         

    }//While loop ends here
    
    if(cntr < 8) {
        uint8_t rev = reverseBits(byte_flag);
        out[output_cntr++] = rev;
        while(!lcl_input.empty()) {
            uint8_t temp = lcl_input.front();
            out[output_cntr++] = temp;
            lcl_input.pop();
        }
    }

    for(int i = 0; i < output_cntr; i++) {
        printf("%d-%c \n", out[i], out[i]);
        putc(out[i], outFile);
    }

    fclose(inFile);
    fclose(outFile);    
}
