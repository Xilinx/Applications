#define BPS                             8   // this is the common stride for enc/dec
#define BPS_IN                          4
#define BPS_I4                          2

#define Y_OFF_ENC                       (0)
#define U_OFF_ENC                       (0)
#define V_OFF_ENC                       (2)

#define MAX_ALPHA                       255              // 8b of precision for susceptibilities.
#define ALPHA_SCALE                     (2 * MAX_ALPHA)  // scaling factor for alpha.
#define DEFAULT_ALPHA                   (-1)
#define IS_BETTER_ALPHA(alpha, best_alpha) ((alpha) > (best_alpha))

#define MAX_INTRA16_MODE                2
#define MAX_INTRA4_MODE                 2
#define MAX_UV_MODE                     2

#define I16DC16                         (0)
#define I16TM16                         (I16DC16 + 4)

// chroma 8x8, two U/V blocks side by side (hence: 16x8 each)
#define C8DC8                           (0)
#define C8TM8                           (C8DC8 + 1 * 4)

// intra 4x4
#define I4DC4                           (0)
#define I4TM4                           (I4DC4 + 1)

#define MAX_COEFF_THRESH                31   // size of histogram used by CollectHistogram.

#define ANALYZE_GRX_SIZE                240

#define LARGEST_Y_STRIDE                3840
#define LARGEST_UV_STRIDE               1920
#define LARGEST_MB_W                    240
#define LARGEST_MB_H                    135
#define LARGEST_PREDS_W                 (4 * LARGEST_MB_W + 1)
#define LARGEST_PREDS_H                 (4 * LARGEST_MB_H + 1)

#define VECTOR_Y_STRIDE                 960
#define VECTOR_UV_STRIDE                480

#define NULL                            0

typedef signed   char int8_t;
typedef unsigned char uint8_t;
typedef signed   short int16_t;
typedef unsigned short uint16_t;
typedef signed   int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;

typedef struct AnalyzeInputInfo {
  int width;
  int height;
  int mb_w;
  int mb_h;
  int y_stride;
  int uv_stride;
  int preds_w;
  int top_stride;
  size_t mb_size;
  size_t preds_size;
  size_t nz_size;
  uint64_t y_size;
  uint64_t u_size;
  uint64_t v_size;
}AnalyzeInputInfo;

typedef struct {
  int x, y;
  int y_stride, uv_stride;
  int width, height;
  int preds_w;
  int mb_w, mb_h;
  int i4;
}VP8EncItPara;

typedef struct {
  // We only need to store max_value and last_non_zero, not the distribution.
  int max_value;
  int last_non_zero;
} VP8Histogram;

//uint8_t clip1[255 + 510 + 1];    // clips [-255,510] to [0,255]
static const uint8_t clip1[255 + 511 + 1] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
  0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
  0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
  0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
  0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
  0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
  0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
  0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
  0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// Must be ordered using {DC_PRED, TM_PRED, V_PRED, H_PRED} as index
const int VP8I16ModeOffsets[2] = { I16DC16, I16TM16 };
const int VP8UVModeOffsets[2] = { C8DC8, C8TM8 };

// Must be indexed using {B_DC_PRED -> B_HU_PRED} as index
const int VP8I4ModeOffsets[2] = { I4DC4, I4TM4};

static const int VP8DspScanY[16] = {
  // Luma
  0 +  0 * BPS,  1 +  0 * BPS, 2 +  0 * BPS, 3 +  0 * BPS,
  0 +  4 * BPS,  1 +  4 * BPS, 2 +  4 * BPS, 3 +  4 * BPS,
  0 +  8 * BPS,  1 +  8 * BPS, 2 +  8 * BPS, 3 +  8 * BPS,
  0 + 12 * BPS,  1 + 12 * BPS, 2 + 12 * BPS, 3 + 12 * BPS,
};

static const int VP8DspScanUV[8] = {
  0 + 0 * BPS,  1 + 0 * BPS, 0 + 4 * BPS, 1 + 4 * BPS,    // U
  2 + 0 * BPS,  3 + 0 * BPS, 2 + 4 * BPS, 3 + 4 * BPS     // V
};

static const int VP8DspScanInY[16] = {
  // Luma
  0 +  0 * BPS_IN,  1 +  0 * BPS_IN, 2 +  0 * BPS_IN, 3 +  0 * BPS_IN,
  0 +  4 * BPS_IN,  1 +  4 * BPS_IN, 2 +  4 * BPS_IN, 3 +  4 * BPS_IN,
  0 +  8 * BPS_IN,  1 +  8 * BPS_IN, 2 +  8 * BPS_IN, 3 +  8 * BPS_IN,
  0 + 12 * BPS_IN,  1 + 12 * BPS_IN, 2 + 12 * BPS_IN, 3 + 12 * BPS_IN,
};

static const int VP8DspScanInUV[8] = {
  0 + 0 * BPS_IN,  1 + 0 * BPS_IN, 0 + 4 * BPS_IN, 1 + 4 * BPS_IN,    // U
  2 + 0 * BPS_IN,  3 + 0 * BPS_IN, 2 + 4 * BPS_IN, 3 + 4 * BPS_IN     // V
};

static const int VP8Scan[16] = {  // Luma
  0 +  0 * BPS_IN,  1 +  0 * BPS_IN, 2 +  0 * BPS_IN, 3 +  0 * BPS_IN,
  0 +  4 * BPS_IN,  1 +  4 * BPS_IN, 2 +  4 * BPS_IN, 3 +  4 * BPS_IN,
  0 +  8 * BPS_IN,  1 +  8 * BPS_IN, 2 +  8 * BPS_IN, 3 +  8 * BPS_IN,
  0 + 12 * BPS_IN,  1 + 12 * BPS_IN, 2 + 12 * BPS_IN, 3 + 12 * BPS_IN,
};

void InitLeft(__local uchar4* y_left_p, __local uchar4* u_left_p, __local uchar4* v_left_p, int y) {
  uchar4 value = (uchar4)(129);
  y_left_p[-1].s3 = u_left_p[-1].s3 = v_left_p[-1].s3 = (y > 0) ? 129 : 127;

  for (int y_index = 0; y_index < 4; y_index++) {
    y_left_p[y_index] = value;
  }

  for (int u_index = 0; u_index < 2; u_index++) {
    u_left_p[u_index] = value;
  }

  for (int v_index = 0; v_index < 2; v_index++) {
    v_left_p[v_index] = value;
  }
}

void ImportBlock(__local const uchar4* src, int src_stride,
                 __local uchar4* dst, int w, int h, int size) {
  int i, j;
  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < size; ++i) {
    for (j = 0; j < w; j++) {
      dst[j] = src[j];
    }
    dst += BPS_IN;
    src += src_stride;
  }
}

void ImportLine(__local const uchar4* src, int src_stride,
                __local uchar4* dst, int len, int total_len) {
  int i;
  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < total_len; ++i, src += src_stride) {
    dst[i] = *src;
  }
}

void ImportLeft(__local const uchar4* src, int src_stride,
                __local uchar4* dst, int len, int total_len) {
  int i;
  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < total_len; ++i) {
      dst[i].s0 = (*src).s3;
      src += src_stride;

      dst[i].s1 = (*src).s3;
      src += src_stride;

      dst[i].s2 = (*src).s3;
      src += src_stride;

      dst[i].s3 = (*src).s3;
      src += src_stride;
  }
}

void VP8IteratorImport(VP8EncItPara* para,
                       __local uchar4* y_l, __local uchar4* u_l, __local uchar4* v_l,
                       __local uchar4* tmp_y, __local uchar4* tmp_uv,
                       __local uchar4* y_in_p, __local uchar4* uv_in_p,
                       __local uchar4* y_left_p, __local uchar4* u_left_p, __local uchar4* v_left_p,
                       int shift) {
  const int x = para->x, y = para->y;
  const int stride_y = para->y_stride >> 2;
  const int stride_uv = para->uv_stride >> 2;
  uchar4 tmp_value = (uchar4)(127);
  int i;
  __local const uchar4* const ysrc = y_l + stride_y * shift + x * 4;
  __local const uchar4* const usrc = u_l + stride_uv * shift + x * 2;
  __local const uchar4* const vsrc = v_l + stride_uv * shift + x * 2;
  const int w = min(para->width - x * 16, 16);
  const int h = min(para->height - y * 16, 16);
  const int uv_w = (w + 1) >> 1;
  const int uv_h = (h + 1) >> 1;

  ImportBlock(ysrc, stride_y,  y_in_p + Y_OFF_ENC, 4, h, 16);
  ImportBlock(usrc, stride_uv, uv_in_p + U_OFF_ENC, 2, uv_h, 8);
  ImportBlock(vsrc, stride_uv, uv_in_p + V_OFF_ENC, 2, uv_h, 8);

  //if (tmp_y == NULL || tmp_uv == NULL) return;

  // Import source (uncompressed) samples into boundary.
  if (x == 0) {
    InitLeft(y_left_p, u_left_p, v_left_p, y);
  } else {
    if (y == 0) {
      y_left_p[-1].s3 = u_left_p[-1].s3 = v_left_p[-1].s3 = 127;
    } else {
      y_left_p[-1].s3 = ysrc[- 1 - stride_y].s3;
      u_left_p[-1].s3 = usrc[- 1 - stride_uv].s3;
      v_left_p[-1].s3 = vsrc[- 1 - stride_uv].s3;
    }
    ImportLeft(ysrc - 1, stride_y,  y_left_p, h,   4);
    ImportLeft(usrc - 1, stride_uv, u_left_p, uv_h, 2);
    ImportLeft(vsrc - 1, stride_uv, v_left_p, uv_h, 2);
  }

  if (y == 0) {
    for (i = 0; i < 4; i++) {
      tmp_y[i] = tmp_value;
      tmp_uv[i] = tmp_value;
    }
  } else {
    ImportLine(ysrc - stride_y,  1, tmp_y,      4, 4);
    ImportLine(usrc - stride_uv, 1, tmp_uv,     2, 2);
    ImportLine(vsrc - stride_uv, 1, tmp_uv + 2, 2, 2);
  }
}

void VP8SetIntra16Mode(VP8EncItPara* para, __local uint8_t* mb_info_p, __local uint8_t* preds_p, int mode) {
  __local uint8_t* preds = preds_p + 4 * para->x;
  int y;
  int i;
  //__attribute__((opencl_unroll_hint))
  __attribute__((xcl_pipeline_loop))
  for (y = 0; y < 4; ++y) {
    for (i = 0; i < 4; i++) {
      preds[i] = mode;
    }
    preds += para->preds_w;
  }
  (*(mb_info_p + 3 * para->x)) = 1;
}

void FillI4(__local uchar4* dst, int value, int size) {
  int j, k;
  uchar4 dc_value = (uchar4)(value);
  __attribute__((xcl_pipeline_loop))
  for (j = 0; j < size; ++j) {
    dst[j * BPS_I4] = dc_value;
  }
}

void TrueMotion(__local uchar4* dst, __local const uchar4* left,
                __local const uchar4* top, int size,
                int left_status, int top_status) {
  int y;
  int j, k;

  if (left_status != 0) {
    if (top_status != 0) {
      const uint8_t* const clip = clip1 + 255 - left[-1].s3;
      __attribute__((xcl_pipeline_loop))
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table_s0 = clip + left[y].s0;
        const uint8_t* const clip_table_s1 = clip + left[y].s1;
        const uint8_t* const clip_table_s2 = clip + left[y].s2;
        const uint8_t* const clip_table_s3 = clip + left[y].s3;
        int x;
        for (x = 0; x < size; ++x) {
          dst[x].s0 = clip_table_s0[top[x].s0];
          dst[x].s1 = clip_table_s0[top[x].s1];
          dst[x].s2 = clip_table_s0[top[x].s2];
          dst[x].s3 = clip_table_s0[top[x].s3];
        }
        dst += BPS;
        for (x = 0; x < size; ++x) {
          dst[x].s0 = clip_table_s1[top[x].s0];
          dst[x].s1 = clip_table_s1[top[x].s1];
          dst[x].s2 = clip_table_s1[top[x].s2];
          dst[x].s3 = clip_table_s1[top[x].s3];
        }
        dst += BPS;
        for (x = 0; x < size; ++x) {
          dst[x].s0 = clip_table_s2[top[x].s0];
          dst[x].s1 = clip_table_s2[top[x].s1];
          dst[x].s2 = clip_table_s2[top[x].s2];
          dst[x].s3 = clip_table_s2[top[x].s3];
        }
        dst += BPS;
        for (x = 0; x < size; ++x) {
          dst[x].s0 = clip_table_s3[top[x].s0];
          dst[x].s1 = clip_table_s3[top[x].s1];
          dst[x].s2 = clip_table_s3[top[x].s2];
          dst[x].s3 = clip_table_s3[top[x].s3];
        }
        dst += BPS;
      }
    } else {
      // HorizontalPred case.
      __attribute__((xcl_pipeline_loop))
      for (j = 0; j < size; ++j) {
        for (k = 0; k < size; k++) {
          dst[(4 * j + 0) * BPS + k] = (uchar4)(left[j].s0);
          dst[(4 * j + 1) * BPS + k] = (uchar4)(left[j].s1);
          dst[(4 * j + 2) * BPS + k] = (uchar4)(left[j].s2);
          dst[(4 * j + 3) * BPS + k] = (uchar4)(left[j].s3);
        }
      }
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (top_status != 0) {
      __attribute__((xcl_pipeline_loop))
      for (j = 0; j < (size << 2); ++j) {
        for (k = 0; k < size; k++) {
          dst[j * BPS + k] = top[k];
        }
      }
    } else {
      int4 DC_value = (int4)(129);
      __attribute__((xcl_pipeline_loop))
      for (j = 0; j < (size << 2); ++j) {
        for (k = 0; k < size; k++) {
          dst[j * BPS + k] = convert_uchar4(DC_value);
        }
      }
    }
  }
}

void DCMode(__local uchar4* dst, __local const uchar4* left,
            __local const uchar4* top,
            int size, int round, int shift,
            int left_status, int top_status) {
  int4 DC_tmp = (int4)(0);
  int DC = 0;
  int j, k;
  if (top_status != 0) {
    for (j = 0; j < size; ++j) DC_tmp += convert_int4(top[j]);
    if (left_status != 0) {   // top and left present
      for (j = 0; j < size; ++j) DC_tmp += convert_int4(left[j]);
    } else {      // top, but no left
      DC_tmp += DC_tmp;
    }
    DC = DC_tmp.s0 + DC_tmp.s1 + DC_tmp.s2 + DC_tmp.s3;
    DC = (DC + round) >> shift;
  } else if (left_status != 0) {   // left but no top
    for (j = 0; j < size; ++j) DC_tmp += convert_int4(left[j]);
    DC_tmp += DC_tmp;
    DC = DC_tmp.s0 + DC_tmp.s1 + DC_tmp.s2 + DC_tmp.s3;
    DC = (DC + round) >> shift;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }

  int4 DC_value = (int4)(DC);
  __attribute__((xcl_pipeline_loop))
  for (j = 0; j < (size << 2); ++j) {
    for (k = 0; k < size; k++) {
      dst[j * BPS + k] = convert_uchar4(DC_value);
    }
  }
}

void Intra16Preds(__local uchar4* dst,
                  __local const uchar4* left, __local const uchar4* top,
                  int left_status, int top_status) {
  DCMode(I16DC16 + dst, left, top, 4, 16, 5, left_status, top_status);
  TrueMotion(I16TM16 + dst, left, top, 4, left_status, top_status);
}

void IntraChromaPreds(__local uchar4* dst, __local const uchar4* left,
                      __local const uchar4* top,
                      int left_status, int top_status) {
  // U block
  DCMode(C8DC8 + dst, left, top, 2, 8, 4, left_status, top_status);
  TrueMotion(C8TM8 + dst, left, top, 2, left_status, top_status);
  // V block
  dst += 2;
  if (top_status != 0) top += 2;
  if (left_status != 0) left += 4;
  DCMode(C8DC8 + dst, left, top, 2, 8, 4, left_status, top_status);
  TrueMotion(C8TM8 + dst, left, top, 2, left_status, top_status);
}

void InitHistogram(VP8Histogram* const histo) {
  histo->max_value = 0;
  histo->last_non_zero = 1;
}

void FTransform(__local const uchar4* src, __local const uchar4* ref, int16_t* out) {
  int i;
  int tmp[16] __attribute__((xcl_array_partition(complete, 1)));
  for (i = 0; i < 4; ++i, src += BPS_IN, ref += BPS) {
    const int4 d = (convert_int4)(*src) - (convert_int4)(*ref);   // 9bit dynamic range ([-255,255])
    const int a0 = (d.s0 + d.s3);         // 10b                      [-510,510]
    const int a1 = (d.s1 + d.s2);
    const int a2 = (d.s1 - d.s2);
    const int a3 = (d.s0 - d.s3);
    tmp[0 + i * 4] = (a0 + a1) * 8;   // 14b                      [-8160,8160]
    tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 1812) >> 9;      // [-7536,7542]
    tmp[2 + i * 4] = (a0 - a1) * 8;
    tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 +  937) >> 9;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[12 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[ 8 + i]);
    const int a2 = (tmp[4 + i] - tmp[ 8 + i]);
    const int a3 = (tmp[0 + i] - tmp[12 + i]);
    out[0 + i] = (a0 + a1 + 7) >> 4;            // 12b
    out[4 + i] = ((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0);
    out[8 + i] = (a0 - a1 + 7) >> 4;
    out[12+ i] = ((a3 * 2217 - a2 * 5352 + 51000) >> 16);
  }
}

void FTransformI4(__local const uchar4* src, __local const uchar4* ref, int16_t* out) {
  int i;
  int tmp[16];
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  for (i = 0; i < 4; ++i, src += BPS_IN, ref += BPS_I4) {
    const int4 d = (convert_int4)(*src) - (convert_int4)(*ref);   // 9bit dynamic range ([-255,255])
    const int a0 = (d.s0 + d.s3);         // 10b                      [-510,510]
    const int a1 = (d.s1 + d.s2);
    const int a2 = (d.s1 - d.s2);
    const int a3 = (d.s0 - d.s3);
    tmp[0 + i * 4] = (a0 + a1) * 8;   // 14b                      [-8160,8160]
    tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 1812) >> 9;      // [-7536,7542]
    tmp[2 + i * 4] = (a0 - a1) * 8;
    tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 +  937) >> 9;
  }
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[12 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[ 8 + i]);
    const int a2 = (tmp[4 + i] - tmp[ 8 + i]);
    const int a3 = (tmp[0 + i] - tmp[12 + i]);
    out[0 + i] = (a0 + a1 + 7) >> 4;            // 12b
    out[4 + i] = ((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0);
    out[8 + i] = (a0 - a1 + 7) >> 4;
    out[12+ i] = ((a3 * 2217 - a2 * 5352 + 51000) >> 16);
  }
}

// general-purpose util function
void VP8SetHistogramData(const int* distribution,
                         VP8Histogram* const histo) {
  int max_value = 0, last_non_zero = 1;
  int k;
  __attribute__((xcl_pipeline_loop))
  for (k = 0; k <= MAX_COEFF_THRESH; ++k) {
    const int value = distribution[k];
    if (value > 0) {
      if (value > max_value) max_value = value;
      last_non_zero = k;
    }
  }
  histo->max_value = max_value;
  histo->last_non_zero = last_non_zero;
}

void CollectHistogramI4(const __local const uchar4* ref, const __local const uchar4* pred,
                      VP8Histogram* const histo) {
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };

  int k;
  int16_t out[16];

  FTransformI4(ref + VP8DspScanInY[j], pred + VP8DspScanY[j], out);

  // Convert coefficients to bin.
  //__attribute__((opencl_unroll_hint))
  for (k = 0; k < 16; ++k) {
    const int v = abs(out[k]) >> 3;  // TODO(skal): add rounding?
    const int clipped_value = min(v, MAX_COEFF_THRESH);
    ++distribution[clipped_value];
  }
  VP8SetHistogramData(distribution, histo);
}

int GetAlpha(const VP8Histogram* const histo) {
  // 'alpha' will later be clipped to [0..MAX_ALPHA] range, clamping outer
  // values which happen to be mostly noise. This leaves the maximum precision
  // for handling the useful small values which contribute most.
  const int max_value = histo->max_value;
  const int last_non_zero = histo->last_non_zero;
  const int alpha =
      (max_value > 1) ? ALPHA_SCALE * last_non_zero / max_value : 0;
  return alpha;
}

int MBAnalyzeBestIntra16Mode(VP8EncItPara* para, __local uint8_t* mb_info_p, __local uint8_t* preds_p,
                             __local uchar4* y_in_p, __local uchar4* y_p_p,
                             __local uchar4* y_left_p, __local uchar4* y_top_p) {
  int best_alpha = DEFAULT_ALPHA;
  int best_mode = 0;

  int max_value0 = 0;
  int max_value1 = 0;
  int last_non_zero0 = 1;
  int last_non_zero1 = 1;
  int alpha0;
  int alpha1;

  int i, j, k;
  int distribution0[MAX_COEFF_THRESH + 1] __attribute__((xcl_array_partition(complete, 1)));
  int distribution1[MAX_COEFF_THRESH + 1] __attribute__((xcl_array_partition(complete, 1)));

  int cache_value;
  int index_cache0[256] __attribute__((xcl_array_partition(cyclic, 16, 1)));
  int index_cache1[256] __attribute__((xcl_array_partition(cyclic, 16, 1)));

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < MAX_COEFF_THRESH + 1; i++) {
    distribution0[i] = 0;
    distribution1[i] = 0;
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 16; i++) {
    index_cache0[0 + 16 * i] = -1;
    index_cache0[1 + 16 * i] = -1;
    index_cache0[2 + 16 * i] = -1;
    index_cache0[3 + 16 * i] = -1;
    index_cache0[4 + 16 * i] = -1;
    index_cache0[5 + 16 * i] = -1;
    index_cache0[6 + 16 * i] = -1;
    index_cache0[7 + 16 * i] = -1;
    index_cache0[8 + 16 * i] = -1;
    index_cache0[9 + 16 * i] = -1;
    index_cache0[10 + 16 * i] = -1;
    index_cache0[11 + 16 * i] = -1;
    index_cache0[12 + 16 * i] = -1;
    index_cache0[13 + 16 * i] = -1;
    index_cache0[14 + 16 * i] = -1;
    index_cache0[15 + 16 * i] = -1;

    index_cache1[0 + 16 * i] = -1;
    index_cache1[1 + 16 * i] = -1;
    index_cache1[2 + 16 * i] = -1;
    index_cache1[3 + 16 * i] = -1;
    index_cache1[4 + 16 * i] = -1;
    index_cache1[5 + 16 * i] = -1;
    index_cache1[6 + 16 * i] = -1;
    index_cache1[7 + 16 * i] = -1;
    index_cache1[8 + 16 * i] = -1;
    index_cache1[9 + 16 * i] = -1;
    index_cache1[10 + 16 * i] = -1;
    index_cache1[11 + 16 * i] = -1;
    index_cache1[12 + 16 * i] = -1;
    index_cache1[13 + 16 * i] = -1;
    index_cache1[14 + 16 * i] = -1;
    index_cache1[15 + 16 * i] = -1;
  }

  if (para->x == 0 && para->y == 0) {
    Intra16Preds(y_p_p, y_left_p, y_top_p, 0, 0);
  }
  else if (para->x == 0 && para->y != 0) {
    Intra16Preds(y_p_p, y_left_p, y_top_p, 0, 1);
  }
  else if (para->x != 0 && para->y == 0) {
    Intra16Preds(y_p_p, y_left_p, y_top_p, 1, 0);
  }
  else {
    Intra16Preds(y_p_p, y_left_p, y_top_p, 1, 1);
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 16; ++i) {
    int16_t out0[16] __attribute__((xcl_array_partition(complete, 1)));

    FTransform(y_in_p + VP8DspScanInY[i], y_p_p + VP8I16ModeOffsets[0] + VP8DspScanY[i], out0);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out0[k]) >> 3;  // TODO(skal): add rounding?
      const int clipped_value = min(v, MAX_COEFF_THRESH);
      index_cache0[k + i * 16] = clipped_value;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 256; i++) {
    cache_value = index_cache0[i];
    if (cache_value != -1) {
      distribution0[cache_value] += 1;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (int i = 0; i <= MAX_COEFF_THRESH; ++i) {
    const int value0 = distribution0[i];
    if (value0 > 0) {
      if (value0 > max_value0) max_value0 = value0;
      last_non_zero0 = i;
    }
  }

  alpha0 = (max_value0 > 1) ? ALPHA_SCALE * last_non_zero0 / max_value0 : 0;
  if (alpha0 > best_alpha) {
    best_alpha = alpha0;
    best_mode = 0;
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 16; ++i) {
    int16_t out1[16] __attribute__((xcl_array_partition(complete, 1)));

    FTransform(y_in_p + VP8DspScanInY[i], y_p_p + VP8I16ModeOffsets[1] + VP8DspScanY[i], out1);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out1[k]) >> 3;  // TODO(skal): add rounding?
      const int clipped_value = min(v, MAX_COEFF_THRESH);
      index_cache1[k + i * 16] = clipped_value;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 256; i++) {
    cache_value = index_cache1[i];
    if (cache_value != -1) {
      distribution1[cache_value] += 1;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i <= MAX_COEFF_THRESH; ++i) {
    const int value1 = distribution1[i];
    if (value1 > 0) {
      if (value1 > max_value1) max_value1 = value1;
      last_non_zero1 = i;
    }
  }

  alpha1 = (max_value1 > 1) ? ALPHA_SCALE * last_non_zero1 / max_value1 : 0;
  if (alpha1 > best_alpha) {
    best_alpha = alpha1;
    best_mode = 1;
  }

  VP8SetIntra16Mode(para, mb_info_p, preds_p, best_mode);
  return best_alpha;
}

int MBAnalyzeBestUVMode(VP8EncItPara* para, __local uint8_t* mb_info_p,
                        __local uchar4* uv_in_p, __local uchar4* uv_p_p,
                        __local uchar4* u_left_p, __local uchar4* uv_top_p) {
  int best_alpha = DEFAULT_ALPHA;
  int best_mode = 0;

  int max_value0 = 0;
  int max_value1 = 0;
  int last_non_zero0 = 1;
  int last_non_zero1 = 1;
  int alpha0;
  int alpha1;

  int i, j, k;
  int distribution0[MAX_COEFF_THRESH + 1] __attribute__((xcl_array_partition(complete, 1)));
  int distribution1[MAX_COEFF_THRESH + 1] __attribute__((xcl_array_partition(complete, 1)));

  int cache_value;
  int index_cache0[128] __attribute__((xcl_array_partition(cyclic, 16, 1)));
  int index_cache1[128] __attribute__((xcl_array_partition(cyclic, 16, 1)));

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < MAX_COEFF_THRESH + 1; i++) {
    distribution0[i] = 0;
    distribution1[i] = 0;
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 8; i++) {
    index_cache0[0 + 16 * i] = -1;
    index_cache0[1 + 16 * i] = -1;
    index_cache0[2 + 16 * i] = -1;
    index_cache0[3 + 16 * i] = -1;
    index_cache0[4 + 16 * i] = -1;
    index_cache0[5 + 16 * i] = -1;
    index_cache0[6 + 16 * i] = -1;
    index_cache0[7 + 16 * i] = -1;
    index_cache0[8 + 16 * i] = -1;
    index_cache0[9 + 16 * i] = -1;
    index_cache0[10 + 16 * i] = -1;
    index_cache0[11 + 16 * i] = -1;
    index_cache0[12 + 16 * i] = -1;
    index_cache0[13 + 16 * i] = -1;
    index_cache0[14 + 16 * i] = -1;
    index_cache0[15 + 16 * i] = -1;

    index_cache1[0 + 16 * i] = -1;
    index_cache1[1 + 16 * i] = -1;
    index_cache1[2 + 16 * i] = -1;
    index_cache1[3 + 16 * i] = -1;
    index_cache1[4 + 16 * i] = -1;
    index_cache1[5 + 16 * i] = -1;
    index_cache1[6 + 16 * i] = -1;
    index_cache1[7 + 16 * i] = -1;
    index_cache1[8 + 16 * i] = -1;
    index_cache1[9 + 16 * i] = -1;
    index_cache1[10 + 16 * i] = -1;
    index_cache1[11 + 16 * i] = -1;
    index_cache1[12 + 16 * i] = -1;
    index_cache1[13 + 16 * i] = -1;
    index_cache1[14 + 16 * i] = -1;
    index_cache1[15 + 16 * i] = -1;
  }

  if (para->x == 0 && para->y == 0) {
    IntraChromaPreds(uv_p_p, u_left_p, uv_top_p, 0, 0);
  }
  else if (para->x == 0 && para->y != 0) {
    IntraChromaPreds(uv_p_p, u_left_p, uv_top_p, 0, 1);
  }
  else if (para->x != 0 && para->y == 0) {
    IntraChromaPreds(uv_p_p, u_left_p, uv_top_p, 1, 0);
  }
  else {
    IntraChromaPreds(uv_p_p, u_left_p, uv_top_p, 1, 1);
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 8; ++i) {
    int16_t out0[16] __attribute__((xcl_array_partition(complete, 1)));

    FTransform(uv_in_p + VP8DspScanInUV[i], uv_p_p + VP8UVModeOffsets[0] + VP8DspScanUV[i], out0);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out0[k]) >> 3;  // TODO(skal): add rounding?
      const int clipped_value = min(v, MAX_COEFF_THRESH);
      index_cache0[k + i * 16] = clipped_value;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 128; i++) {
    cache_value = index_cache0[i];
    if (cache_value != -1) {
      distribution0[cache_value] += 1;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (int k = 0; k <= MAX_COEFF_THRESH; ++k) {
    const int value0 = distribution0[k];
    if (value0 > 0) {
      if (value0 > max_value0) max_value0 = value0;
      last_non_zero0 = k;
    }
  }

  alpha0 = (max_value0 > 1) ? ALPHA_SCALE * last_non_zero0 / max_value0 : 0;
  if (alpha0 > best_alpha) {
    best_alpha = alpha0;
    best_mode = 0;
  }

  __attribute__((xcl_pipeline_loop))
  for (j = 0; j < 8; ++j) {
    int16_t out1[16] __attribute__((xcl_array_partition(complete, 1)));

    FTransform(uv_in_p + VP8DspScanInUV[j], uv_p_p + VP8UVModeOffsets[1] + VP8DspScanUV[j], out1);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out1[k]) >> 3;  // TODO(skal): add rounding?
      const int clipped_value = min(v, MAX_COEFF_THRESH);
      index_cache1[k + j * 16] = clipped_value;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i < 128; i++) {
    cache_value = index_cache1[i];
    if (cache_value != -1) {
      distribution1[cache_value] += 1;
    }
  }

  __attribute__((xcl_pipeline_loop))
  for (i = 0; i <= MAX_COEFF_THRESH; ++i) {
    const int value1 = distribution1[i];
    if (value1 > 0) {
      if (value1 > max_value1) max_value1 = value1;
      last_non_zero1 = i;
    }
  }

  alpha1 = (max_value1 > 1) ? ALPHA_SCALE * last_non_zero1 / max_value1 : 0;
  if (alpha1 > best_alpha) {
    best_alpha = alpha1;
    best_mode = 1;
  }

  // Set intra UV mode.
  (*(mb_info_p + 3 * para->x + 1)) = best_mode;
  return best_alpha;
}

// Array to record the position of the top sample to pass to the prediction
// functions in dsp.c.
static const uint8_t VP8TopLeftI4[16] = {
  5, 6, 7, 8,
  4, 5, 6, 7,
  3, 4, 5, 6,
  2, 3, 4, 5
};

void VP8IteratorStartI4(__local const uchar4* const y_left_p, __local uchar4* const y_top_p,
                        __local uchar4* const i4_boundary_p, VP8EncItPara* para) {
  int i;

  para->i4 = 0;    // first 4x4 sub-block

  // Import the boundary samples
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  for (i = 0; i < 5; ++i) {    // left
    i4_boundary_p[i].s3 = y_left_p[3 - i].s3;
    if (i < 4) {
      i4_boundary_p[i + 1].s0 = y_left_p[3 - i].s2;
      i4_boundary_p[i + 1].s1 = y_left_p[3 - i].s1;
      i4_boundary_p[i + 1].s2 = y_left_p[3 - i].s0;
    }
  }
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  for (i = 0; i < 4; ++i) {    // top
    i4_boundary_p[5 + i] = y_top_p[i];
  }
  // top-right samples have a special case on the far right of the picture
  if (para->x < para->mb_w - 1) {
    //__attribute__((opencl_unroll_hint))
    //__attribute__((xcl_pipeline_loop))
    i4_boundary_p[9] = y_top_p[4];
  } else {    // else, replicate the last valid pixel four times
    //__attribute__((opencl_unroll_hint))
    //__attribute__((xcl_pipeline_loop))
    i4_boundary_p[9].s0 = i4_boundary_p[8].s3;
    i4_boundary_p[9].s1 = i4_boundary_p[8].s3;
    i4_boundary_p[9].s2 = i4_boundary_p[8].s3;
    i4_boundary_p[9].s3 = i4_boundary_p[8].s3;
  }
}

void DC4(__local uchar4* dst, __local const uchar4* const top) {
  uint32_t dc = 4;
  int i;
  dc += top[0].s0 + top[0].s1 + top[0].s2 + top[0].s3;
  dc += top[-1].s0 + top[-1].s1 + top[-1].s2 + top[-2].s3;
  FillI4(dst, dc >> 3, 4);
}

void TM4(__local uchar4* dst, __local const uchar4* const top) {
  int x, y;
  const uint8_t* const clip = clip1 + 255 - top[-1].s3;
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  const uint8_t* const clip_table0 = clip + top[-1].s2;
  const uint8_t* const clip_table1 = clip + top[-1].s1;
  const uint8_t* const clip_table2 = clip + top[-1].s0;
  const uint8_t* const clip_table3 = clip + top[-2].s3;

  (*dst).s0 = clip_table0[top[0].s0];
  (*dst).s1 = clip_table0[top[0].s1];
  (*dst).s2 = clip_table0[top[0].s2];
  (*dst).s3 = clip_table0[top[0].s3];
  dst += BPS_I4;

  (*dst).s0 = clip_table1[top[0].s0];
  (*dst).s1 = clip_table1[top[0].s1];
  (*dst).s2 = clip_table1[top[0].s2];
  (*dst).s3 = clip_table1[top[0].s3];
  dst += BPS_I4;

  (*dst).s0 = clip_table2[top[0].s0];
  (*dst).s1 = clip_table2[top[0].s1];
  (*dst).s2 = clip_table2[top[0].s2];
  (*dst).s3 = clip_table2[top[0].s3];
  dst += BPS_I4;

  (*dst).s0 = clip_table3[top[0].s0];
  (*dst).s1 = clip_table3[top[0].s1];
  (*dst).s2 = clip_table3[top[0].s2];
  (*dst).s3 = clip_table3[top[0].s3];
}

// Left samples are top[-5 .. -2], top_left is top[-1], top are
// located at top[0..3], and top right is top[4..7]
void Intra4Preds(__local uchar4* dst, __local const uchar4* const top) {
  DC4(I4DC4 + dst, top);
  TM4(I4TM4 + dst, top);
}

void VP8MakeIntra4Preds(VP8EncItPara* para, __local uchar4* i4_p_p, __local const uchar4* const i4_top_p) {
  Intra4Preds(i4_p_p, i4_top_p);
}

void MergeHistograms(const VP8Histogram* const in,
                     VP8Histogram* const out) {
  if (in->max_value > out->max_value) {
    out->max_value = in->max_value;
  }
  if (in->last_non_zero > out->last_non_zero) {
    out->last_non_zero = in->last_non_zero;
  }
}

int VP8IteratorRotateI4(__local uchar4* const i4_boundry_p, VP8EncItPara* para,
                        __local const uchar4* const yuv_out) {
  __local const uchar4* const blk = yuv_out + VP8Scan[para->i4];
  __local uchar4* const top = i4_boundry_p;
  int i, j;

  // Update the cache with 7 fresh samples
  top[-1] = blk[3 * BPS_IN];
  if ((para->i4 & 3) != 3) {  // if not on the right sub-blocks #3, #7, #11, #15
    (*top).s0 = blk[2 * BPS_IN].s3;
    (*top).s1 = blk[1 * BPS_IN].s3;
    (*top).s2 = blk[0 * BPS_IN].s3;
  } else {  // else replicate top-right samples, as says the specs.
    *top = *(top + 1);
  }
  // move pointers to next sub-block
  ++para->i4;
  if (para->i4 == 16) {    // we're done
    return 0;
  }

  return 1;
}

void VP8SetIntra4Mode(VP8EncItPara* para, const uint8_t* modes,
                      __local uint8_t* mb_info_p, __local uint8_t* preds_p) {
  __local uint8_t* preds = preds_p + 4 * para->x;
  int y, i;
  //__attribute__((opencl_unroll_hint))
  //__attribute__((xcl_pipeline_loop))
  for (y = 4; y > 0; --y) {
    for (i = 0; i < 4; i++) {
      preds[i] = modes[i];
    }
    preds += para->preds_w;
    modes += 4;
  }
  (*(mb_info_p + 3 * para->x)) = 0;
}

int MBAnalyzeBestIntra4Mode(VP8EncItPara* para, int best_alpha,
                            __local uint8_t* mb_info_p, __local uint8_t* preds_p,
                            __local uchar4* const y_in_p, __local uchar4* const i4_p_p, __local uchar4* const i4_boundry_p,
                            __local const uchar4* const y_left_p, __local const uchar4* const y_top_p) {
  uint8_t modes[16];
  const int max_mode = MAX_INTRA4_MODE;
  int i4_alpha;
  VP8Histogram total_histo;
  int cur_histo = 0;
  InitHistogram(&total_histo);

  VP8IteratorStartI4(y_left_p, y_top_p, i4_boundry_p, para);
  do {
    int mode;
    int best_mode_alpha = DEFAULT_ALPHA;
    VP8Histogram histos[2];
    __local const uchar4* const src = y_in_p + Y_OFF_ENC + VP8Scan[para->i4];

    VP8MakeIntra4Preds(para, i4_p_p, i4_boundry_p + VP8TopLeftI4[para->i4]);

    for (mode = 0; mode < max_mode; ++mode) {
      int alpha;

      InitHistogram(&histos[cur_histo]);
      CollectHistogramI4(src, i4_p_p + VP8I4ModeOffsets[mode], &histos[cur_histo]);
      alpha = GetAlpha(&histos[cur_histo]);
      if (IS_BETTER_ALPHA(alpha, best_mode_alpha)) {
        best_mode_alpha = alpha;
        modes[para->i4] = mode;
        cur_histo ^= 1;   // keep track of best histo so far.
      }
    }
    // accumulate best histogram
    MergeHistograms(&histos[cur_histo ^ 1], &total_histo);
    // Note: we reuse the original samples for predictors
  } while (VP8IteratorRotateI4(i4_boundry_p + VP8TopLeftI4[para->i4], para, y_in_p + Y_OFF_ENC));

  i4_alpha = GetAlpha(&total_histo);
  if (IS_BETTER_ALPHA(i4_alpha, best_alpha)) {
    VP8SetIntra4Mode(para, modes, mb_info_p, preds_p);
    best_alpha = i4_alpha;
  }
  return best_alpha;
}

void MBAnalyze(VP8EncItPara* para, __local uint8_t* mb_info_p, __local uint8_t* preds_p,
               __local uchar4* y_in_p, __local uchar4* uv_in_p,
               __local uchar4* y_p_p, __local uchar4* uv_p_p,
               __local uchar4* y_left_p, __local uchar4* u_left_p, __local uchar4* v_left_p,
               __local uchar4* y_top_p, __local uchar4* uv_top_p,
               __local uchar4* i4_p_p, __local uchar4* i4_boundary_p,
               __global int* alphas, __global int* const alpha,
               __global int* const uv_alpha, int method) {
  int best_alpha, best_uv_alpha, alpha_tmp;

  VP8SetIntra16Mode(para, mb_info_p, preds_p, 0);  // default: Intra16, DC_PRED
  //VP8SetSkip(it, 0);         // not skipped
  //VP8SetSegment(it, 0);      // default segment, spec-wise.

  best_alpha = MBAnalyzeBestIntra16Mode(para, mb_info_p, preds_p,
                                        y_in_p, y_p_p,
                                        y_left_p, y_top_p);
  if (method >= 5) {
    // We go and make a fast decision for intra4/intra16.
    // It's usually not a good and definitive pick, but helps seeding the stats
    // about level bit-cost.
    // TODO(skal): improve criterion.
    best_alpha = MBAnalyzeBestIntra4Mode(para, best_alpha, mb_info_p, preds_p,
                                         y_in_p, i4_p_p, i4_boundary_p,
                                         y_left_p, y_top_p);
  }
  best_uv_alpha = MBAnalyzeBestUVMode(para, mb_info_p,
                                      uv_in_p, uv_p_p,
                                      u_left_p, uv_top_p);

  // Final susceptibility mix
  best_alpha = (3 * best_alpha + best_uv_alpha + 2) >> 2;
  alpha_tmp = MAX_ALPHA - best_alpha;
  best_alpha = clamp(alpha_tmp, 0, MAX_ALPHA);
  alphas[best_alpha + 256 * get_group_id(1)]++;
  (*(mb_info_p + 3 * para->x + 2)) = best_alpha;

  // Accumulate for later complexity analysis.
  (*(alpha + get_group_id(1))) += best_alpha;   // mixed susceptibility (not just luma)
  (*(uv_alpha + get_group_id(1))) += best_uv_alpha;
}

__kernel __attribute__ ((reqd_work_group_size(ANALYZE_GRX_SIZE, 1, 1)))
void analyze (__global uchar4* y,
              __global uchar4* u,
              __global uchar4* v,
              __global uint8_t* mb_info,
              __global uint8_t* preds,
              __global int* output_alpha,
              __global int* output_uvalpha,
              __global int* alphas,
              AnalyzeInputInfo input_info,
              int method) {
  event_t event_read[3];
  event_t event_write[2];

  VP8EncItPara input_para;

  int mb_offset, mb_length;
  int preds_offset, preds_length;
  int mb_w_l, mb_h_l, preds_w_l;
  int shift;
  int y_offset, y_length;
  int uv_offset, uv_length;
  int alphas_index;
  int vector_y_stride, vector_uv_stride;

  mb_w_l = input_info.mb_w;
  mb_h_l = input_info.mb_h;
  preds_w_l = input_info.preds_w;

  vector_y_stride = input_info.y_stride >> 2;
  vector_uv_stride = input_info.uv_stride >> 2;

  if (get_group_id(1) == 0) {
    shift = 0;
  } else {
    shift = 2;
  }

  __local uint8_t mb_info_l[3 * LARGEST_MB_W];
  __local uint8_t preds_l[4 * LARGEST_PREDS_W];
  __local uchar4 y_l[VECTOR_Y_STRIDE * 18] __attribute__((xcl_array_partition(cyclic, 8, 1)));
  __local uchar4 u_l[VECTOR_UV_STRIDE * 10] __attribute__((xcl_array_partition(cyclic, 8, 1)));
  __local uchar4 v_l[VECTOR_UV_STRIDE * 10] __attribute__((xcl_array_partition(cyclic, 8, 1)));

  input_para.y_stride  = input_info.y_stride;
  input_para.uv_stride = input_info.uv_stride;
  input_para.width     = input_info.width;
  input_para.height    = input_info.height;
  input_para.preds_w   = input_info.preds_w;
  input_para.mb_w      = input_info.mb_w;
  input_para.mb_h      = input_info.mb_h;
  input_para.y         = get_group_id(1);

  y_offset = get_group_id(1) * vector_y_stride * 16 - vector_y_stride * shift;
  y_length = vector_y_stride * (16 + shift);

  uv_offset = get_group_id(1) * vector_uv_stride * 8 - vector_uv_stride * shift;
  uv_length = vector_uv_stride * (8 + shift);

  event_read[0] = async_work_group_copy(y_l, y + y_offset, y_length, 0);
  event_read[1] = async_work_group_copy(u_l, u + uv_offset, uv_length, 0);
  event_read[2] = async_work_group_copy(v_l, v + uv_offset, uv_length, 0);
  wait_group_events(3, event_read);

  //__attribute__((xcl_pipeline_workitems)) {
    input_para.x = get_local_id(0);
    __local uchar4 y_tmp[5] __attribute__((xcl_array_partition(complete, 1)));
    __local uchar4 uv_tmp[4] __attribute__((xcl_array_partition(complete, 1)));
    __local uchar4 y_left_mem[8] __attribute__((xcl_array_partition(complete, 1)));
    __local uchar4 uv_left_mem[8] __attribute__((xcl_array_partition(complete, 1)));
    __local uchar4 y_in_mem[4*16] __attribute__((xcl_array_partition(cyclic, 16, 1)));
    __local uchar4 uv_in_mem[4*8] __attribute__((xcl_array_partition(cyclic, 8, 1)));
    __local uchar4 y_p_mem[8*16] __attribute__((xcl_array_partition(cyclic, 16, 1)));
    __local uchar4 uv_p_mem[8*8] __attribute__((xcl_array_partition(cyclic, 8, 1)));
    __local uchar4 i4_p_mem[2*4];
    __local uchar4 i4_bound[10];

    if (get_local_id(0) < mb_w_l) {
      VP8IteratorImport(&input_para,
                        y_l, u_l, v_l,
                        y_tmp, uv_tmp,
                        y_in_mem, uv_in_mem,
                        y_left_mem + 1, uv_left_mem + 1, uv_left_mem + 5,
                        shift);
      MBAnalyze(&input_para, mb_info_l, preds_l,
                y_in_mem, uv_in_mem,
                y_p_mem,uv_p_mem,
                y_left_mem + 1, uv_left_mem + 1, uv_left_mem + 5,
                y_tmp, uv_tmp,
                i4_p_mem, i4_bound,
                alphas, output_alpha, output_uvalpha, method);
    }
  //}
  barrier(CLK_LOCAL_MEM_FENCE);

  mb_offset = get_group_id(1) * 3 * input_info.mb_w;
  mb_length = 3 * input_info.mb_w;

  event_write[0] = async_work_group_copy(mb_info + mb_offset, mb_info_l, mb_length, 0);

  preds_offset = get_group_id(1) * 4 * input_info.preds_w;
  preds_length = 4 * input_info.preds_w;

  event_write[1] = async_work_group_copy(preds + preds_offset, preds_l, preds_length, 0);
  wait_group_events(2, event_write);
}