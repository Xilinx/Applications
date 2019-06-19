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

