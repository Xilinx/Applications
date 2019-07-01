#pragma once

#include "xil_lzma.h"
#include <cstdint>

class lzma {

private:
    static lzma* instance;
    xil_lzma* list[C_COMPUTE_UNIT];
    int list_count;
    lzma();

public:
    static lzma* getinstance();

    int xlzma_init();

    int xlzma_set_dict_size(int xlzma_handle, uint64_t dict_size_bytes);

    uint64_t xlzma_bound(uint64_t input_size);

    int xlzma_compress(int xlzma_handle, char* input, uint64_t input_size, char* output, uint64_t output_buf_size);

    int xlzma_close(int xlzma_handle);
    ~lzma();
};

