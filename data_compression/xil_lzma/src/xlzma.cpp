#include "xlzma.h"
#include "lzma.h"

int xlzma_init() {
    return lzma::getinstance()->xlzma_init();
}

// set the dict size of LZMA compression, in bytes
// return the actual dict size set if succeeded, -1 otherwise
int xlzma_set_dict_size(int xlzma_handle, uint64_t dict_size_bytes) {
    return lzma::getinstance()->xlzma_set_dict_size(xlzma_handle,dict_size_bytes);
}

// return min size of output buffer needed
// if error return 0 
uint64_t xlzma_bound(uint64_t input_size) {
    return lzma::getinstance()->xlzma_bound(input_size);
}

// encode the input buffer, and write it into the output buffer
// return the encoded length if succeeded, return an ERROR code otherwise, for example
// #define XLZMA_FAILED_UNKNOWN -1
// #define XLZMA_FAILED_INPUT_TOO_LARGE -2
// #define XLZMA_FAILED_OUTPUT_TOO_SMALL -3
uint64_t xlzma_compress(int xlzma_handle, char* input, uint64_t input_size, char* output, uint64_t output_buf_size) {
    return lzma::getinstance()->xlzma_compress(xlzma_handle,input,input_size,output,output_buf_size);
}

// close the xlzma stream
// return 0 if succeeded, -1 otherwise
int xlzma_close(int xlzma_handle) {
    return lzma::getinstance()->xlzma_close(xlzma_handle);
}

