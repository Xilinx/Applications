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
#define VEC 8 

#ifndef PARALLEL_BLOCK
#define PARALLEL_BLOCK 8
#endif

#define GMEM_DWIDTH 512
#define GMEM_BURST_SIZE 16
#define BLOCK_PARITION 1024
#define MARKER 255
#define MAX_LIT_COUNT 4096 
const int c_gmem_burst_size = (2*GMEM_BURST_SIZE);
const int c_size_stream_depth = 8;
const int max_literal_count = MAX_LIT_COUNT;

