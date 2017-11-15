#include <math.h>
#include <string.h>
#include <assert.h>

typedef signed   char int8_t;
typedef unsigned char uint8_t;
typedef signed   short int16_t;
typedef unsigned short uint16_t;
typedef signed   int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;

typedef int64_t score_t;     // type used for scores, rate, distortion

#define BPS 32   // this is the common stride for enc/dec
#define WEBP_ALIGN_CST 31

#define YUV_SIZE_ENC (BPS * 16)
#define PRED_SIZE_ENC (32 * BPS + 16 * BPS + 8 * BPS)   // I16+Chroma+I4 preds
#define Y_OFF_ENC    (0)
#define U_OFF_ENC    (16)
#define V_OFF_ENC    (16 + 8)

// Layout of prediction blocks
// intra 16x16
#define I16DC16 (0 * 16 * BPS)
#define I16TM16 (I16DC16 + 16)
#define I16VE16 (1 * 16 * BPS)
#define I16HE16 (I16VE16 + 16)
// chroma 8x8, two U/V blocks side by side (hence: 16x8 each)
#define C8DC8 (2 * 16 * BPS)
#define C8TM8 (C8DC8 + 1 * 16)
#define C8VE8 (2 * 16 * BPS + 8 * BPS)
#define C8HE8 (C8VE8 + 1 * 16)
// intra 4x4
#define I4DC4 (3 * 16 * BPS +  0)
#define I4TM4 (I4DC4 +  4)
#define I4VE4 (I4DC4 +  8)
#define I4HE4 (I4DC4 + 12)
#define I4RD4 (I4DC4 + 16)
#define I4VR4 (I4DC4 + 20)
#define I4LD4 (I4DC4 + 24)
#define I4VL4 (I4DC4 + 28)
#define I4HD4 (3 * 16 * BPS + 4 * BPS)
#define I4HU4 (I4HD4 + 4)
#define I4TMP (I4HD4 + 8)

#define MAX_COST ((score_t)0x7fffffffffffffLL)
#define I4_PENALTY 14000  // Rate-penalty for quick i4/i16 decision
#define QFIX 17
#define BIAS(b)  ((b) << (QFIX - 8))

#define LARGEST_Y_STRIDE                3840
#define LARGEST_UV_STRIDE               1920
#define LARGEST_MB_W                    240
#define LARGEST_MB_H                    135
#define LARGEST_PREDS_W                 (4 * LARGEST_MB_W + 1)
#define LARGEST_PREDS_H                 (4 * LARGEST_MB_H + 1)
#define MATRIX_SIZE                     80

/////////////////////////////////// enum //////////////////////////////////////////

// intra prediction modes
enum { B_DC_PRED = 0,   // 4x4 modes
       B_TM_PRED = 1,
       B_VE_PRED = 2,
       B_HE_PRED = 3,
       B_RD_PRED = 4,
       B_VR_PRED = 5,
       B_LD_PRED = 6,
       B_VL_PRED = 7,
       B_HD_PRED = 8,
       B_HU_PRED = 9,
       NUM_BMODES = B_HU_PRED + 1 - B_DC_PRED,  // = 10

       // Luma16 or UV modes
       DC_PRED = B_DC_PRED, V_PRED = B_VE_PRED,
       H_PRED = B_HE_PRED, TM_PRED = B_TM_PRED,
       B_PRED = NUM_BMODES,   // refined I4x4 mode
       NUM_PRED_MODES = 4,

       // special modes
       B_DC_PRED_NOTOP = 4,
       B_DC_PRED_NOLEFT = 5,
       B_DC_PRED_NOTOPLEFT = 6,
       NUM_B_DC_MODES = 7 };

enum { MB_FEATURE_TREE_PROBS = 3,
       NUM_MB_SEGMENTS = 4,
       NUM_REF_LF_DELTAS = 4,
       NUM_MODE_LF_DELTAS = 4,    // I4x4, ZERO, *, SPLIT
       MAX_NUM_PARTITIONS = 8,
       // Probabilities
       NUM_TYPES = 4,   // 0: i16-AC,  1: i16-DC,  2:chroma-AC,  3:i4-AC
       NUM_BANDS = 8,
       NUM_CTX = 3,
       NUM_PROBAS = 11
     };

enum { MAX_LF_LEVELS = 64,       // Maximum loop filter level
       MAX_VARIABLE_LEVEL = 67,  // last (inclusive) level with variable cost
       MAX_LEVEL = 2047          // max level (note: max codable is 2047 + 67)
     };

#define TYPES_SIZE                        NUM_BANDS * NUM_CTX * NUM_PROBAS
#define BANDS_SIZE                        NUM_CTX * NUM_PROBAS
#define CTX_SIZE                          NUM_PROBAS

//////////////////////////////////////// struct ///////////////////////////////////////////

typedef struct EncloopInputData {
  int width;
  int height;
  int filter_sharpness;
  int show_compressed;
  int extra_info_type;
  int stats_add;
  int simple;
  int num_parts;
  int max_i4_header_bits;
  int lf_stats_status;
  int use_skip_proba;
  int method;
  int rd_opt;
}EncloopInputData;

typedef struct EncLoopOutputData {
  int32_t range;
  int32_t value;
  int32_t run;
  int32_t nb_bits;
  int32_t pos;
  int32_t max_pos;
  int32_t error;
}EncLoopOutputData;

typedef struct {
  uint8_t   yuv_in_p[YUV_SIZE_ENC];           // input samples
  uint8_t   yuv_out_p[YUV_SIZE_ENC];          // output samples
  uint8_t   yuv_out2_p[YUV_SIZE_ENC];         // secondary buffer swapped with yuv_out_.
  uint8_t   yuv_p_p[PRED_SIZE_ENC];            // scratch buffer for prediction

  uint8_t   mb_info_p[5 * LARGEST_MB_W];              // type/uv_mode/skip/segment/alpha
  uint8_t   preds_p[4 * LARGEST_PREDS_W];            // intra mode predictors (4x4 blocks)
  uint32_t  nz_p[LARGEST_MB_W + 1];               // non-zero pattern

  uint8_t   y_left_p[32];           // left luma samples (addressable from index -1 to 15).
  uint8_t   uv_left_p[32];           // left uv samples (addressable from index -1 to 7)

  uint8_t   i4_boundary_p[37];   // 32+5 boundary samples needed by intra4x4
  int       top_nz_p[9];         // top-non-zero context.
  int       left_nz_p[9];        // left-non-zero. left_nz[8] is independent.

  uint8_t   y_top_p[LARGEST_MB_W * 16];
  uint8_t   uv_top_p[LARGEST_MB_W * 16];
} VP8EncIteratorPointer;

typedef struct {
  uint8_t coeffs_p[NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS];
  uint8_t bw_buf_p[408000];
  uint64_t bit_count_p[4 * 3];
  uint64_t sse_p[4];
  uint64_t sse_count[1];
  int block_count_p[3];
} VP8EncLoopPointer;

typedef struct {
  int           x_, y_;                      // current macroblock
  int           mb_w, mb_h;
  int           preds_w, preds_h;
  int           uv_width, uv_height;
  int           y_stride_, uv_stride_;       // respective strides
  int           i4_;                         // current intra4x4 mode being tested
  int           do_trellis_;                 // if true, perform extra level optimisation
  uint64_t      luma_bits_;                  // macroblock bit-cost for luma
  uint64_t      uv_bits_;                    // macroblock bit-cost for chroma
} VP8EncIteratorVariable;

typedef struct {
  score_t D, SD;              // Distortion, spectral distortion
  score_t H, R, score;        // header bits, rate, score.
  int16_t y_dc_levels[16];    // Quantized levels for luma-DC, luma-AC, chroma.
  int16_t y_ac_levels[16][16];
  int16_t uv_levels[4 + 4][16];
  int mode_i16;               // mode number for intra16 prediction
  uint8_t modes_i4[16];       // mode numbers for intra4 predictions
  int mode_uv;                // mode number of chroma prediction
  uint32_t nz;                // non-zero blocks
} VP8ModeScore;

typedef struct VP8Matrix {
  uint32_t q_[16];        // quantizer steps
  uint32_t iq_[16];       // reciprocals, fixed point.
  uint32_t bias_[16];     // rounding bias
  uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  uint32_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;

typedef struct VP8Residual VP8Residual;
struct VP8Residual {
  int first;
  int last;
  int coeff_type;
};

////////////////////////////////////// Global array ////////////////////////////////////////
// Must be ordered using {DC_PRED, TM_PRED, V_PRED, H_PRED} as index
const int VP8I16ModeOffsets[4] = { I16DC16, I16TM16, I16VE16, I16HE16 };
const int VP8UVModeOffsets[4] = { C8DC8, C8TM8, C8VE8, C8HE8 };

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

const int VP8Scan[16] = {  // Luma
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS,
};

static const int VP8ScanUV[4 + 4] = {
  0 + 0 * BPS,   4 + 0 * BPS, 0 + 4 * BPS,  4 + 4 * BPS,    // U
  8 + 0 * BPS,  12 + 0 * BPS, 8 + 4 * BPS, 12 + 4 * BPS     // V
};

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

const uint8_t VP8EncBands[16 + 1] = {
  0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
  0  // sentinel
};

static const uint8_t kNorm[128] = {  // renorm_sizes[i] = 8 - log2(i)
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

// range = ((range + 1) << kVP8Log2Range[range]) - 1
static const uint8_t kNewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127, 143, 159, 175, 191, 207, 223, 239,
  127, 135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239,
  247, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179,
  183, 187, 191, 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239,
  243, 247, 251, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
  151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179,
  181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209,
  211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

const uint8_t VP8Cat3[] = { 173, 148, 140 };
const uint8_t VP8Cat4[] = { 176, 155, 140, 135 };
const uint8_t VP8Cat5[] = { 180, 157, 141, 134, 130 };
const uint8_t VP8Cat6[] =
    { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129 };

//////////////////////////////////////// function ///////////////////////////////////////////

int QUANTDIV(uint32_t n, uint32_t iQ, uint32_t B) {
  return (int)((n * iQ + B) >> QFIX);
}

void InitLeft(VP8EncIteratorPointer* const it,
              VP8EncIteratorVariable* const it_var) {
  int i;
  int x = it_var->x_;
  uint8_t* y_left = it->y_left_p + 1;
  uint8_t* u_left = it->uv_left_p + 1;
  uint8_t* v_left = it->uv_left_p + 1 + 16;
  y_left[-1] = u_left[-1] = v_left[-1] =
      (it_var->y_ > 0) ? 129 : 127;
  for (i = 0; i < 16; i++) {
    y_left[i] = 129;
  }
  for (i = 0; i < 8; i++) {
    u_left[i] = 129;
  }
  for (i = 0; i < 8; i++) {
    v_left[i] = 129;
  }
  it->left_nz_p[8] = 0;
}

void ImportBlock(const uint8_t* src, int src_stride,
                 uint8_t* dst, int w, int h) {
  int i, j;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j++) {
      dst[j + i * BPS] = src[j + i * src_stride];
    }
  }
}

void VP8IteratorImport(VP8EncIteratorPointer* const it,
                       VP8EncIteratorVariable* const it_var,
                       uint8_t* y_l, uint8_t* u_l, uint8_t* v_l) {
  const int x = it_var->x_;
  const int stride_y = it_var->y_stride_;
  const int stride_uv = it_var->uv_stride_;

  const uint8_t* const ysrc = y_l + x * 16;
  const uint8_t* const usrc = u_l + x * 8;
  const uint8_t* const vsrc = v_l + x * 8;

  ImportBlock(ysrc, stride_y,  it->yuv_in_p + Y_OFF_ENC, 16, 16);
  ImportBlock(usrc, stride_uv, it->yuv_in_p + U_OFF_ENC, 8, 8);
  ImportBlock(vsrc, stride_uv, it->yuv_in_p + V_OFF_ENC, 8, 8);
}

// Init/Copy the common fields in score.
void InitScore(VP8ModeScore* const rd) {
  rd->D  = 0;
  rd->SD = 0;
  rd->R  = 0;
  rd->H  = 0;
  rd->nz = 0;
  rd->score = MAX_COST;
}

void Fill(uint8_t* dst, int value, int size) {
  int j, k;
  for (j = 0; j < size; ++j) {
    for (k = 0; k < size; k++) {
      dst[j * BPS + k] = value;
    }
  }
}

void VerticalPred(uint8_t* dst,
                  const uint8_t* top, int size,
                  int top_status) {
  int j, k;
  if (top_status != 0) {
    for (j = 0; j < size; ++j) {
      for (k = 0; k < size; k++) {
        dst[j * BPS + k] = top[k];
      }
    }
  } else {
    Fill(dst, 127, size);
  }
}

void HorizontalPred(uint8_t* dst,
                    const uint8_t* left, int size,
                    int left_status) {
  if (left_status != 0) {
    int j, k;
    for (j = 0; j < size; ++j) {
      for (k = 0; k < size; k++) {
        dst[j * BPS + k] = left[j];
      }
    }
  } else {
    Fill(dst, 129, size);
  }
}

void TrueMotion(uint8_t* dst, const uint8_t* left,
                const uint8_t* top, int size,
                int left_status, int top_status) {
  int y;
  if (left_status != 0) {
    if (top_status != 0) {
      const uint8_t* const clip = clip1 + 255 - left[-1];
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table = clip + left[y];
        int x;
        for (x = 0; x < size; ++x) {
          dst[x + y * BPS] = clip_table[top[x]];
        }
      }
    } else {
      HorizontalPred(dst, left, size, left_status);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (top_status != 0) {
      VerticalPred(dst, top, size, top_status);
    } else {
      Fill(dst, 129, size);
    }
  }
}

void DCMode(uint8_t* dst,
            const uint8_t* left,
            const uint8_t* top,
            int size, int round, int shift,
            int left_status, int top_status) {
  int DC = 0;
  int j;
  if (top_status != 0) {
    for (j = 0; j < size; ++j) DC += top[j];
    if (left_status != 0) {   // top and left present
      for (j = 0; j < size; ++j) DC += left[j];
    } else {      // top, but no left
      DC += DC;
    }
    DC = (DC + round) >> shift;
  } else if (left_status != 0) {   // left but no top
    for (j = 0; j < size; ++j) DC += left[j];
    DC += DC;
    DC = (DC + round) >> shift;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }
  Fill(dst, DC, size);
}

void Intra16Preds(uint8_t* dst,
                  const uint8_t* left,
                  const uint8_t* top,
                  int left_status, int top_status) {
  DCMode(I16DC16 + dst, left, top, 16, 16, 5, left_status, top_status);
  VerticalPred(I16VE16 + dst, top, 16, top_status);
  HorizontalPred(I16HE16 + dst, left, 16, left_status);
  TrueMotion(I16TM16 + dst, left, top, 16, left_status, top_status);
}

void IntraChromaPreds(uint8_t* dst, uint8_t* left, uint8_t* top,
                      int left_status, int top_status) {
  uint8_t* top_v;
  uint8_t* dst_v;
  uint8_t* left_v;
  // U block
  DCMode(C8DC8 + dst, left, top, 8, 8, 4, left_status, top_status);
  VerticalPred(C8VE8 + dst, top, 8, top_status);
  HorizontalPred(C8HE8 + dst, left, 8, left_status);
  TrueMotion(C8TM8 + dst, left, top, 8, left_status, top_status);
  // V block
  dst_v = dst + 8;
  if (top_status != 0) top_v = top + 8;
  if (left_status != 0) left_v = left + 16;
  DCMode(C8DC8 + dst_v, left_v, top_v, 8, 8, 4, left_status, top_status);
  VerticalPred(C8VE8 + dst_v, top_v, 8, top_status);
  HorizontalPred(C8HE8 + dst_v, left_v, 8, left_status);
  TrueMotion(C8TM8 + dst_v, left_v, top_v, 8, left_status, top_status);
}

void VP8MakeLuma16Preds(VP8EncIteratorVariable* const it_var,
                        VP8EncIteratorPointer* const it) {
  uint8_t* left = it->y_left_p + 1;
  uint8_t* top = it->y_top_p + it_var->x_ * 16;
  if (it_var->x_ == 0 && it_var->y_ == 0) {
    Intra16Preds(it->yuv_p_p, left, top, 0, 0);
  }
  else if (it_var->x_ == 0 && it_var->y_ != 0) {
    Intra16Preds(it->yuv_p_p, left, top, 0, 1);
  }
  else if (it_var->x_ != 0 && it_var->y_ == 0) {
    Intra16Preds(it->yuv_p_p, left, top, 1, 0);
  }
  else {
    Intra16Preds(it->yuv_p_p, left, top, 1, 1);
  }
}

void VP8MakeChroma8Preds(VP8EncIteratorVariable* const it_var,
                         VP8EncIteratorPointer* const it) {
  uint8_t* u_left = it->uv_left_p + 1;
  uint8_t* top = it->uv_top_p + it_var->x_ * 16;
  if (it_var->x_ == 0 && it_var->y_ == 0) {
    IntraChromaPreds(it->yuv_p_p, u_left, top, 0, 0);
  }
  else if (it_var->x_ == 0 && it_var->y_ != 0) {
    IntraChromaPreds(it->yuv_p_p, u_left, top, 0, 1);
  }
  else if (it_var->x_ != 0 && it_var->y_ == 0) {
    IntraChromaPreds(it->yuv_p_p, u_left, top, 1, 0);
  }
  else {
    IntraChromaPreds(it->yuv_p_p, u_left, top, 1, 1);
  }
}

void VP8SetIntra16Mode(VP8EncIteratorVariable* const it_var,
                       VP8EncIteratorPointer* const it, int mode) {
  int x = it_var->x_;
  uint8_t* preds_tmp = it->preds_p + 4 * x;
  int y, z;
  for (y = 0; y < 4; ++y) {
    for (z = 0; z < 4; z++) {
      preds_tmp[z] = mode;
    }
    preds_tmp += it_var->preds_w;
  }
  it->mb_info_p[5 * x + 0] = 1;
}

void VP8SetSkip(VP8EncIteratorVariable* const it_var,
                VP8EncIteratorPointer* const it, int skip) {
  int x = it_var->x_;
  it->mb_info_p[5 * x + 2] = skip;
}

int GetSSE(const uint8_t* a, const uint8_t* b,
           int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const int diff = (int)a[x + y * BPS] - b[x + y * BPS];
      count += diff * diff;
    }
  }
  return count;
}

int SSE16x16(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 16);
}

int SSE16x8(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 16, 8);
}

int SSE8x8(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 8, 8);
}

int SSE4x4(const uint8_t* a, const uint8_t* b) {
  return GetSSE(a, b, 4, 4);
}

uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(ref[(x) + (y) * BPS] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

void ITransformOne(const uint8_t* ref, const int16_t* in,
                   uint8_t* dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];
    const int b = in[0] - in[8];
    const int c = MUL(in[4], kC2) - MUL(in[12], kC1);
    const int d = MUL(in[4], kC1) + MUL(in[12], kC2);
    tmp[0] = a + d;
    tmp[1] = b + c;
    tmp[2] = b - c;
    tmp[3] = a - d;
    tmp += 4;
    in++;
  }

  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c = MUL(tmp[4], kC2) - MUL(tmp[12], kC1);
    const int d = MUL(tmp[4], kC1) + MUL(tmp[12], kC2);
    STORE(0, i, a + d);
    STORE(1, i, b + c);
    STORE(2, i, b - c);
    STORE(3, i, a - d);
    tmp++;
  }
}

void FTransform(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i, src += BPS, ref += BPS) {
    const int d0 = src[0] - ref[0];   // 9bit dynamic range ([-255,255])
    const int d1 = src[1] - ref[1];
    const int d2 = src[2] - ref[2];
    const int d3 = src[3] - ref[3];
    const int a0 = (d0 + d3);         // 10b                      [-510,510]
    const int a1 = (d1 + d2);
    const int a2 = (d1 - d2);
    const int a3 = (d0 - d3);
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

void FTransform2(const uint8_t* src, const uint8_t* ref, int16_t* out) {
  FTransform(src, ref, out);
  FTransform(src + 4, ref + 4, out + 16);
}

void FTransformWHT(const int16_t* in, int16_t* out) {
  // input is 12b signed
  int32_t tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += 64) {
    const int a0 = (in[0 * 16] + in[2 * 16]);  // 13b
    const int a1 = (in[1 * 16] + in[3 * 16]);
    const int a2 = (in[1 * 16] - in[3 * 16]);
    const int a3 = (in[0 * 16] - in[2 * 16]);
    tmp[0 + i * 4] = a0 + a1;   // 14b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[8 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[12+ i]);
    const int a2 = (tmp[4 + i] - tmp[12+ i]);
    const int a3 = (tmp[0 + i] - tmp[8 + i]);
    const int b0 = a0 + a1;    // 16b
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;
    out[ 0 + i] = b0 >> 1;     // 15b
    out[ 4 + i] = b1 >> 1;
    out[ 8 + i] = b2 >> 1;
    out[12 + i] = b3 >> 1;
  }
}

void TransformWHT(const int16_t* in, int16_t* out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
    const int a0 = in[0 + i] + in[12 + i];
    const int a1 = in[4 + i] + in[ 8 + i];
    const int a2 = in[4 + i] - in[ 8 + i];
    const int a3 = in[0 + i] - in[12 + i];
    tmp[0  + i] = a0 + a1;
    tmp[8  + i] = a0 - a1;
    tmp[4  + i] = a3 + a2;
    tmp[12 + i] = a3 - a2;
  }
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0 + i * 4] + 3;    // w/ rounder
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[ 0] = (a0 + a1) >> 3;
    out[16] = (a3 + a2) >> 3;
    out[32] = (a0 - a1) >> 3;
    out[48] = (a3 - a2) >> 3;
    out += 64;
  }
}

void ITransform(const uint8_t* ref, const int16_t* in,
                 uint8_t* dst, int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

int QuantizeBlockWHT(int16_t in[16], int16_t out[16],
                     const VP8Matrix* const mtx) {
  int n, last = -1;
  for (n = 0; n < 16; ++n) {
    const int j = kZigzag[n];
    const int sign = (in[j] < 0);
    const uint32_t coeff = sign ? -in[j] : in[j];
    assert(mtx->sharpen_[j] == 0);
    if (coeff > mtx->zthresh_[j]) {
      const uint32_t Q = mtx->q_[j];
      const uint32_t iQ = mtx->iq_[j];
      const uint32_t B = mtx->bias_[j];
      int level = QUANTDIV(coeff, iQ, B);
      if (level > MAX_LEVEL) level = MAX_LEVEL;
      if (sign) level = -level;
      in[j] = level * Q;
      out[n] = level;
      if (level) last = n;
    } else {
      out[n] = 0;
      in[j] = 0;
    }
  }
  return (last >= 0);
}

// Simple quantization
int QuantizeBlock(int16_t in[16], int16_t out[16],
                  const VP8Matrix* const mtx) {
  int last = -1;
  int n;
  for (n = 0; n < 16; ++n) {
    const int j = kZigzag[n];
    const int sign = (in[j] < 0);
    const uint32_t coeff = (sign ? -in[j] : in[j]) + mtx->sharpen_[j];
    if (coeff > mtx->zthresh_[j]) {
      const uint32_t Q = mtx->q_[j];
      const uint32_t iQ = mtx->iq_[j];
      const uint32_t B = mtx->bias_[j];
      int level = QUANTDIV(coeff, iQ, B);
      if (level > MAX_LEVEL) level = MAX_LEVEL;
      if (sign) level = -level;
      in[j] = level * Q;
      out[n] = level;
      if (level) last = n;
    } else {
      out[n] = 0;
      in[j] = 0;
    }
  }
  return (last >= 0);
}

int Quantize2Blocks(int16_t in[32], int16_t out[32],
                    const VP8Matrix* const mtx) {
  int nz;
  nz  = QuantizeBlock(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= QuantizeBlock(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}

// Convert packed context to byte array
#define BIT(nz, n) (!!((nz) & (1 << (n))))

void VP8IteratorNzToBytes(VP8EncIteratorVariable* const it_var,
                          VP8EncIteratorPointer* const it) {
  int x = it_var->x_;
  uint32_t* nz = it->nz_p + x + 1;
  const int tnz = nz[0], lnz = nz[-1];
  int* const top_nz = it->top_nz_p;
  int* const left_nz = it->left_nz_p;

  // Top-Y
  top_nz[0] = BIT(tnz, 12);
  top_nz[1] = BIT(tnz, 13);
  top_nz[2] = BIT(tnz, 14);
  top_nz[3] = BIT(tnz, 15);
  // Top-U
  top_nz[4] = BIT(tnz, 18);
  top_nz[5] = BIT(tnz, 19);
  // Top-V
  top_nz[6] = BIT(tnz, 22);
  top_nz[7] = BIT(tnz, 23);
  // DC
  top_nz[8] = BIT(tnz, 24);

  // left-Y
  left_nz[0] = BIT(lnz,  3);
  left_nz[1] = BIT(lnz,  7);
  left_nz[2] = BIT(lnz, 11);
  left_nz[3] = BIT(lnz, 15);
  // left-U
  left_nz[4] = BIT(lnz, 17);
  left_nz[5] = BIT(lnz, 19);
  // left-V
  left_nz[6] = BIT(lnz, 21);
  left_nz[7] = BIT(lnz, 23);
  // left-DC is special, iterated separately
}

void VP8IteratorBytesToNz(VP8EncIteratorVariable* const it_var,
                          VP8EncIteratorPointer* const it) {
  int x = it_var->x_;
  uint32_t nz = 0;
  uint32_t* const nz_tmp = it->nz_p + 1;
  const int* const top_nz = it->top_nz_p;
  const int* const left_nz = it->left_nz_p;
  // top
  nz |= (top_nz[0] << 12) | (top_nz[1] << 13);
  nz |= (top_nz[2] << 14) | (top_nz[3] << 15);
  nz |= (top_nz[4] << 18) | (top_nz[5] << 19);
  nz |= (top_nz[6] << 22) | (top_nz[7] << 23);
  nz |= (top_nz[8] << 24);  // we propagate the _top_ bit, esp. for intra4
  // left
  nz |= (left_nz[0] << 3) | (left_nz[1] << 7);
  nz |= (left_nz[2] << 11);
  nz |= (left_nz[4] << 17) | (left_nz[6] << 21);

  nz_tmp[x] = nz;
}

int ReconstructIntra16(VP8EncIteratorVariable* const it_var,
                       VP8EncIteratorPointer* const it,
                       VP8ModeScore* const rd,
                       VP8Matrix* matrix_y1_p,
                       VP8Matrix* matrix_y2_p,
                       uint8_t* yuv_out,
                       int mode) {
  int x = it_var->x_;
  const uint8_t* const ref = it->yuv_p_p + VP8I16ModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC;
  VP8Matrix* const dqm_y1 = matrix_y1_p + it->mb_info_p[5 * x + 3];
  VP8Matrix* const dqm_y2 = matrix_y2_p + it->mb_info_p[5 * x + 3];
  int nz = 0;
  int n;
  int16_t tmp[16][16], dc_tmp[16];
  int i, j;

  for (n = 0; n < 16; n += 2) {
    FTransform2(src + VP8Scan[n], ref + VP8Scan[n], tmp[n]);
  }
  FTransformWHT(tmp[0], dc_tmp);
  nz |= QuantizeBlockWHT(dc_tmp, rd->y_dc_levels, dqm_y2) << 24;

  for (n = 0; n < 16; n += 2) {
    // Zero-out the first coeff, so that: a) nz is correct below, and
    // b) finding 'last' non-zero coeffs in SetResidualCoeffs() is simplified.
    tmp[n][0] = tmp[n + 1][0] = 0;
    nz |= Quantize2Blocks(tmp[n], rd->y_ac_levels[n], dqm_y1) << n;
    assert(rd->y_ac_levels[n + 0][0] == 0);
    assert(rd->y_ac_levels[n + 1][0] == 0);
  }

  // Transform back
  TransformWHT(dc_tmp, tmp[0]);
  for (n = 0; n < 16; n += 2) {
    ITransform(ref + VP8Scan[n], tmp[n], yuv_out + VP8Scan[n], 1);
  }

  return nz;
}

int ReconstructUV(VP8EncIteratorVariable* const it_var,
                  VP8EncIteratorPointer* const it,
                  VP8ModeScore* const rd,
                  VP8Matrix* matrix_uv_p,
                  uint8_t* const yuv_out,
                  int mode) {
  int x = it_var->x_;
  const uint8_t* const ref = it->yuv_p_p + VP8UVModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_p + U_OFF_ENC;
  VP8Matrix* const dqm_uv = matrix_uv_p + it->mb_info_p[5 * x + 3];
  int nz = 0;
  int n;
  int16_t tmp[8][16];

  for (n = 0; n < 8; n += 2) {
    FTransform2(src + VP8ScanUV[n], ref + VP8ScanUV[n], tmp[n]);
  }

  for (n = 0; n < 8; n += 2) {
    nz |= Quantize2Blocks(tmp[n], rd->uv_levels[n], dqm_uv) << n;
  }

  for (n = 0; n < 8; n += 2) {
    ITransform(ref + VP8ScanUV[n], tmp[n], yuv_out + VP8ScanUV[n], 1);
  }
  return (nz << 16);
}

void RefineUsingDistortion(VP8EncIteratorVariable* const it_var,
                           VP8EncIteratorPointer* const it,
                           int try_both_modes, int refine_uv_mode,
                           VP8ModeScore* const rd,
                           VP8Matrix* matrix_y1_p,
                           VP8Matrix* matrix_y2_p,
                           VP8Matrix* matrix_uv_p) {
  score_t best_score = MAX_COST;
  score_t score_i4 = (score_t)I4_PENALTY;
  int16_t tmp_levels[16][16];
  uint8_t modes_i4[16];
  int x = it_var->x_;
  int nz = 0;
  int mode;
  int i, j;
  int is_i16 = try_both_modes || (it->mb_info_p[5 * x + 0] == 1);
  uint8_t tmp_out;

  if (is_i16) {   // First, evaluate Intra16 distortion
    int best_mode = -1;
    const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC;
    for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
      const uint8_t* const ref = it->yuv_p_p + VP8I16ModeOffsets[mode];
      const score_t score = SSE16x16(src, ref);
      if (score < best_score) {
        best_mode = mode;
        best_score = score;
      }
    }
    VP8SetIntra16Mode(it_var, it, best_mode);
    // we'll reconstruct later, if i16 mode actually gets selected
  }

  nz= ReconstructIntra16(it_var, it, rd, matrix_y1_p, matrix_y2_p, it->yuv_out_p + Y_OFF_ENC, it->preds_p[4 * x + 0]);

  nz |= ReconstructUV(it_var, it, rd, matrix_uv_p, it->yuv_out_p + U_OFF_ENC, it->mb_info_p[5 * x + 1]);

  rd->nz = nz;
  rd->score = best_score;
}

int VP8Decimate(EncloopInputData* const input,
                VP8EncIteratorVariable* const it_var,
                VP8EncIteratorPointer* const it,
                VP8ModeScore* const rd,
                VP8Matrix* matrix_y1_p,
                VP8Matrix* matrix_y2_p,
                VP8Matrix* matrix_uv_p) {
  int is_skipped;
  int rd_opt = input->rd_opt;
  const int method = input->method;

  InitScore(rd);

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.
  VP8MakeLuma16Preds(it_var, it);
  VP8MakeChroma8Preds(it_var, it);

  RefineUsingDistortion(it_var, it, (method >= 2), (method >= 1), rd, matrix_y1_p, matrix_y2_p, matrix_uv_p);

  is_skipped = (rd->nz == 0);
  VP8SetSkip(it_var, it, is_skipped);

  return is_skipped;
}

uint64_t VP8BitWriterPos(EncLoopOutputData* const output) {
  return (uint64_t)(output->pos + output->run) * 8 + 8 + output->nb_bits;
}

void SetResidualCoeffs(const int16_t* const coeffs,
                       VP8Residual* const res) {
  int n;
  res->last = -1;
  assert(res->first == 0 || coeffs[0] == 0);
  for (n = 15; n >= 0; --n) {
    if (coeffs[n]) {
      res->last = n;
      break;
    }
  }
}

void Flush(uint8_t* const bw, EncLoopOutputData* const output) {
  const int s = 8 + output->nb_bits;
  const int32_t bits = output->value >> s;
  assert(output->nb_bits >= 0);
  output->value -= bits << s;
  output->nb_bits -= 8;
  if ((bits & 0xff) != 0xff) {
    size_t pos = output->pos;
    if ((bits & 0x100) && pos > 0) {  // overflow -> propagate carry over pending 0xff's
      bw[pos - 1]++;
    }

    while (output->run > 0) {
      const int value = (bits & 0x100) ? 0x00 : 0xff;
      bw[pos++] = value;
      --output->run;
    }

    bw[pos++] = bits;
    output->pos = pos;

  } else {
    output->run++;   // delay writing of bytes 0xff, pending eventual carry.
  }
}

int VP8PutBit(uint8_t* const bw, EncLoopOutputData* const output, int bit, int prob) {
  const int split = (output->range * prob) >> 8;
  if (bit) {
    output->value += split + 1;
    output->range -= split + 1;
  } else {
    output->range = split;
  }
  if (output->range < 127) {   // emit 'shift' bits out and renormalize
    const int shift = kNorm[output->range];
    output->range = kNewRange[output->range];
    output->value <<= shift;
    output->nb_bits += shift;
    if (output->nb_bits > 0) Flush(bw, output);
  }
  return bit;
}

int VP8PutBitUniform(uint8_t* const bw, EncLoopOutputData* const output, int bit) {
  const int split = output->range >> 1;
  if (bit) {
    output->value += split + 1;
    output->range -= split + 1;
  } else {
    output->range = split;
  }
  if (output->range < 127) {
    output->range = kNewRange[output->range];
    output->value <<= 1;
    output->nb_bits += 1;
    if (output->nb_bits > 0) Flush(bw, output);
  }
  return bit;
}

int PutCoeffs(uint8_t* const bw,
              EncLoopOutputData* const output, int ctx,
              const int16_t* coeffs, const uint8_t* const prob,
              const VP8Residual* res) {
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const uint8_t* p = prob + n * BANDS_SIZE + ctx * CTX_SIZE;
  if (!VP8PutBit(bw, output, res->last >= 0, p[0])) {
    return 0;
  }

  while (n < 16) {
    const int c = coeffs[n++];
    const int sign = c < 0;
    int v = sign ? -c : c;
    if (!VP8PutBit(bw, output, v != 0, p[1])) {
      p = prob + VP8EncBands[n] * BANDS_SIZE + 0 * CTX_SIZE;
      continue;
    }
    if (!VP8PutBit(bw, output, v > 1, p[2])) {
      p = prob + VP8EncBands[n] * BANDS_SIZE + 1 * CTX_SIZE;
    } else {
      if (!VP8PutBit(bw, output, v > 4, p[3])) {
        if (VP8PutBit(bw, output, v != 2, p[4]))
          VP8PutBit(bw, output, v == 4, p[5]);
      } else if (!VP8PutBit(bw, output, v > 10, p[6])) {
        if (!VP8PutBit(bw, output, v > 6, p[7])) {
          VP8PutBit(bw, output, v == 6, 159);
        } else {
          VP8PutBit(bw, output, v >= 9, 165);
          VP8PutBit(bw, output, !(v & 1), 145);
        }
      } else {
        int mask;
        const uint8_t* tab;
        if (v < 3 + (8 << 1)) {          // VP8Cat3  (3b)
          VP8PutBit(bw, output, 0, p[8]);
          VP8PutBit(bw, output, 0, p[9]);
          v -= 3 + (8 << 0);
          mask = 1 << 2;
          tab = VP8Cat3;
        } else if (v < 3 + (8 << 2)) {   // VP8Cat4  (4b)
          VP8PutBit(bw, output, 0, p[8]);
          VP8PutBit(bw, output, 1, p[9]);
          v -= 3 + (8 << 1);
          mask = 1 << 3;
          tab = VP8Cat4;
        } else if (v < 3 + (8 << 3)) {   // VP8Cat5  (5b)
          VP8PutBit(bw, output, 1, p[8]);
          VP8PutBit(bw, output, 0, p[10]);
          v -= 3 + (8 << 2);
          mask = 1 << 4;
          tab = VP8Cat5;
        } else {                         // VP8Cat6 (11b)
          VP8PutBit(bw, output, 1, p[8]);
          VP8PutBit(bw, output, 1, p[10]);
          v -= 3 + (8 << 3);
          mask = 1 << 10;
          tab = VP8Cat6;
        }
        while (mask) {
          VP8PutBit(bw, output, !!(v & mask), *tab++);
          mask >>= 1;
        }
      }
      p = prob + VP8EncBands[n] * BANDS_SIZE + 2 * CTX_SIZE;
    }
    VP8PutBitUniform(bw, output, sign);
    if (n == 16 || !VP8PutBit(bw, output, n <= res->last, p[0])) {
      return 1;   // EOB
    }
  }
  return 1;
}

void CodeResiduals(EncLoopOutputData* const output,
                   VP8EncIteratorVariable* const it_var,
                   VP8EncIteratorPointer* const it,
                   VP8EncLoopPointer* loop,
                   const VP8ModeScore* const rd) {
  int x = it_var->x_;
  int i, j, ch;
  VP8Residual res;
  uint64_t pos1, pos2, pos3;
  const int i16 = (it->mb_info_p[5 * x + 0] == 1);
  const int segment = it->mb_info_p[5 * x + 3];
  const int16_t* coeffs;
  const uint8_t* proba;          // the cast maybe wrong. by wu

  VP8IteratorNzToBytes(it_var, it);

  pos1 = VP8BitWriterPos(output);
  if (i16) {
    res.first = 0;
    res.coeff_type = 1;
    coeffs = rd->y_dc_levels;
    proba = loop->coeffs_p + 1 * TYPES_SIZE;
    SetResidualCoeffs(rd->y_dc_levels, &res);
    it->top_nz_p[8] = it->left_nz_p[8] =
      PutCoeffs(loop->bw_buf_p, output, it->top_nz_p[8] + it->left_nz_p[8], coeffs, proba, &res);
    res.first = 1;
    res.coeff_type = 0;
    proba = loop->coeffs_p + 0 * TYPES_SIZE;
  } else {
    res.first = 0;
    res.coeff_type = 3;
    proba = loop->coeffs_p + 3 * TYPES_SIZE;
  }

  // luma-AC
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j) {
      const int ctx = it->top_nz_p[j] + it->left_nz_p[i];
      coeffs = rd->y_ac_levels[j + i * 4];
      SetResidualCoeffs(rd->y_ac_levels[j + i * 4], &res);
      it->top_nz_p[j] = it->left_nz_p[i] = PutCoeffs(loop->bw_buf_p, output, ctx, coeffs, proba, &res);
    }
  }
  pos2 = VP8BitWriterPos(output);

  // U/V
  res.first = 0;
  res.coeff_type = 2;
  proba = loop->coeffs_p + 2 * TYPES_SIZE;
  for (ch = 0; ch <= 2; ch += 2) {
    for (i = 0; i < 2; ++i) {
      for (j = 0; j < 2; ++j) {
        const int ctx = it->top_nz_p[4 + ch + j] + it->left_nz_p[4 + ch + i];
        coeffs = rd->uv_levels[ch * 2 + j + i * 2];
        SetResidualCoeffs(rd->uv_levels[ch * 2 + j + i * 2], &res);
        it->top_nz_p[4 + ch + j] = it->left_nz_p[4 + ch + i] =
            PutCoeffs(loop->bw_buf_p, output, ctx, coeffs, proba, &res);
      }
    }
  }
  pos3 = VP8BitWriterPos(output);
  it_var->luma_bits_ = pos2 - pos1;
  it_var->uv_bits_ = pos3 - pos2;
  loop->bit_count_p[segment * 4 + i16] += it_var->luma_bits_;
  loop->bit_count_p[segment * 4 + 2] += it_var->uv_bits_;
  VP8IteratorBytesToNz(it_var, it);
}

void ResetAfterSkip(VP8EncIteratorVariable* const it_var,
                    VP8EncIteratorPointer* const it) {
  int x = it_var->x_;
  uint32_t* nz = it->nz_p + 1;
  if (it->mb_info_p[5 * x + 0] == 1) {
    nz[x] = 0;  // reset all predictors
    it->left_nz_p[8] = 0;
  } else {
    nz[x] &= (1 << 24);  // preserve the dc_nz bit
  }
}

void StoreSSE(const VP8EncIteratorPointer* const it,
              VP8EncLoopPointer* loop) {
  const uint8_t* const in = it->yuv_in_p;
  const uint8_t* const out = it->yuv_out_p;
  // Note: not totally accurate at boundary. And doesn't include in-loop filter.
  loop->sse_p[0] = SSE16x16(in + Y_OFF_ENC, out + Y_OFF_ENC);
  loop->sse_p[1] = SSE8x8(in + U_OFF_ENC, out + U_OFF_ENC);
  loop->sse_p[2] = SSE8x8(in + V_OFF_ENC, out + V_OFF_ENC);
  loop->sse_count[0] += 16 * 16;
}

void StoreSideInfo(const VP8EncIteratorVariable* const it_var,
                   const VP8EncIteratorPointer* const it,
                   VP8EncLoopPointer* loop,
                   const EncloopInputData* const input,
                   EncLoopOutputData* output) {
  int x = it_var->x_;
  if (input->stats_add != 0) {
    StoreSSE(it, loop);
    loop->block_count_p[0] += (it->mb_info_p[5 * x + 0] == 0);
    loop->block_count_p[1] += (it->mb_info_p[5 * x + 0] == 1);
    loop->block_count_p[2] += (it->mb_info_p[5 * x + 2] != 0);
  }
}

void VP8IteratorSaveBoundary(const VP8EncIteratorVariable* const it_var,
                             VP8EncIteratorPointer* const it) {
  int i;
  const int x = it_var->x_, y = it_var->y_;
  uint8_t* const y_top = it->y_top_p + 16 * x;
  uint8_t* const uv_top = it->uv_top_p + 16 * x;
  uint8_t* const y_left = it->y_left_p + 1;
  uint8_t* const u_left = it->uv_left_p + 1;
  uint8_t* const v_left = it->uv_left_p + 1 + 16;
  const uint8_t* const ysrc = it->yuv_out_p + Y_OFF_ENC;
  const uint8_t* const uvsrc = it->yuv_out_p + U_OFF_ENC;
  if (x < it_var->mb_w - 1) {   // left
    for (i = 0; i < 16; ++i) {
      y_left[i] = ysrc[15 + i * BPS];
    }
    for (i = 0; i < 8; ++i) {
      u_left[i] = uvsrc[7 + i * BPS];
      v_left[i] = uvsrc[15 + i * BPS];
    }
    // top-left (before 'top'!)
    y_left[-1] = y_top[15];
    u_left[-1] = uv_top[0 + 7];
    v_left[-1] = uv_top[8 + 7];
  }
  if (y < it_var->mb_h - 1) {  // top
    for (i = 0; i < 16; i++) {
      y_top[i] = ysrc[15 * BPS + i];
    }
    for (i = 0; i < 16; i++) {
      uv_top[i] = uvsrc[7 * BPS + i];
    }
  }
}

void encloop (int* input_data,
              uint8_t* y,                               // input/output
              uint8_t* u,                               // input/output
              uint8_t* v,                               // input/output
              uint8_t* mb_info,                         // input/output
              uint8_t* preds,                           // input/output
              uint32_t* nz,                             // output
              uint8_t* y_top,                           // output          // maybe no need to read to host, to be confirem.
              uint8_t* uv_top,                          // output
              uint32_t* const quant_matrix,             // input
              uint8_t* const coeffs,                    // input
              uint32_t* const stats,                    // input
              uint16_t* const level_cost,               // input
              int* segment_data,                        // input
              uint8_t* bw_buf,                          // output
              uint64_t* sse,                            // output
              int* block_count,                         // output
              uint8_t* extra_info,                      // output
              int* max_edge,                            // output
              uint64_t* bit_count,                      // output
              uint64_t* sse_count,                      // output
              int32_t* output_data) {
#pragma HLS INTERFACE m_axi port=input_data offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=y offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=u offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=v offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=mb_info offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=preds offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=nz offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=y_top offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=uv_top offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=quant_matrix offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=coeffs offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=stats offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=level_cost offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=segment_data offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=bw_buf offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=sse offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=block_count offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=extra_info offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=max_edge offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=bit_count offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=sse_count offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=output_data offset=slave bundle=gmem

#pragma HLS INTERFACE s_axilite port=input_data bundle=control
#pragma HLS INTERFACE s_axilite port=y bundle=control
#pragma HLS INTERFACE s_axilite port=u bundle=control
#pragma HLS INTERFACE s_axilite port=v bundle=control
#pragma HLS INTERFACE s_axilite port=mb_info bundle=control
#pragma HLS INTERFACE s_axilite port=preds bundle=control
#pragma HLS INTERFACE s_axilite port=nz bundle=control
#pragma HLS INTERFACE s_axilite port=y_top bundle=control
#pragma HLS INTERFACE s_axilite port=uv_top bundle=control
#pragma HLS INTERFACE s_axilite port=quant_matrix bundle=control
#pragma HLS INTERFACE s_axilite port=coeffs bundle=control
#pragma HLS INTERFACE s_axilite port=stats bundle=control
#pragma HLS INTERFACE s_axilite port=level_cost bundle=control
#pragma HLS INTERFACE s_axilite port=segment_data bundle=control
#pragma HLS INTERFACE s_axilite port=bw_buf bundle=control
#pragma HLS INTERFACE s_axilite port=sse bundle=control
#pragma HLS INTERFACE s_axilite port=block_count bundle=control
#pragma HLS INTERFACE s_axilite port=extra_info bundle=control
#pragma HLS INTERFACE s_axilite port=max_edge bundle=control
#pragma HLS INTERFACE s_axilite port=bit_count bundle=control
#pragma HLS INTERFACE s_axilite port=sse_count bundle=control
#pragma HLS INTERFACE s_axilite port=output_data bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  int i, j;
  int ok;

  int y_offset, uv_offset, mb_offset, preds_offset;
  int y_length, uv_length, mb_length, preds_length;

  VP8EncIteratorPointer it;
  VP8EncIteratorVariable it_variable;
  VP8EncLoopPointer encloop;
  EncLoopOutputData output;
  EncloopInputData input;
  EncLoopOutputData* out = (EncLoopOutputData*)output_data;

  VP8Matrix matrix_y1_l[NUM_MB_SEGMENTS];
  VP8Matrix matrix_y2_l[NUM_MB_SEGMENTS];
  VP8Matrix matrix_uv_l[NUM_MB_SEGMENTS];

  input.width = input_data[0];
  input.height = input_data[1];
  input.filter_sharpness = input_data[2];
  input.show_compressed = input_data[3];
  input.extra_info_type = input_data[4];
  input.stats_add = input_data[5];
  input.simple = input_data[6];
  input.num_parts = input_data[7];
  input.max_i4_header_bits = input_data[8];
  input.lf_stats_status = input_data[9];
  input.use_skip_proba = input_data[10];
  input.method = input_data[11];
  input.rd_opt = input_data[12];

  output.range = out->range;
  output.value = out->value;
  output.run = out->run;
  output.nb_bits = out->nb_bits;
  output.pos = out->pos;
  output.max_pos = out->max_pos;
  output.error = out->error;

  const int width  = input.width;
  const int height = input.height;
  const int mb_w = (width + 15) >> 4;
  const int mb_h = (height + 15) >> 4;
  const int preds_w = 4 * mb_w + 1;
  const int preds_h = 4 * mb_h + 1;
  const int y_stride = width;
  const int uv_stride = (width + 1) >> 1;
  const int uv_width = (width + 1) >> 1;
  const int uv_height = (height + 1) >> 1;
  const size_t top_size = mb_w * 16;

  const int coeffs_size = NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS;

  it_variable.mb_w = (width + 15) >> 4;
  it_variable.mb_h = (height + 15) >> 4;
  it_variable.preds_w = 4 * it_variable.mb_w + 1;
  it_variable.preds_h = 4 * it_variable.mb_w + 1;
  it_variable.uv_width  = uv_width;
  it_variable.uv_height = uv_height;

  uint8_t y_l[LARGEST_Y_STRIDE * 16];
  uint8_t u_l[LARGEST_UV_STRIDE * 8];
  uint8_t v_l[LARGEST_UV_STRIDE * 8];

  it_variable.do_trellis_ = 0;
  it_variable.y_stride_  = y_stride;
  it_variable.uv_stride_ = uv_stride;

  for (i = 0; i < NUM_MB_SEGMENTS; i++) {
    uint32_t* matrix = quant_matrix + i * MATRIX_SIZE;
    for (j = 0; j < 16; j++) {
      matrix_y1_l[i].q_[j] = matrix[j + 0 * 16];
      matrix_y1_l[i].iq_[j] = matrix[j + 1 * 16];
      matrix_y1_l[i].bias_[j] = matrix[j + 2 * 16];
      matrix_y1_l[i].zthresh_[j] = matrix[j + 3 * 16];
      matrix_y1_l[i].sharpen_[j] = matrix[j + 4 * 16];
    }
  }

  for (i = 0; i < NUM_MB_SEGMENTS; i++) {
    uint32_t* matrix = quant_matrix + (NUM_MB_SEGMENTS + i) * MATRIX_SIZE;
    for (j = 0; j < 16; j++) {
      matrix_y2_l[i].q_[j] = matrix[j + 0 * 16];
      matrix_y2_l[i].iq_[j] = matrix[j + 1 * 16];
      matrix_y2_l[i].bias_[j] = matrix[j + 2 * 16];
      matrix_y2_l[i].zthresh_[j] = matrix[j + 3 * 16];
      matrix_y2_l[i].sharpen_[j] = matrix[j + 4 * 16];
    }
  }

  for (i = 0; i < NUM_MB_SEGMENTS; i++) {
    uint32_t* matrix = quant_matrix + (2 * NUM_MB_SEGMENTS + i) * MATRIX_SIZE;
    for (j = 0; j < 16; j++) {
      matrix_uv_l[i].q_[j] = matrix[j + 0 * 16];
      matrix_uv_l[i].iq_[j] = matrix[j + 1 * 16];
      matrix_uv_l[i].bias_[j] = matrix[j + 2 * 16];
      matrix_uv_l[i].zthresh_[j] = matrix[j + 3 * 16];
      matrix_uv_l[i].sharpen_[j] = matrix[j + 4 * 16];
    }
  }

  for (i = 0; i < 12; i++) {
    encloop.bit_count_p[i] = 0;
  }

  for (i = 0; i < 4; i++) {
    encloop.sse_p[i] = 0;
  }
  encloop.sse_count[0] = 0;

  memcpy(encloop.coeffs_p, coeffs, coeffs_size * sizeof(uint8_t));

  InitLeft(&it, &it_variable);
  memcpy(it.y_top_p, y_top, top_size * sizeof(uint8_t));          // init top
  memcpy(it.y_top_p, uv_top, top_size * sizeof(uint8_t));         // init top
  memcpy(it.nz_p, nz, (mb_w + 1) * sizeof(uint32_t));

  for (int index_h = 0; index_h < mb_h; index_h++) {
    it_variable.y_ = index_h;
    y_offset = index_h * y_stride * 16;
    y_length = y_stride * 16;

    uv_offset = index_h * uv_stride * 8;
    uv_length = uv_stride * 8;
    memcpy(y_l, y + y_offset, y_length * sizeof(uint8_t));
    memcpy(u_l, u + uv_offset, uv_length * sizeof(uint8_t));
    memcpy(v_l, v + uv_offset, uv_length * sizeof(uint8_t));

    mb_offset = index_h * 5 * mb_w;
    mb_length = 5 * mb_w;
    memcpy(it.mb_info_p, mb_info + mb_offset, mb_length * sizeof(uint8_t));

    preds_offset = index_h * 4 * preds_w;
    preds_length = 4 * preds_w;
    memcpy(it.preds_p, preds + preds_offset, preds_length * sizeof(uint8_t));

    for (int index_w = 0; index_w < mb_w; index_w++) {
      it_variable.x_ = index_w;
      VP8ModeScore info;
      const int dont_use_skip = input.use_skip_proba;

      VP8IteratorImport(&it, &it_variable, y_l, u_l, v_l);
      // Warning! order is important: first call VP8Decimate() and
      // *then* decide how to code the skip decision if there's one.
      int skip_status = !VP8Decimate(&input, &it_variable, &it, &info, matrix_y1_l, matrix_y2_l, matrix_uv_l) || dont_use_skip;

      if (skip_status) {
        CodeResiduals(&output, &it_variable, &it, &encloop, &info);
      } else {   // reset predictors after a skip
        ResetAfterSkip(&it_variable, &it);
      }
      StoreSideInfo(&it_variable, &it, &encloop, &input, &output);
      VP8IteratorSaveBoundary(&it_variable, &it);

      sse[0] += encloop.sse_p[0];
      sse[1] += encloop.sse_p[1];
      sse[2] += encloop.sse_p[2];
    }
    for (i = 0; i < 12; i++) {
      encloop.bit_count_p[i] = 0;
    }
    InitLeft(&it, &it_variable);

    memcpy(mb_info + mb_offset, it.mb_info_p, mb_length * sizeof(uint8_t));
    memcpy(preds + preds_offset, it.preds_p, preds_length * sizeof(uint8_t));
  }

  memcpy(nz, it.nz_p, (mb_w + 1) * sizeof(uint32_t));
  memcpy(bw_buf, encloop.bw_buf_p, 408000 * sizeof(uint8_t));
  memcpy(bit_count, encloop.bit_count_p, 12 * sizeof(int));
  // memcpy(sse, encloop.sse_p, 4 * sizeof(uint64_t));
  memcpy(block_count, encloop.block_count_p, 3 * sizeof(int));
  *sse_count = encloop.sse_count[0];

  output_data[0] = output.range;
  output_data[1] = output.value;
  output_data[2] = output.run;
  output_data[3] = output.nb_bits;
  output_data[4] = output.pos;
  output_data[5] = output.max_pos;
  output_data[6] = output.error;
}