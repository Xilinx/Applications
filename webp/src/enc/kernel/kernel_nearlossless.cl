#pragma OPENCL EXTENSION cl_amd_printf:enable
typedef unsigned int       uint32_t;

#define GRX_SIZE           256
#define GRY_SIZE           16
#define PADDING_SIZE       2
#define BUFFER_SIZE_X      (GRX_SIZE + PADDING_SIZE)
#define BUFFER_SIZE_Y      (GRY_SIZE + PADDING_SIZE)

#define PIXELS_PER_ITEM           4
#define EXPAND_BUFFER_SIZE_X      (GRX_SIZE * PIXELS_PER_ITEM + PADDING_SIZE)

#define VECTOR_GRX_SIZE           128
#define VECTOR_GRY_SIZE           16
#define VECTOR_WIDTH_PADDING      2
#define VECTOR_HEIGHT_PADDING     2
#define VECTOR_BUFFER_SIZE_X      (VECTOR_GRX_SIZE + VECTOR_WIDTH_PADDING)
#define VECTOR_BUFFER_SIZE_Y      (VECTOR_GRY_SIZE + VECTOR_HEIGHT_PADDING)
#define SHIFT                     4

#define VECTOR_GRX_SIZE_4K        256
#define VECTOR_GRY_SIZE_4K        8
#define VECTOR_BUFFER_SIZE_Y_4K   (VECTOR_GRY_SIZE_4K + VECTOR_HEIGHT_PADDING)

// Computes quantized pixel value and distance from original value.
static void GetValAndDistance(int a, int initial, int bits,
                              int* const val, int* const distance) {
  const int mask = ~((1 << bits) - 1);
  *val = (initial & mask) | (initial >> (8 - bits));
  *distance = 2 * abs(a - *val);
}

// Quantizes values {a, a+(1<<bits), a-(1<<bits)} and returns the nearest one.
static int FindClosestDiscretized(int a, int bits) {
  int best_val = a, i;
  int min_distance = 256;

  for (i = -1; i <= 1; ++i) {
    int candidate, distance;
    const int val = clamp(a + i * (1 << bits), 0, 255);
    GetValAndDistance(a, val, bits, &candidate, &distance);
    if (i != 0) {
      ++distance;
    }
    // Smallest distance but favor i == 0 over i == -1 and i == 1
    // since that keeps the overall intensity more constant in the
    // images.
    if (distance < min_distance) {
      min_distance = distance;
      best_val = candidate;
    }
  }
  return best_val;
}

// Applies FindClosestDiscretized to all channels of pixel.
static uint32_t ClosestDiscretizedArgb(uint32_t a, int bits) {
  return
      (FindClosestDiscretized(a >> 24, bits) << 24) |
      (FindClosestDiscretized((a >> 16) & 0xff, bits) << 16) |
      (FindClosestDiscretized((a >> 8) & 0xff, bits) << 8) |
      (FindClosestDiscretized(a & 0xff, bits));
}

// Checks if distance between corresponding channel values of pixels a and b
// is within the given limit.
static int IsNear(uint32_t a, uint32_t b, int limit) {
  int k;
  for (k = 0; k < 4; ++k) {
    const int delta =
        (int)((a >> (k * 8)) & 0xff) - (int)((b >> (k * 8)) & 0xff);
    if (delta >= limit || delta <= -limit) {
      return 0;
    }
  }
  return 1;
}

__kernel __attribute__ ((reqd_work_group_size(GRX_SIZE, GRY_SIZE, 1)))
void nearlossless (__global uint32_t* input_argb,
                   __global uint32_t* output_argb,
                   //__local uint32_t* copy_buffer,
                   int width,
                   int height,
                   int lwidth,
                   int lheight,
                   int edgewidth,
                   int limitbits)
{
  uint32_t curPix, topPix, buttomPix, leftPix, rightPix;

  int group_startx, group_starty;
  int global_offset, local_offset;
  int localx, localy, globalx, globaly;

  int limit;
  int nearStatus;
  int row;

  event_t event_read[GRY_SIZE];
  event_t event_write[GRY_SIZE];

  __local uint32_t copy_buffer[BUFFER_SIZE_X * BUFFER_SIZE_Y];
  __local uint32_t result_buffer[BUFFER_SIZE_X * BUFFER_SIZE_Y];

  //int limit;
  limit = 1 << limitbits;

  group_startx = get_group_id(0) * GRX_SIZE;
  group_starty = get_group_id(1) * GRY_SIZE;

  // Cache the data to local memeory
  global_offset = group_starty * width + group_startx;

  __attribute__((opencl_unroll_hint))
  for (row = 0; row < BUFFER_SIZE_Y; row++) {
    event_read[row] = async_work_group_copy(copy_buffer + row * lwidth, 
                                            input_argb + global_offset + row * width, 
                                            BUFFER_SIZE_X, 0);
  }
  wait_group_events(BUFFER_SIZE_Y, event_read);

  if (get_group_id(0) == get_num_groups(0) - 1 || get_group_id(1) == get_num_groups(1) - 1) {
    __attribute__((xcl_pipeline_loop))
    for (int i = 0; i < BUFFER_SIZE_Y; i++) {
      for (int j = 0; j < BUFFER_SIZE_X; j ++) {
        result_buffer[i * BUFFER_SIZE_X + j] = copy_buffer[i * BUFFER_SIZE_X +j];
      }
    }
  }
  //barrier(CLK_LOCAL_MEM_FENCE);

  __attribute__((xcl_pipeline_workitems)) {
    group_startx = get_group_id(0) * GRX_SIZE;
    group_starty = get_group_id(1) * GRY_SIZE;

    localx = get_local_id(0);
    localy = get_local_id(1);

    globalx = group_startx + localx;
    globaly = group_starty + localy;

    if (globalx < (width - PADDING_SIZE) && globaly < (height - PADDING_SIZE)) {
      curPix    = copy_buffer[(localy + 1) * lwidth + (localx + 1)];
      topPix    = copy_buffer[localy * lwidth + (localx + 1)];
      buttomPix = copy_buffer[(localy + 2) * lwidth + (localx + 1)];
      leftPix   = copy_buffer[(localy + 1) * lwidth + localx];
      rightPix  = copy_buffer[(localy + 1) * lwidth + (localx + 2)];

      nearStatus = (IsNear(curPix, leftPix, limit) &&
                    IsNear(curPix, rightPix, limit) &&
                    IsNear(curPix, topPix, limit) &&
                    IsNear(curPix, buttomPix, limit));

      if (!nearStatus) {
        result_buffer[(localy + 1) * lwidth + (localx + 1)] = ClosestDiscretizedArgb(curPix, limitbits);
      } else {
        result_buffer[(localy + 1) * lwidth + (localx + 1)] = curPix;
      }
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  if (get_group_id(0) == get_num_groups(0) - 1) {
    __attribute__((opencl_unroll_hint))
    for (row = 1; row < BUFFER_SIZE_Y - 1; row++) {
      async_work_group_copy(output_argb + (global_offset + 1) + row * width, 
                            result_buffer + 1 + row * lwidth, 
                            edgewidth, 0);
    }
  } else {
    __attribute__((opencl_unroll_hint))
    for (row = 1; row < BUFFER_SIZE_Y - 1; row++) {
      async_work_group_copy(output_argb + (global_offset + 1) + row * width, 
                            result_buffer + 1 + row * lwidth, 
                            GRX_SIZE, 0);
    }
  }
}

__kernel __attribute__ ((reqd_work_group_size(GRX_SIZE, GRY_SIZE, 1)))
void nearlossless_multipixel (__global uint32_t* input_argb,
                              __global uint32_t* output_argb,
                              //__local uint32_t* copy_buffer,
                              int width,
                              int height,
                              int lwidth,
                              int lheight,
                              int edgewidth,
                              int limitbits)
{
  uint32_t curPix, topPix, buttomPix, leftPix, rightPix;

  int group_startx, group_starty;
  int global_offset, local_offset;
  int localx, localy, globalx, globaly;

  int limit;
  int nearStatus;
  int row;

  event_t event_read[GRY_SIZE];

  __local uint32_t copy_buffer[EXPAND_BUFFER_SIZE_X * BUFFER_SIZE_Y];
  __local uint32_t result_buffer[EXPAND_BUFFER_SIZE_X * BUFFER_SIZE_Y];

  //int limit;
  limit = 1 << limitbits;

  group_startx = get_group_id(0) * GRX_SIZE * PIXELS_PER_ITEM;
  group_starty = get_group_id(1) * GRY_SIZE;

  // Cache the data to local memeory
  global_offset = group_starty * width + group_startx;

  __attribute__((opencl_unroll_hint))
  for (row = 0; row < BUFFER_SIZE_Y; row++) {
    event_read[row] = async_work_group_copy(copy_buffer + row * lwidth, 
                                            input_argb + global_offset + row * width, 
                                            EXPAND_BUFFER_SIZE_X, 0);
  }
  wait_group_events(BUFFER_SIZE_Y, event_read);

  if (get_group_id(0) == get_num_groups(0) - 1 || get_group_id(1) == get_num_groups(1) - 1) {
    __attribute__((xcl_pipeline_loop))
    for (int i = 0; i < BUFFER_SIZE_Y; i++) {
      for (int j = 0; j < EXPAND_BUFFER_SIZE_X; j ++) {
        result_buffer[i * EXPAND_BUFFER_SIZE_X + j] = copy_buffer[i * EXPAND_BUFFER_SIZE_X +j];
      }
    }
  }
  //barrier(CLK_LOCAL_MEM_FENCE);

  __attribute__((xcl_pipeline_workitems)) {
    group_startx = get_group_id(0) * GRX_SIZE * PIXELS_PER_ITEM;
    group_starty = get_group_id(1) * GRY_SIZE;

    localx = get_local_id(0) * PIXELS_PER_ITEM;
    localy = get_local_id(1);

    globalx = group_startx + localx;
    globaly = group_starty + localy;

    int global_index;
    int local_index;
    for (int i = 0; i < PIXELS_PER_ITEM; i++) {
      global_index = globalx + i;
      local_index  = localx + i;
      if (global_index < (width - PADDING_SIZE) && globaly < (height - PADDING_SIZE)) {
        curPix    = copy_buffer[(localy + 1) * lwidth + (local_index + 1)];
        topPix    = copy_buffer[localy * lwidth + (local_index + 1)];
        buttomPix = copy_buffer[(localy + 2) * lwidth + (local_index + 1)];
        leftPix   = copy_buffer[(localy + 1) * lwidth + local_index];
        rightPix  = copy_buffer[(localy + 1) * lwidth + (local_index + 2)];

        nearStatus = (IsNear(curPix, leftPix, limit) &&
                      IsNear(curPix, rightPix, limit) &&
                      IsNear(curPix, topPix, limit) &&
                      IsNear(curPix, buttomPix, limit));

        if (!nearStatus) {
          result_buffer[(localy + 1) * lwidth + (local_index + 1)] = ClosestDiscretizedArgb(curPix, limitbits);
        } else {
          result_buffer[(localy + 1) * lwidth + (local_index + 1)] = curPix;
        }
      }
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  if (get_group_id(0) == get_num_groups(0) - 1) {
    __attribute__((opencl_unroll_hint))
    for (row = 1; row < BUFFER_SIZE_Y - 1; row++) {
      async_work_group_copy(output_argb + (global_offset + 1) + row * width, 
                            result_buffer + 1 + row * lwidth, 
                            edgewidth, 0);
    }
  } else {
    __attribute__((opencl_unroll_hint))
    for (row = 1; row < BUFFER_SIZE_Y - 1; row++) {
      async_work_group_copy(output_argb + (global_offset + 1) + row * width, 
                            result_buffer + 1 + row * lwidth, 
                            EXPAND_BUFFER_SIZE_X, 0);
    }
  }
}

// Computes quantized pixel value and distance from original value.
static void GetValAndDistanceVec(int16 a, int16 initial, int bits,
                              int16* const val, uint16* const distance) {
  const int mask = ~((1 << bits) - 1);
  *val = (initial & mask) | (initial >> (8 - bits));
  *distance = 2 * abs(a - *val);
}

// Quantizes values {a, a+(1<<bits), a-(1<<bits)} and returns the nearest one.
static int16 FindClosestDiscretizedVec(int16 a, int bits) {
  int i;
  int16 best_val = a;
  int16 min_distance = (int16)(256);

  for (i = -1; i <= 1; ++i) {
    int16 candidate;
    uint16 distance;
    const int16 val = clamp(a + i * (1 << bits), 0, 255);
    //GetValAndDistanceVec(a, val, bits, &candidate, &distance);
    const int mask = ~((1 << bits) - 1);
    candidate = (val & mask) | (val >> (8 - bits));
    distance = 2 * abs(a - candidate);
    if (i != 0) {
      distance += 1;
    }
    // Smallest distance but favor i == 0 over i == -1 and i == 1
    // since that keeps the overall intensity more constant in the
    // images.
    if (distance.s0 < min_distance.s0) {
      min_distance.s0 = distance.s0;
      best_val.s0 = candidate.s0;
    }
    if (distance.s1 < min_distance.s1) {
      min_distance.s1 = distance.s1;
      best_val.s1 = candidate.s1;
    }
    if (distance.s2 < min_distance.s2) {
      min_distance.s2 = distance.s2;
      best_val.s2 = candidate.s2;
    }
    if (distance.s3 < min_distance.s3) {
      min_distance.s3 = distance.s3;
      best_val.s3 = candidate.s3;
    }
    if (distance.s4 < min_distance.s4) {
      min_distance.s4 = distance.s4;
      best_val.s4 = candidate.s4;
    }
    if (distance.s5 < min_distance.s5) {
      min_distance.s5 = distance.s5;
      best_val.s5 = candidate.s5;
    }
    if (distance.s6 < min_distance.s6) {
      min_distance.s6 = distance.s6;
      best_val.s6 = candidate.s6;
    }
    if (distance.s7 < min_distance.s7) {
      min_distance.s7 = distance.s7;
      best_val.s7 = candidate.s7;
    }
    if (distance.s8 < min_distance.s8) {
      min_distance.s8 = distance.s8;
      best_val.s8 = candidate.s8;
    }
    if (distance.s9 < min_distance.s9) {
      min_distance.s9 = distance.s9;
      best_val.s9 = candidate.s9;
    }
    if (distance.sa < min_distance.sa) {
      min_distance.sa = distance.sa;
      best_val.sa = candidate.sa;
    }
    if (distance.sb < min_distance.sb) {
      min_distance.sb = distance.sb;
      best_val.sb = candidate.sb;
    }
    if (distance.sc < min_distance.sc) {
      min_distance.sc = distance.sc;
      best_val.sc = candidate.sc;
    }
    if (distance.sd < min_distance.sd) {
      min_distance.sd = distance.sd;
      best_val.sd = candidate.sd;
    }
    if (distance.se < min_distance.se) {
      min_distance.se = distance.se;
      best_val.se = candidate.se;
    }
    if (distance.sf < min_distance.sf) {
      min_distance.sf = distance.sf;
      best_val.sf = candidate.sf;
    }
  }
  return best_val;
}

// Applies FindClosestDiscretizedVec to all channels of pixel.
static uint16 ClosestDiscretizedArgbVec(uint16 a, int bits) {
  return
      convert_uint16(FindClosestDiscretizedVec(convert_int16(a >> 24), bits) << 24) |
      (FindClosestDiscretizedVec(convert_int16((a >> 16) & 0xff), bits) << 16) |
      (FindClosestDiscretizedVec(convert_int16((a >> 8) & 0xff), bits) << 8) |
      (FindClosestDiscretizedVec(convert_int16(a & 0xff), bits));
}

// Checks if distance between corresponding channel values of pixels a and b
// is within the given limit.
static int16 IsNearVec(uint16 a, uint16 b, int limit) {
  int k;
  int16 isnear = (int16)(1);
  for (k = 0; k < 4; ++k) {
    const int16 delta =
        (convert_int16)((a >> (k * 8)) & 0xff) - (convert_int16)((b >> (k * 8)) & 0xff);
    if (delta.s0 >= limit || delta.s0 <= -limit) {
      isnear.s0 = 0;
    }
    if (delta.s1 >= limit || delta.s1 <= -limit) {
      isnear.s1 = 0;
    }
    if (delta.s2 >= limit || delta.s2 <= -limit) {
      isnear.s2 = 0;
    }
    if (delta.s3 >= limit || delta.s3 <= -limit) {
      isnear.s3 = 0;
    }
    if (delta.s4 >= limit || delta.s4 <= -limit) {
      isnear.s4 = 0;
    }
    if (delta.s5 >= limit || delta.s5 <= -limit) {
      isnear.s5 = 0;
    }
    if (delta.s6 >= limit || delta.s6 <= -limit) {
      isnear.s6 = 0;
    }
    if (delta.s7 >= limit || delta.s7 <= -limit) {
      isnear.s7 = 0;
    }
    if (delta.s8 >= limit || delta.s8 <= -limit) {
      isnear.s8 = 0;
    }
    if (delta.s9 >= limit || delta.s9 <= -limit) {
      isnear.s9 = 0;
    }
    if (delta.sa >= limit || delta.sa <= -limit) {
      isnear.sa = 0;
    }
    if (delta.sb >= limit || delta.sb <= -limit) {
      isnear.sb = 0;
    }
    if (delta.sc >= limit || delta.sc <= -limit) {
      isnear.sc = 0;
    }
    if (delta.sd >= limit || delta.sd <= -limit) {
      isnear.sd = 0;
    }
    if (delta.se >= limit || delta.se <= -limit) {
      isnear.se = 0;
    }
    if (delta.sf >= limit || delta.sf <= -limit) {
      isnear.sf = 0;
    }
  }
  return isnear;
}

__kernel __attribute__ ((reqd_work_group_size(VECTOR_GRX_SIZE, VECTOR_GRY_SIZE, 1)))
void nearlossless_vector (__global uint16* input_argb,
                          __global uint16* output_argb,
                          int width,
                          int height,
                          int lwidth,
                          int lheight,
                          int edgewidth,
                          int limitbits)
{
  uint16 curPix;
  uint16 topPix;
  uint16 buttomPix;
  uint16 leftPix;
  uint16 rightPix;
  uint16 result;
  uint16 closestargb;
  int16  nearStatus;

  int localx, localy;

  int limit;
  int vector_width;

  event_t event_read;

  __local uint16 copy_buffer[VECTOR_GRX_SIZE * VECTOR_BUFFER_SIZE_Y];
  __local uint16 result_buffer[VECTOR_GRX_SIZE * VECTOR_BUFFER_SIZE_Y];

  //int limit;
  limit = 1 << limitbits;

  vector_width = width >> SHIFT;

  event_read = async_work_group_copy(copy_buffer, 
                                     input_argb + VECTOR_GRY_SIZE * vector_width * get_group_id(1), 
                                     VECTOR_BUFFER_SIZE_Y * vector_width, 0);
  wait_group_events(1, &event_read);

  __attribute__((xcl_pipeline_workitems)) {
    localx = get_local_id(0);
    localy = get_local_id(1);

    if (localx < vector_width) {
      curPix    = copy_buffer[(localy + 1) * vector_width + localx];
      topPix    = copy_buffer[localy * vector_width + localx];
      buttomPix = copy_buffer[(localy + 2) * vector_width + localx];
      leftPix   = copy_buffer[(localy + 1) * vector_width + (localx - 1)];
      rightPix  = copy_buffer[(localy + 1) * vector_width + (localx + 1)];

      leftPix.s0 = leftPix.sf;
      leftPix.s123456789abcdef = curPix.s0123456789abcde;

      rightPix.sf = rightPix.s0;
      rightPix.s0123456789abcde = curPix.s123456789abcdef;

      nearStatus = (IsNearVec(curPix, rightPix, limit) &&
                    IsNearVec(curPix, leftPix, limit) &&
                    IsNearVec(curPix, topPix, limit) &&
                    IsNearVec(curPix, buttomPix, limit));

      closestargb = ClosestDiscretizedArgbVec(curPix, limitbits);

      result = select(closestargb, curPix, nearStatus);

      if (localx == 0) {
        result.s0 = curPix.s0;
      }

      if (localx == vector_width - 1) {
        result.sf = curPix.sf;
      }

      if (get_global_id(1) == height - 2) {
        result = curPix;
      }

      result_buffer[(localy + 1) * vector_width + localx] = result;
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  async_work_group_copy(output_argb + vector_width + VECTOR_GRY_SIZE * vector_width * get_group_id(1), 
                        result_buffer + vector_width, 
                        VECTOR_GRY_SIZE * vector_width, 0);
}

__kernel __attribute__ ((reqd_work_group_size(VECTOR_GRX_SIZE_4K, VECTOR_GRY_SIZE_4K, 1)))
void nearlossless_vector_4k (__global uint16* input_argb,
                          __global uint16* output_argb,
                          int width,
                          int height,
                          int lwidth,
                          int lheight,
                          int edgewidth,
                          int limitbits)
{
  uint16 curPix;
  uint16 topPix;
  uint16 buttomPix;
  uint16 leftPix;
  uint16 rightPix;
  uint16 result;
  uint16 closestargb;
  int16  nearStatus;

  int localx, localy;

  int limit;
  int vector_width;

  event_t event_read;

  __local uint16 copy_buffer[VECTOR_GRX_SIZE_4K * VECTOR_BUFFER_SIZE_Y_4K];
  __local uint16 result_buffer[VECTOR_GRX_SIZE_4K * VECTOR_BUFFER_SIZE_Y_4K];

  //int limit;
  limit = 1 << limitbits;

  vector_width = width >> SHIFT;

  event_read = async_work_group_copy(copy_buffer, 
                                     input_argb + VECTOR_GRY_SIZE_4K * vector_width * get_group_id(1), 
                                     VECTOR_BUFFER_SIZE_Y_4K * vector_width, 0);
  wait_group_events(1, &event_read);

  __attribute__((xcl_pipeline_workitems)) {
    localx = get_local_id(0);
    localy = get_local_id(1);

    if (localx < vector_width) {
      curPix    = copy_buffer[(localy + 1) * vector_width + localx];
      topPix    = copy_buffer[localy * vector_width + localx];
      buttomPix = copy_buffer[(localy + 2) * vector_width + localx];
      leftPix   = copy_buffer[(localy + 1) * vector_width + (localx - 1)];
      rightPix  = copy_buffer[(localy + 1) * vector_width + (localx + 1)];

      leftPix.s0 = leftPix.sf;
      leftPix.s123456789abcdef = curPix.s0123456789abcde;

      rightPix.sf = rightPix.s0;
      rightPix.s0123456789abcde = curPix.s123456789abcdef;

      nearStatus = (IsNearVec(curPix, rightPix, limit) &&
                    IsNearVec(curPix, leftPix, limit) &&
                    IsNearVec(curPix, topPix, limit) &&
                    IsNearVec(curPix, buttomPix, limit));

      closestargb = ClosestDiscretizedArgbVec(curPix, limitbits);

      result = select(closestargb, curPix, nearStatus);

      if (localx == 0) {
        result.s0 = curPix.s0;
      }

      if (localx == vector_width - 1) {
        result.sf = curPix.sf;
      }

      if (get_global_id(1) == height - 2) {
        result = curPix;
      }

      result_buffer[(localy + 1) * vector_width + localx] = result;
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  async_work_group_copy(output_argb + vector_width + VECTOR_GRY_SIZE_4K * vector_width * get_group_id(1), 
                        result_buffer + vector_width, 
                        VECTOR_GRY_SIZE_4K * vector_width, 0);
}