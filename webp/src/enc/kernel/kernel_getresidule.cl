#pragma OPENCL EXTENSION cl_amd_printf:enable

//------------------------------------------------------------------------------
#define GRP_X                                  (256)
#define GRP_Y                                  (16)
#define GRP_X_2                                (GRP_X + 2)
#define GRP_Y_1                                (GRP_Y + 1)
#define GRP_X_Y_K14                            (GRP_X * GRP_Y * 14)
#define GRP_X_K14                              (GRP_X * 14)
#define ARGB_BLACK                             (0xff000000)
#define kMaskAlpha                             (0xff000000)
#define kNumPredModes                          (14)

typedef unsigned int                           uint32_t;
//------------------------------------------------------------------------------
// Image transforms.

static inline uint32_t Average2(uint32_t a0, uint32_t a1) {
  return (((a0 ^ a1) & 0xfefefefeu) >> 1) + (a0 & a1);
}

static inline uint32_t Average3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return Average2(Average2(a0, a2), a1);
}

static inline uint32_t Average4(uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
  return Average2(Average2(a0, a1), Average2(a2, a3));
}

static inline uint32_t Clip255(uint32_t a) {
  if (a < 256) {
    return a;
  }
  // return 0, when a is a negative integer.
  // return 255, when a is positive.
  return ~a >> 24;
}

static inline int AddSubtractComponentFull(int a, int b, int c) {
  return Clip255(a + b - c);
}

static inline uint32_t ClampedAddSubtractFull(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const int a = AddSubtractComponentFull(c0 >> 24, c1 >> 24, c2 >> 24);
  const int r = AddSubtractComponentFull((c0 >> 16) & 0xff,
                                         (c1 >> 16) & 0xff,
                                         (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentFull((c0 >> 8) & 0xff,
                                         (c1 >> 8) & 0xff,
                                         (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentFull(c0 & 0xff, c1 & 0xff, c2 & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

static inline int AddSubtractComponentHalf(int a, int b) {
  return Clip255(a + (a - b) / 2);
}

static inline uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const uint32_t ave = Average2(c0, c1);
  const int a = AddSubtractComponentHalf(ave >> 24, c2 >> 24);
  const int r = AddSubtractComponentHalf((ave >> 16) & 0xff, (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentHalf((ave >> 8) & 0xff, (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentHalf((ave >> 0) & 0xff, (c2 >> 0) & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

static inline int Sub3(int a, int b, int c) {
  const int pb = b - c;
  const int pa = a - c;
  return abs(pb) - abs(pa);
}

static inline uint32_t Select(uint32_t a, uint32_t b, uint32_t c) {
  const int pa_minus_pb =
      Sub3((a >> 24)       , (b >> 24)       , (c >> 24)       ) +
      Sub3((a >> 16) & 0xff, (b >> 16) & 0xff, (c >> 16) & 0xff) +
      Sub3((a >>  8) & 0xff, (b >>  8) & 0xff, (c >>  8) & 0xff) +
      Sub3((a      ) & 0xff, (b      ) & 0xff, (c      ) & 0xff);
  return (pa_minus_pb <= 0) ? a : b;
}

//------------------------------------------------------------------------------

// In-place difference of each component with mod 256.
static inline uint32_t VP8LSubPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green =
      0x00ff00ffu + (a & 0xff00ff00u) - (b & 0xff00ff00u);
  const uint32_t red_and_blue =
      0xff00ff00u + (a & 0x00ff00ffu) - (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

//------------------------------------------------------------------------------
// kernel func that get residule info for whole frame
__kernel __attribute__ ((xcl_max_work_group_size(GRP_X, GRP_Y, 1)))
void getresiduleforframe(const int width, const int height,
                         __global const uint32_t* const argb,
                         //__local uint32_t* const local_argb,
                         __global uint32_t* const residual,
                         //__local uint32_t* const local_residual,
						  int exact) {
  __local uint32_t local_argb[GRP_X_2 * GRP_Y_1];
  __local uint32_t local_residual[GRP_X_Y_K14];
  __global uint32_t* g_argb;
  int group_x, group_y, global_x, global_y, local_x, local_y;
  int global_width, global_offset, local_offset;
  int copy_width, copy_height;
  int row = 0;
  event_t evt[GRP_Y_1];

  uint32_t predict, res;
  uint32_t current_pixel, left, left_top, top, right_top, tmp;
  __local uint32_t* p_current_pixel;
  __local uint32_t* p_upper_pixel;
  __local uint32_t* p_local_res;
  int mode;

  // copy group argb value
  g_argb = argb + width + 1;
  group_x = get_group_id(0);
  group_y = get_group_id(1);
  global_offset = GRP_X * group_x + group_y * GRP_Y * width - (width + 1); //move up one row and left one col
  copy_width = GRP_X_2;
  copy_height = GRP_Y_1;
  __attribute__((opencl_unroll_hint))
  for(row = 0; row < copy_height; row++) {
    evt[row] = async_work_group_copy(local_argb + row * copy_width, g_argb + global_offset + row * width, copy_width, 0);
    //prefetch(g_argb + global_offset + (row + 1) * width, copy_width);
  }
  wait_group_events(GRP_Y_1, evt);

__attribute__((xcl_pipeline_workitems)) {

  global_x = get_global_id(0);
  global_y = get_global_id(1);
  local_x = get_local_id(0);
  local_y = get_local_id(1);

  local_offset = local_x + 1 + (local_y + 1) * GRP_X_2;          //for get argb offset
  p_current_pixel = local_argb + local_offset;
  current_pixel = *p_current_pixel;
  p_upper_pixel = local_argb + local_offset - GRP_X_2;

  global_offset = local_y * GRP_X_K14 + local_x * kNumPredModes; //for save res
  p_local_res = local_residual + global_offset;

  if (global_y == 0 || global_x == 0) {
    if (global_x == 0 && global_y == 0) {
      tmp = ARGB_BLACK;
    } else if ( global_y == 0) {
      tmp = p_current_pixel[-1];
    } else {
      tmp = p_upper_pixel[0];
    }
    res = VP8LSubPixels(current_pixel, tmp);
    if (!exact && (current_pixel & kMaskAlpha) == 0) {
      res &= kMaskAlpha;
    }
    p_local_res[13] = p_local_res[12] = p_local_res[11] = p_local_res[10] = p_local_res[9] = p_local_res[8] = p_local_res[7] = p_local_res[6] = p_local_res[5] = p_local_res[4] = p_local_res[3] = p_local_res[2] = p_local_res[1] = p_local_res[0] = res;
  } else {
    left = p_current_pixel[-1];
    top = p_upper_pixel[0];
    left_top = p_upper_pixel[-1];
    right_top = p_upper_pixel[1];
    __attribute__((opencl_unroll_hint))
    for(mode = 0; mode < kNumPredModes; mode++) {
      switch(mode) {
        case 0: predict = ARGB_BLACK; break;
        case 1: predict = left; break;
        case 2: predict = top; break;
        case 3: predict = right_top; break;
        case 4: predict = left_top; break;
        case 5: predict = Average3(left, top, right_top); break;
        case 6: predict = Average2(left, left_top); break;
        case 7: predict = Average2(left, top); break;
        case 8: predict = Average2(left_top, top); break;
        case 9: predict = Average2(top, right_top); break;
        case 10: predict = Average4(left, left_top, top, right_top); break;
        case 11: predict = Select(top, left, left_top); break;
        case 12: predict = ClampedAddSubtractFull(left, top, left_top); break;
        case 13: predict = ClampedAddSubtractHalf(left, top, left_top); break;
      }
      res = VP8LSubPixels(current_pixel, predict);
      if (!exact && (current_pixel & kMaskAlpha) == 0) {
        res &= kMaskAlpha;
      }
      p_local_res[mode] = res;
    }
  }

End:
  //copy res to residual
  global_width = get_global_size(0);
  global_offset =  ( (global_width / GRP_X) * group_y + group_x) * GRP_X_Y_K14;
}
  barrier(CLK_LOCAL_MEM_FENCE);
  async_work_group_copy(residual + global_offset, local_residual, GRP_X_Y_K14, 0);
  return;
}
