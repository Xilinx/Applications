#pragma OPENCL EXTENSION cl_khr_fp64 : enable

typedef signed   char int8_t;
typedef unsigned char uint8_t;
typedef signed   short int16_t;
typedef unsigned short uint16_t;
typedef signed   int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;

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

typedef int64_t score_t;     // type used for scores, rate, distortion
// Note that MAX_COST is not the maximum allowed by sizeof(score_t),
// in order to allow overflowing computations.
#define MAX_COST ((score_t)0x7fffffffffffffLL)
#define QFIX 17
#define BIAS(b)  ((b) << (QFIX - 8))

#define DO_TRELLIS_I4  1
#define DO_TRELLIS_I16 1   // not a huge gain, but ok at low bitrate.
#define DO_TRELLIS_UV  0   // disable trellis for UV. Risky. Not worth.
#define USE_TDISTO 1

#define FLATNESS_LIMIT_I16 10      // I16 mode
#define FLATNESS_LIMIT_I4  3       // I4 mode
#define FLATNESS_LIMIT_UV  2       // UV mode
#define FLATNESS_PENALTY   140     // roughly ~1bit per block

#define I4_PENALTY 14000  // Rate-penalty for quick i4/i16 decision

// If a coefficient was quantized to a value Q (using a neutral bias),
// we test all alternate possibilities between [Q-MIN_DELTA, Q+MAX_DELTA]
// We don't test negative values though.
#define MIN_DELTA 0   // how much lower level to try
#define MAX_DELTA 1   // how much higher
#define NUM_NODES (MIN_DELTA + 1 + MAX_DELTA)
#define NODE(n, l) (nodes[(n)][(l) + MIN_DELTA])
#define SCORE_STATE(n, l) (score_states[n][(l) + MIN_DELTA])

#define RD_DISTO_MULT      256  // distortion multiplier (equivalent of lambda)

#define MULT_8B(a, b) (((a) * (b) + 128) >> 8)

#define SEGMENT_VISU 0

#define MAX_COEFF_THRESH                31   // size of histogram used by CollectHistogram.

#define LARGEST_Y_STRIDE                3840
#define LARGEST_UV_STRIDE               1920
#define LARGEST_MB_W                    240
#define LARGEST_MB_H                    135
#define LARGEST_PREDS_W                 (4 * LARGEST_MB_W + 1)
#define LARGEST_PREDS_H                 (4 * LARGEST_MB_H + 1)
#define MATRIX_SIZE                     112

#define NULL                            0

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

#define LEVEL_TYPES_SIZE                  NUM_BANDS * NUM_CTX * (MAX_VARIABLE_LEVEL + 1)
#define LEVEL_BANDS_SIZE                  NUM_CTX * (MAX_VARIABLE_LEVEL + 1)
#define LEVEL_CTX_SIZE                    (MAX_VARIABLE_LEVEL + 1)

typedef enum {   // Rate-distortion optimization levels
  RD_OPT_NONE        = 0,  // no rd-opt
  RD_OPT_BASIC       = 1,  // basic scoring (no trellis)
  RD_OPT_TRELLIS     = 2,  // perform trellis-quant on the final decision only
  RD_OPT_TRELLIS_ALL = 3   // trellis-quant for every scoring (much slower)
} VP8RDLevel;

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
}EncloopInputData;

typedef struct EncloopSegmentData {
  int quant[NUM_MB_SEGMENTS];
  int fstrength[NUM_MB_SEGMENTS];
  int min_disto[NUM_MB_SEGMENTS];
  int lambda_i16[NUM_MB_SEGMENTS];
  int lambda_i4[NUM_MB_SEGMENTS];
  int lambda_uv[NUM_MB_SEGMENTS];
  int lambda_mode[NUM_MB_SEGMENTS];
  int tlambda[NUM_MB_SEGMENTS];
  int lambda_trellis_i16[NUM_MB_SEGMENTS];
  int lambda_trellis_i4[NUM_MB_SEGMENTS];
  int lambda_trellis_uv[NUM_MB_SEGMENTS];
}EncloopSegmentData;

typedef struct EncLoopOutputData {
  int32_t range;
  int32_t value;
  int run;
  int nb_bits;
  size_t pos;
  size_t max_pos;
  int error;
}EncLoopOutputData;

typedef struct VP8Matrix {
  __local uint16_t q_[16];        // quantizer steps
  __local uint16_t iq_[16];       // reciprocals, fixed point.
  __local uint32_t bias_[16];     // rounding bias
  __local uint32_t zthresh_[16];  // value below which a coefficient is zeroed
  __local uint16_t sharpen_[16];  // frequency boosters for slight sharpening
} VP8Matrix;

typedef struct EncloopSizeData {
  int mb_w;
  int mb_h;
  int preds_w;
  int preds_h;
  int uv_width;
  int uv_height;
}EncloopSizeData;

typedef struct {
  __local uint8_t*   yuv_in_p;           // input samples
  __local uint8_t*   yuv_out_p;          // output samples
  __local uint8_t*   yuv_out2_p;         // secondary buffer swapped with yuv_out_.
  __local uint8_t*   yuv_p_p;            // scratch buffer for prediction

  __local uint8_t*   mb_info_p;
  __local uint8_t*   preds_p;            // intra mode predictors (4x4 blocks)
  __local uint32_t*  nz_p;               // non-zero pattern
  __local uint8_t*   i4_top_p;           // pointer to the current top boundary sample

  __local uint8_t*   y_left_p;           // left luma samples (addressable from index -1 to 15).
  __local uint8_t*   u_left_p;           // left u samples (addressable from index -1 to 7)
  __local uint8_t*   v_left_p;           // left v samples (addressable from index -1 to 7)

  __local uint8_t    i4_boundary_[37];   // 32+5 boundary samples needed by intra4x4
  __local int        top_nz_[9];         // top-non-zero context.
  __local int        left_nz_[9];        // left-non-zero. left_nz[8] is independent.
} VP8EncIteratorPointer;

/*typedef struct {
  __local uint8_t       i4_boundary_[37];  // 32+5 boundary samples needed by intra4x4
  __local int           top_nz_[9];        // top-non-zero context.
  __local int           left_nz_[9];       // left-non-zero. left_nz[8] is independent.
} VP8EncIteratorArray;*/

typedef struct {
  int           x_, y_;                      // current macroblock
  int           y_stride_, uv_stride_;       // respective strides
  int           i4_;                         // current intra4x4 mode being tested
  int           do_trellis_;                 // if true, perform extra level optimisation
  uint64_t      luma_bits_;                  // macroblock bit-cost for luma
  uint64_t      uv_bits_;                    // macroblock bit-cost for chroma
} VP8EncIteratorVariable;

typedef struct {
  __local VP8Matrix* matrix_y1_p;
  __local VP8Matrix* matrix_y2_p;
  __local VP8Matrix* matrix_uv_p;
  __local uint8_t* coeffs_p;
  __local uint32_t* stats_p;
  __local uint16_t* level_cost_p;
  __local uint8_t* bw_buf_p;
  __local uint8_t* extra_info_p;
  __local uint64_t* bit_count_p;
} VP8EncLoopPointer;

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

typedef struct {
  int16_t y_dc_levels[16];    // Quantized levels for luma-DC, luma-AC, chroma.
  int16_t y_ac_levels[16][16];
  int16_t uv_levels[4 + 4][16];
  int16_t modes_i4[16];       // mode numbers for intra4 predictions         // uint8_t -> int16_t need to check. by wu
} VP8ModeScoreP;

// Trellis node
typedef struct {
  int8_t prev;            // best previous node
  int8_t sign;            // sign of coeff_i
  int16_t level;          // level
} Node;

// Score state
typedef struct {
  __local score_t score;          // partial RD score
  __local const uint16_t* costs;  // shortcut to cost tables
} ScoreState;

typedef uint32_t proba_t;   // 16b + 16b
typedef uint8_t ProbaArray[NUM_CTX][NUM_PROBAS];
typedef proba_t StatsArray[NUM_CTX][NUM_PROBAS];
typedef uint16_t CostArray[NUM_CTX][MAX_VARIABLE_LEVEL + 1];
typedef const uint16_t* (*CostArrayPtr)[NUM_CTX];   // for easy casting
typedef const uint16_t* CostArrayMap[16][NUM_CTX];
typedef double LFStats[NUM_MB_SEGMENTS][MAX_LF_LEVELS];  // filter stats

typedef struct VP8Residual VP8Residual;
struct VP8Residual {
  int first;
  int last;
  int coeff_type;
};

typedef struct {
  double w, xm, ym, xxm, xym, yym;
} DistoStats;

////////////////////////////////////// Global array ////////////////////////////////////////
// Must be ordered using {DC_PRED, TM_PRED, V_PRED, H_PRED} as index
const int VP8I16ModeOffsets[4] = { I16DC16, I16TM16, I16VE16, I16HE16 };
const int VP8UVModeOffsets[4] = { C8DC8, C8TM8, C8VE8, C8HE8 };

// Must be indexed using {B_DC_PRED -> B_HU_PRED} as index
const int VP8I4ModeOffsets[NUM_BMODES] = {
  I4DC4, I4TM4, I4VE4, I4HE4, I4RD4, I4VR4, I4LD4, I4VL4, I4HD4, I4HU4
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

// Distortion measurement
static const uint16_t kWeightY[16] = {
  38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2
};

static const uint16_t kWeightTrellis[16] = {
#if USE_TDISTO == 0
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
#else
  30, 27, 19, 11,
  27, 24, 17, 10,
  19, 17, 12,  8,
  11, 10,  8,  6
#endif
};

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

static const uint8_t abs0[255 + 255 + 1] = {
  0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4,
  0xf3, 0xf2, 0xf1, 0xf0, 0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
  0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0, 0xdf, 0xde, 0xdd, 0xdc,
  0xdb, 0xda, 0xd9, 0xd8, 0xd7, 0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xd0,
  0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4,
  0xc3, 0xc2, 0xc1, 0xc0, 0xbf, 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8,
  0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1, 0xb0, 0xaf, 0xae, 0xad, 0xac,
  0xab, 0xaa, 0xa9, 0xa8, 0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0,
  0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94,
  0x93, 0x92, 0x91, 0x90, 0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88,
  0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80, 0x7f, 0x7e, 0x7d, 0x7c,
  0x7b, 0x7a, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
  0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
  0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a, 0x59, 0x58,
  0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x4e, 0x4d, 0x4c,
  0x4b, 0x4a, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40,
  0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34,
  0x33, 0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28,
  0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c,
  0x1b, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
  0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
  0x03, 0x02, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
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
  0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const int8_t sclip1[1020 + 1020 + 1] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
  0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
  0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
  0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
  0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
  0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
  0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
  0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
  0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f
};

static const int8_t sclip2[112 + 112 + 1] = {
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
  0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
};

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

const uint8_t VP8EncBands[16 + 1] = {
  0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
  0  // sentinel
};

const uint16_t VP8EntropyCost[256] = {
  1792, 1792, 1792, 1536, 1536, 1408, 1366, 1280, 1280, 1216,
  1178, 1152, 1110, 1076, 1061, 1024, 1024,  992,  968,  951,
   939,  911,  896,  878,  871,  854,  838,  820,  811,  794,
   786,  768,  768,  752,  740,  732,  720,  709,  704,  690,
   683,  672,  666,  655,  647,  640,  631,  622,  615,  607,
   598,  592,  586,  576,  572,  564,  559,  555,  547,  541,
   534,  528,  522,  512,  512,  504,  500,  494,  488,  483,
   477,  473,  467,  461,  458,  452,  448,  443,  438,  434,
   427,  424,  419,  415,  410,  406,  403,  399,  394,  390,
   384,  384,  377,  374,  370,  366,  362,  359,  355,  351,
   347,  342,  342,  336,  333,  330,  326,  323,  320,  316,
   312,  308,  305,  302,  299,  296,  293,  288,  287,  283,
   280,  277,  274,  272,  268,  266,  262,  256,  256,  256,
   251,  248,  245,  242,  240,  237,  234,  232,  228,  226,
   223,  221,  218,  216,  214,  211,  208,  205,  203,  201,
   198,  196,  192,  191,  188,  187,  183,  181,  179,  176,
   175,  171,  171,  168,  165,  163,  160,  159,  156,  154,
   152,  150,  148,  146,  144,  142,  139,  138,  135,  133,
   131,  128,  128,  125,  123,  121,  119,  117,  115,  113,
   111,  110,  107,  105,  103,  102,  100,   98,   96,   94,
    92,   91,   89,   86,   86,   83,   82,   80,   77,   76,
    74,   73,   71,   69,   67,   66,   64,   63,   61,   59,
    57,   55,   54,   52,   51,   49,   47,   46,   44,   43,
    41,   40,   38,   36,   35,   33,   32,   30,   29,   27,
    25,   24,   22,   21,   19,   18,   16,   15,   13,   12,
    10,    9,    7,    6,    4,    3
};

// fixed costs for coding levels, deduce from the coding tree.
// This is only the part that doesn't depend on the probability state.
const uint16_t VP8LevelFixedCosts[MAX_LEVEL + 1] = {
     0,  256,  256,  256,  256,  432,  618,  630,
   731,  640,  640,  828,  901,  948, 1021, 1101,
  1174, 1221, 1294, 1042, 1085, 1115, 1158, 1202,
  1245, 1275, 1318, 1337, 1380, 1410, 1453, 1497,
  1540, 1570, 1613, 1280, 1295, 1317, 1332, 1358,
  1373, 1395, 1410, 1454, 1469, 1491, 1506, 1532,
  1547, 1569, 1584, 1601, 1616, 1638, 1653, 1679,
  1694, 1716, 1731, 1775, 1790, 1812, 1827, 1853,
  1868, 1890, 1905, 1727, 1733, 1742, 1748, 1759,
  1765, 1774, 1780, 1800, 1806, 1815, 1821, 1832,
  1838, 1847, 1853, 1878, 1884, 1893, 1899, 1910,
  1916, 1925, 1931, 1951, 1957, 1966, 1972, 1983,
  1989, 1998, 2004, 2027, 2033, 2042, 2048, 2059,
  2065, 2074, 2080, 2100, 2106, 2115, 2121, 2132,
  2138, 2147, 2153, 2178, 2184, 2193, 2199, 2210,
  2216, 2225, 2231, 2251, 2257, 2266, 2272, 2283,
  2289, 2298, 2304, 2168, 2174, 2183, 2189, 2200,
  2206, 2215, 2221, 2241, 2247, 2256, 2262, 2273,
  2279, 2288, 2294, 2319, 2325, 2334, 2340, 2351,
  2357, 2366, 2372, 2392, 2398, 2407, 2413, 2424,
  2430, 2439, 2445, 2468, 2474, 2483, 2489, 2500,
  2506, 2515, 2521, 2541, 2547, 2556, 2562, 2573,
  2579, 2588, 2594, 2619, 2625, 2634, 2640, 2651,
  2657, 2666, 2672, 2692, 2698, 2707, 2713, 2724,
  2730, 2739, 2745, 2540, 2546, 2555, 2561, 2572,
  2578, 2587, 2593, 2613, 2619, 2628, 2634, 2645,
  2651, 2660, 2666, 2691, 2697, 2706, 2712, 2723,
  2729, 2738, 2744, 2764, 2770, 2779, 2785, 2796,
  2802, 2811, 2817, 2840, 2846, 2855, 2861, 2872,
  2878, 2887, 2893, 2913, 2919, 2928, 2934, 2945,
  2951, 2960, 2966, 2991, 2997, 3006, 3012, 3023,
  3029, 3038, 3044, 3064, 3070, 3079, 3085, 3096,
  3102, 3111, 3117, 2981, 2987, 2996, 3002, 3013,
  3019, 3028, 3034, 3054, 3060, 3069, 3075, 3086,
  3092, 3101, 3107, 3132, 3138, 3147, 3153, 3164,
  3170, 3179, 3185, 3205, 3211, 3220, 3226, 3237,
  3243, 3252, 3258, 3281, 3287, 3296, 3302, 3313,
  3319, 3328, 3334, 3354, 3360, 3369, 3375, 3386,
  3392, 3401, 3407, 3432, 3438, 3447, 3453, 3464,
  3470, 3479, 3485, 3505, 3511, 3520, 3526, 3537,
  3543, 3552, 3558, 2816, 2822, 2831, 2837, 2848,
  2854, 2863, 2869, 2889, 2895, 2904, 2910, 2921,
  2927, 2936, 2942, 2967, 2973, 2982, 2988, 2999,
  3005, 3014, 3020, 3040, 3046, 3055, 3061, 3072,
  3078, 3087, 3093, 3116, 3122, 3131, 3137, 3148,
  3154, 3163, 3169, 3189, 3195, 3204, 3210, 3221,
  3227, 3236, 3242, 3267, 3273, 3282, 3288, 3299,
  3305, 3314, 3320, 3340, 3346, 3355, 3361, 3372,
  3378, 3387, 3393, 3257, 3263, 3272, 3278, 3289,
  3295, 3304, 3310, 3330, 3336, 3345, 3351, 3362,
  3368, 3377, 3383, 3408, 3414, 3423, 3429, 3440,
  3446, 3455, 3461, 3481, 3487, 3496, 3502, 3513,
  3519, 3528, 3534, 3557, 3563, 3572, 3578, 3589,
  3595, 3604, 3610, 3630, 3636, 3645, 3651, 3662,
  3668, 3677, 3683, 3708, 3714, 3723, 3729, 3740,
  3746, 3755, 3761, 3781, 3787, 3796, 3802, 3813,
  3819, 3828, 3834, 3629, 3635, 3644, 3650, 3661,
  3667, 3676, 3682, 3702, 3708, 3717, 3723, 3734,
  3740, 3749, 3755, 3780, 3786, 3795, 3801, 3812,
  3818, 3827, 3833, 3853, 3859, 3868, 3874, 3885,
  3891, 3900, 3906, 3929, 3935, 3944, 3950, 3961,
  3967, 3976, 3982, 4002, 4008, 4017, 4023, 4034,
  4040, 4049, 4055, 4080, 4086, 4095, 4101, 4112,
  4118, 4127, 4133, 4153, 4159, 4168, 4174, 4185,
  4191, 4200, 4206, 4070, 4076, 4085, 4091, 4102,
  4108, 4117, 4123, 4143, 4149, 4158, 4164, 4175,
  4181, 4190, 4196, 4221, 4227, 4236, 4242, 4253,
  4259, 4268, 4274, 4294, 4300, 4309, 4315, 4326,
  4332, 4341, 4347, 4370, 4376, 4385, 4391, 4402,
  4408, 4417, 4423, 4443, 4449, 4458, 4464, 4475,
  4481, 4490, 4496, 4521, 4527, 4536, 4542, 4553,
  4559, 4568, 4574, 4594, 4600, 4609, 4615, 4626,
  4632, 4641, 4647, 3515, 3521, 3530, 3536, 3547,
  3553, 3562, 3568, 3588, 3594, 3603, 3609, 3620,
  3626, 3635, 3641, 3666, 3672, 3681, 3687, 3698,
  3704, 3713, 3719, 3739, 3745, 3754, 3760, 3771,
  3777, 3786, 3792, 3815, 3821, 3830, 3836, 3847,
  3853, 3862, 3868, 3888, 3894, 3903, 3909, 3920,
  3926, 3935, 3941, 3966, 3972, 3981, 3987, 3998,
  4004, 4013, 4019, 4039, 4045, 4054, 4060, 4071,
  4077, 4086, 4092, 3956, 3962, 3971, 3977, 3988,
  3994, 4003, 4009, 4029, 4035, 4044, 4050, 4061,
  4067, 4076, 4082, 4107, 4113, 4122, 4128, 4139,
  4145, 4154, 4160, 4180, 4186, 4195, 4201, 4212,
  4218, 4227, 4233, 4256, 4262, 4271, 4277, 4288,
  4294, 4303, 4309, 4329, 4335, 4344, 4350, 4361,
  4367, 4376, 4382, 4407, 4413, 4422, 4428, 4439,
  4445, 4454, 4460, 4480, 4486, 4495, 4501, 4512,
  4518, 4527, 4533, 4328, 4334, 4343, 4349, 4360,
  4366, 4375, 4381, 4401, 4407, 4416, 4422, 4433,
  4439, 4448, 4454, 4479, 4485, 4494, 4500, 4511,
  4517, 4526, 4532, 4552, 4558, 4567, 4573, 4584,
  4590, 4599, 4605, 4628, 4634, 4643, 4649, 4660,
  4666, 4675, 4681, 4701, 4707, 4716, 4722, 4733,
  4739, 4748, 4754, 4779, 4785, 4794, 4800, 4811,
  4817, 4826, 4832, 4852, 4858, 4867, 4873, 4884,
  4890, 4899, 4905, 4769, 4775, 4784, 4790, 4801,
  4807, 4816, 4822, 4842, 4848, 4857, 4863, 4874,
  4880, 4889, 4895, 4920, 4926, 4935, 4941, 4952,
  4958, 4967, 4973, 4993, 4999, 5008, 5014, 5025,
  5031, 5040, 5046, 5069, 5075, 5084, 5090, 5101,
  5107, 5116, 5122, 5142, 5148, 5157, 5163, 5174,
  5180, 5189, 5195, 5220, 5226, 5235, 5241, 5252,
  5258, 5267, 5273, 5293, 5299, 5308, 5314, 5325,
  5331, 5340, 5346, 4604, 4610, 4619, 4625, 4636,
  4642, 4651, 4657, 4677, 4683, 4692, 4698, 4709,
  4715, 4724, 4730, 4755, 4761, 4770, 4776, 4787,
  4793, 4802, 4808, 4828, 4834, 4843, 4849, 4860,
  4866, 4875, 4881, 4904, 4910, 4919, 4925, 4936,
  4942, 4951, 4957, 4977, 4983, 4992, 4998, 5009,
  5015, 5024, 5030, 5055, 5061, 5070, 5076, 5087,
  5093, 5102, 5108, 5128, 5134, 5143, 5149, 5160,
  5166, 5175, 5181, 5045, 5051, 5060, 5066, 5077,
  5083, 5092, 5098, 5118, 5124, 5133, 5139, 5150,
  5156, 5165, 5171, 5196, 5202, 5211, 5217, 5228,
  5234, 5243, 5249, 5269, 5275, 5284, 5290, 5301,
  5307, 5316, 5322, 5345, 5351, 5360, 5366, 5377,
  5383, 5392, 5398, 5418, 5424, 5433, 5439, 5450,
  5456, 5465, 5471, 5496, 5502, 5511, 5517, 5528,
  5534, 5543, 5549, 5569, 5575, 5584, 5590, 5601,
  5607, 5616, 5622, 5417, 5423, 5432, 5438, 5449,
  5455, 5464, 5470, 5490, 5496, 5505, 5511, 5522,
  5528, 5537, 5543, 5568, 5574, 5583, 5589, 5600,
  5606, 5615, 5621, 5641, 5647, 5656, 5662, 5673,
  5679, 5688, 5694, 5717, 5723, 5732, 5738, 5749,
  5755, 5764, 5770, 5790, 5796, 5805, 5811, 5822,
  5828, 5837, 5843, 5868, 5874, 5883, 5889, 5900,
  5906, 5915, 5921, 5941, 5947, 5956, 5962, 5973,
  5979, 5988, 5994, 5858, 5864, 5873, 5879, 5890,
  5896, 5905, 5911, 5931, 5937, 5946, 5952, 5963,
  5969, 5978, 5984, 6009, 6015, 6024, 6030, 6041,
  6047, 6056, 6062, 6082, 6088, 6097, 6103, 6114,
  6120, 6129, 6135, 6158, 6164, 6173, 6179, 6190,
  6196, 6205, 6211, 6231, 6237, 6246, 6252, 6263,
  6269, 6278, 6284, 6309, 6315, 6324, 6330, 6341,
  6347, 6356, 6362, 6382, 6388, 6397, 6403, 6414,
  6420, 6429, 6435, 3515, 3521, 3530, 3536, 3547,
  3553, 3562, 3568, 3588, 3594, 3603, 3609, 3620,
  3626, 3635, 3641, 3666, 3672, 3681, 3687, 3698,
  3704, 3713, 3719, 3739, 3745, 3754, 3760, 3771,
  3777, 3786, 3792, 3815, 3821, 3830, 3836, 3847,
  3853, 3862, 3868, 3888, 3894, 3903, 3909, 3920,
  3926, 3935, 3941, 3966, 3972, 3981, 3987, 3998,
  4004, 4013, 4019, 4039, 4045, 4054, 4060, 4071,
  4077, 4086, 4092, 3956, 3962, 3971, 3977, 3988,
  3994, 4003, 4009, 4029, 4035, 4044, 4050, 4061,
  4067, 4076, 4082, 4107, 4113, 4122, 4128, 4139,
  4145, 4154, 4160, 4180, 4186, 4195, 4201, 4212,
  4218, 4227, 4233, 4256, 4262, 4271, 4277, 4288,
  4294, 4303, 4309, 4329, 4335, 4344, 4350, 4361,
  4367, 4376, 4382, 4407, 4413, 4422, 4428, 4439,
  4445, 4454, 4460, 4480, 4486, 4495, 4501, 4512,
  4518, 4527, 4533, 4328, 4334, 4343, 4349, 4360,
  4366, 4375, 4381, 4401, 4407, 4416, 4422, 4433,
  4439, 4448, 4454, 4479, 4485, 4494, 4500, 4511,
  4517, 4526, 4532, 4552, 4558, 4567, 4573, 4584,
  4590, 4599, 4605, 4628, 4634, 4643, 4649, 4660,
  4666, 4675, 4681, 4701, 4707, 4716, 4722, 4733,
  4739, 4748, 4754, 4779, 4785, 4794, 4800, 4811,
  4817, 4826, 4832, 4852, 4858, 4867, 4873, 4884,
  4890, 4899, 4905, 4769, 4775, 4784, 4790, 4801,
  4807, 4816, 4822, 4842, 4848, 4857, 4863, 4874,
  4880, 4889, 4895, 4920, 4926, 4935, 4941, 4952,
  4958, 4967, 4973, 4993, 4999, 5008, 5014, 5025,
  5031, 5040, 5046, 5069, 5075, 5084, 5090, 5101,
  5107, 5116, 5122, 5142, 5148, 5157, 5163, 5174,
  5180, 5189, 5195, 5220, 5226, 5235, 5241, 5252,
  5258, 5267, 5273, 5293, 5299, 5308, 5314, 5325,
  5331, 5340, 5346, 4604, 4610, 4619, 4625, 4636,
  4642, 4651, 4657, 4677, 4683, 4692, 4698, 4709,
  4715, 4724, 4730, 4755, 4761, 4770, 4776, 4787,
  4793, 4802, 4808, 4828, 4834, 4843, 4849, 4860,
  4866, 4875, 4881, 4904, 4910, 4919, 4925, 4936,
  4942, 4951, 4957, 4977, 4983, 4992, 4998, 5009,
  5015, 5024, 5030, 5055, 5061, 5070, 5076, 5087,
  5093, 5102, 5108, 5128, 5134, 5143, 5149, 5160,
  5166, 5175, 5181, 5045, 5051, 5060, 5066, 5077,
  5083, 5092, 5098, 5118, 5124, 5133, 5139, 5150,
  5156, 5165, 5171, 5196, 5202, 5211, 5217, 5228,
  5234, 5243, 5249, 5269, 5275, 5284, 5290, 5301,
  5307, 5316, 5322, 5345, 5351, 5360, 5366, 5377,
  5383, 5392, 5398, 5418, 5424, 5433, 5439, 5450,
  5456, 5465, 5471, 5496, 5502, 5511, 5517, 5528,
  5534, 5543, 5549, 5569, 5575, 5584, 5590, 5601,
  5607, 5616, 5622, 5417, 5423, 5432, 5438, 5449,
  5455, 5464, 5470, 5490, 5496, 5505, 5511, 5522,
  5528, 5537, 5543, 5568, 5574, 5583, 5589, 5600,
  5606, 5615, 5621, 5641, 5647, 5656, 5662, 5673,
  5679, 5688, 5694, 5717, 5723, 5732, 5738, 5749,
  5755, 5764, 5770, 5790, 5796, 5805, 5811, 5822,
  5828, 5837, 5843, 5868, 5874, 5883, 5889, 5900,
  5906, 5915, 5921, 5941, 5947, 5956, 5962, 5973,
  5979, 5988, 5994, 5858, 5864, 5873, 5879, 5890,
  5896, 5905, 5911, 5931, 5937, 5946, 5952, 5963,
  5969, 5978, 5984, 6009, 6015, 6024, 6030, 6041,
  6047, 6056, 6062, 6082, 6088, 6097, 6103, 6114,
  6120, 6129, 6135, 6158, 6164, 6173, 6179, 6190,
  6196, 6205, 6211, 6231, 6237, 6246, 6252, 6263,
  6269, 6278, 6284, 6309, 6315, 6324, 6330, 6341,
  6347, 6356, 6362, 6382, 6388, 6397, 6403, 6414,
  6420, 6429, 6435, 5303, 5309, 5318, 5324, 5335,
  5341, 5350, 5356, 5376, 5382, 5391, 5397, 5408,
  5414, 5423, 5429, 5454, 5460, 5469, 5475, 5486,
  5492, 5501, 5507, 5527, 5533, 5542, 5548, 5559,
  5565, 5574, 5580, 5603, 5609, 5618, 5624, 5635,
  5641, 5650, 5656, 5676, 5682, 5691, 5697, 5708,
  5714, 5723, 5729, 5754, 5760, 5769, 5775, 5786,
  5792, 5801, 5807, 5827, 5833, 5842, 5848, 5859,
  5865, 5874, 5880, 5744, 5750, 5759, 5765, 5776,
  5782, 5791, 5797, 5817, 5823, 5832, 5838, 5849,
  5855, 5864, 5870, 5895, 5901, 5910, 5916, 5927,
  5933, 5942, 5948, 5968, 5974, 5983, 5989, 6000,
  6006, 6015, 6021, 6044, 6050, 6059, 6065, 6076,
  6082, 6091, 6097, 6117, 6123, 6132, 6138, 6149,
  6155, 6164, 6170, 6195, 6201, 6210, 6216, 6227,
  6233, 6242, 6248, 6268, 6274, 6283, 6289, 6300,
  6306, 6315, 6321, 6116, 6122, 6131, 6137, 6148,
  6154, 6163, 6169, 6189, 6195, 6204, 6210, 6221,
  6227, 6236, 6242, 6267, 6273, 6282, 6288, 6299,
  6305, 6314, 6320, 6340, 6346, 6355, 6361, 6372,
  6378, 6387, 6393, 6416, 6422, 6431, 6437, 6448,
  6454, 6463, 6469, 6489, 6495, 6504, 6510, 6521,
  6527, 6536, 6542, 6567, 6573, 6582, 6588, 6599,
  6605, 6614, 6620, 6640, 6646, 6655, 6661, 6672,
  6678, 6687, 6693, 6557, 6563, 6572, 6578, 6589,
  6595, 6604, 6610, 6630, 6636, 6645, 6651, 6662,
  6668, 6677, 6683, 6708, 6714, 6723, 6729, 6740,
  6746, 6755, 6761, 6781, 6787, 6796, 6802, 6813,
  6819, 6828, 6834, 6857, 6863, 6872, 6878, 6889,
  6895, 6904, 6910, 6930, 6936, 6945, 6951, 6962,
  6968, 6977, 6983, 7008, 7014, 7023, 7029, 7040,
  7046, 7055, 7061, 7081, 7087, 7096, 7102, 7113,
  7119, 7128, 7134, 6392, 6398, 6407, 6413, 6424,
  6430, 6439, 6445, 6465, 6471, 6480, 6486, 6497,
  6503, 6512, 6518, 6543, 6549, 6558, 6564, 6575,
  6581, 6590, 6596, 6616, 6622, 6631, 6637, 6648,
  6654, 6663, 6669, 6692, 6698, 6707, 6713, 6724,
  6730, 6739, 6745, 6765, 6771, 6780, 6786, 6797,
  6803, 6812, 6818, 6843, 6849, 6858, 6864, 6875,
  6881, 6890, 6896, 6916, 6922, 6931, 6937, 6948,
  6954, 6963, 6969, 6833, 6839, 6848, 6854, 6865,
  6871, 6880, 6886, 6906, 6912, 6921, 6927, 6938,
  6944, 6953, 6959, 6984, 6990, 6999, 7005, 7016,
  7022, 7031, 7037, 7057, 7063, 7072, 7078, 7089,
  7095, 7104, 7110, 7133, 7139, 7148, 7154, 7165,
  7171, 7180, 7186, 7206, 7212, 7221, 7227, 7238,
  7244, 7253, 7259, 7284, 7290, 7299, 7305, 7316,
  7322, 7331, 7337, 7357, 7363, 7372, 7378, 7389,
  7395, 7404, 7410, 7205, 7211, 7220, 7226, 7237,
  7243, 7252, 7258, 7278, 7284, 7293, 7299, 7310,
  7316, 7325, 7331, 7356, 7362, 7371, 7377, 7388,
  7394, 7403, 7409, 7429, 7435, 7444, 7450, 7461,
  7467, 7476, 7482, 7505, 7511, 7520, 7526, 7537,
  7543, 7552, 7558, 7578, 7584, 7593, 7599, 7610,
  7616, 7625, 7631, 7656, 7662, 7671, 7677, 7688,
  7694, 7703, 7709, 7729, 7735, 7744, 7750, 7761
};

// These are the fixed probabilities (in the coding trees) turned into bit-cost
// by calling VP8BitCost().
const uint16_t VP8FixedCostsUV[4] = { 302, 984, 439, 642 };

// note: these values include the fixed VP8BitCost(1, 145) mode selection cost.
const uint16_t VP8FixedCostsI16[4] = { 663, 919, 872, 919 };

const uint16_t VP8FixedCostsI4[NUM_BMODES][NUM_BMODES][NUM_BMODES] = {
  { {   40, 1151, 1723, 1874, 2103, 2019, 1628, 1777, 2226, 2137 },
    {  192,  469, 1296, 1308, 1849, 1794, 1781, 1703, 1713, 1522 },
    {  142,  910,  762, 1684, 1849, 1576, 1460, 1305, 1801, 1657 },
    {  559,  641, 1370,  421, 1182, 1569, 1612, 1725,  863, 1007 },
    {  299, 1059, 1256, 1108,  636, 1068, 1581, 1883,  869, 1142 },
    {  277, 1111,  707, 1362, 1089,  672, 1603, 1541, 1545, 1291 },
    {  214,  781, 1609, 1303, 1632, 2229,  726, 1560, 1713,  918 },
    {  152, 1037, 1046, 1759, 1983, 2174, 1358,  742, 1740, 1390 },
    {  512, 1046, 1420,  753,  752, 1297, 1486, 1613,  460, 1207 },
    {  424,  827, 1362,  719, 1462, 1202, 1199, 1476, 1199,  538 } },
  { {  240,  402, 1134, 1491, 1659, 1505, 1517, 1555, 1979, 2099 },
    {  467,  242,  960, 1232, 1714, 1620, 1834, 1570, 1676, 1391 },
    {  500,  455,  463, 1507, 1699, 1282, 1564,  982, 2114, 2114 },
    {  672,  643, 1372,  331, 1589, 1667, 1453, 1938,  996,  876 },
    {  458,  783, 1037,  911,  738,  968, 1165, 1518,  859, 1033 },
    {  504,  815,  504, 1139, 1219,  719, 1506, 1085, 1268, 1268 },
    {  333,  630, 1445, 1239, 1883, 3672,  799, 1548, 1865,  598 },
    {  399,  644,  746, 1342, 1856, 1350, 1493,  613, 1855, 1015 },
    {  622,  749, 1205,  608, 1066, 1408, 1290, 1406,  546,  971 },
    {  500,  753, 1041,  668, 1230, 1617, 1297, 1425, 1383,  523 } },
  { {  394,  553,  523, 1502, 1536,  981, 1608, 1142, 1666, 2181 },
    {  655,  430,  375, 1411, 1861, 1220, 1677, 1135, 1978, 1553 },
    {  690,  640,  245, 1954, 2070, 1194, 1528,  982, 1972, 2232 },
    {  559,  834,  741,  867, 1131,  980, 1225,  852, 1092,  784 },
    {  690,  875,  516,  959,  673,  894, 1056, 1190, 1528, 1126 },
    {  740,  951,  384, 1277, 1177,  492, 1579, 1155, 1846, 1513 },
    {  323,  775, 1062, 1776, 3062, 1274,  813, 1188, 1372,  655 },
    {  488,  971,  484, 1767, 1515, 1775, 1115,  503, 1539, 1461 },
    {  740, 1006,  998,  709,  851, 1230, 1337,  788,  741,  721 },
    {  522, 1073,  573, 1045, 1346,  887, 1046, 1146, 1203,  697 } },
  { {  105,  864, 1442, 1009, 1934, 1840, 1519, 1920, 1673, 1579 },
    {  534,  305, 1193,  683, 1388, 2164, 1802, 1894, 1264, 1170 },
    {  305,  518,  877, 1108, 1426, 3215, 1425, 1064, 1320, 1242 },
    {  683,  732, 1927,  257, 1493, 2048, 1858, 1552, 1055,  947 },
    {  394,  814, 1024,  660,  959, 1556, 1282, 1289,  893, 1047 },
    {  528,  615,  996,  940, 1201,  635, 1094, 2515,  803, 1358 },
    {  347,  614, 1609, 1187, 3133, 1345, 1007, 1339, 1017,  667 },
    {  218,  740,  878, 1605, 3650, 3650, 1345,  758, 1357, 1617 },
    {  672,  750, 1541,  558, 1257, 1599, 1870, 2135,  402, 1087 },
    {  592,  684, 1161,  430, 1092, 1497, 1475, 1489, 1095,  822 } },
  { {  228, 1056, 1059, 1368,  752,  982, 1512, 1518,  987, 1782 },
    {  494,  514,  818,  942,  965,  892, 1610, 1356, 1048, 1363 },
    {  512,  648,  591, 1042,  761,  991, 1196, 1454, 1309, 1463 },
    {  683,  749, 1043,  676,  841, 1396, 1133, 1138,  654,  939 },
    {  622, 1101, 1126,  994,  361, 1077, 1203, 1318,  877, 1219 },
    {  631, 1068,  857, 1650,  651,  477, 1650, 1419,  828, 1170 },
    {  555,  727, 1068, 1335, 3127, 1339,  820, 1331, 1077,  429 },
    {  504,  879,  624, 1398,  889,  889, 1392,  808,  891, 1406 },
    {  683, 1602, 1289,  977,  578,  983, 1280, 1708,  406, 1122 },
    {  399,  865, 1433, 1070, 1072,  764,  968, 1477, 1223,  678 } },
  { {  333,  760,  935, 1638, 1010,  529, 1646, 1410, 1472, 2219 },
    {  512,  494,  750, 1160, 1215,  610, 1870, 1868, 1628, 1169 },
    {  572,  646,  492, 1934, 1208,  603, 1580, 1099, 1398, 1995 },
    {  786,  789,  942,  581, 1018,  951, 1599, 1207,  731,  768 },
    {  690, 1015,  672, 1078,  582,  504, 1693, 1438, 1108, 2897 },
    {  768, 1267,  571, 2005, 1243,  244, 2881, 1380, 1786, 1453 },
    {  452,  899, 1293,  903, 1311, 3100,  465, 1311, 1319,  813 },
    {  394,  927,  942, 1103, 1358, 1104,  946,  593, 1363, 1109 },
    {  559, 1005, 1007, 1016,  658, 1173, 1021, 1164,  623, 1028 },
    {  564,  796,  632, 1005, 1014,  863, 2316, 1268,  938,  764 } },
  { {  266,  606, 1098, 1228, 1497, 1243,  948, 1030, 1734, 1461 },
    {  366,  585,  901, 1060, 1407, 1247,  876, 1134, 1620, 1054 },
    {  452,  565,  542, 1729, 1479, 1479, 1016,  886, 2938, 1150 },
    {  555, 1088, 1533,  950, 1354,  895,  834, 1019, 1021,  496 },
    {  704,  815, 1193,  971,  973,  640, 1217, 2214,  832,  578 },
    {  672, 1245,  579,  871,  875,  774,  872, 1273, 1027,  949 },
    {  296, 1134, 2050, 1784, 1636, 3425,  442, 1550, 2076,  722 },
    {  342,  982, 1259, 1846, 1848, 1848,  622,  568, 1847, 1052 },
    {  555, 1064, 1304,  828,  746, 1343, 1075, 1329, 1078,  494 },
    {  288, 1167, 1285, 1174, 1639, 1639,  833, 2254, 1304,  509 } },
  { {  342,  719,  767, 1866, 1757, 1270, 1246,  550, 1746, 2151 },
    {  483,  653,  694, 1509, 1459, 1410, 1218,  507, 1914, 1266 },
    {  488,  757,  447, 2979, 1813, 1268, 1654,  539, 1849, 2109 },
    {  522, 1097, 1085,  851, 1365, 1111,  851,  901,  961,  605 },
    {  709,  716,  841,  728,  736,  945,  941,  862, 2845, 1057 },
    {  512, 1323,  500, 1336, 1083,  681, 1342,  717, 1604, 1350 },
    {  452, 1155, 1372, 1900, 1501, 3290,  311,  944, 1919,  922 },
    {  403, 1520,  977, 2132, 1733, 3522, 1076,  276, 3335, 1547 },
    {  559, 1374, 1101,  615,  673, 2462,  974,  795,  984,  984 },
    {  547, 1122, 1062,  812, 1410,  951, 1140,  622, 1268,  651 } },
  { {  165,  982, 1235,  938, 1334, 1366, 1659, 1578,  964, 1612 },
    {  592,  422,  925,  847, 1139, 1112, 1387, 2036,  861, 1041 },
    {  403,  837,  732,  770,  941, 1658, 1250,  809, 1407, 1407 },
    {  896,  874, 1071,  381, 1568, 1722, 1437, 2192,  480, 1035 },
    {  640, 1098, 1012, 1032,  684, 1382, 1581, 2106,  416,  865 },
    {  559, 1005,  819,  914,  710,  770, 1418,  920,  838, 1435 },
    {  415, 1258, 1245,  870, 1278, 3067,  770, 1021, 1287,  522 },
    {  406,  990,  601, 1009, 1265, 1265, 1267,  759, 1017, 1277 },
    {  968, 1182, 1329,  788, 1032, 1292, 1705, 1714,  203, 1403 },
    {  732,  877, 1279,  471,  901, 1161, 1545, 1294,  755,  755 } },
  { {  111,  931, 1378, 1185, 1933, 1648, 1148, 1714, 1873, 1307 },
    {  406,  414, 1030, 1023, 1910, 1404, 1313, 1647, 1509,  793 },
    {  342,  640,  575, 1088, 1241, 1349, 1161, 1350, 1756, 1502 },
    {  559,  766, 1185,  357, 1682, 1428, 1329, 1897, 1219,  802 },
    {  473,  909, 1164,  771,  719, 2508, 1427, 1432,  722,  782 },
    {  342,  892,  785, 1145, 1150,  794, 1296, 1550,  973, 1057 },
    {  208, 1036, 1326, 1343, 1606, 3395,  815, 1455, 1618,  712 },
    {  228,  928,  890, 1046, 3499, 1711,  994,  829, 1720, 1318 },
    {  768,  724, 1058,  636,  991, 1075, 1319, 1324,  616,  825 },
    {  305, 1167, 1358,  899, 1587, 1587,  987, 1988, 1332,  501 } }
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

const int8_t* const VP8ksclip1 = &sclip1[1020];
const int8_t* const VP8ksclip2 = &sclip2[112];
const uint8_t* const VP8kclip1 = &clip1[255];
const uint8_t* const VP8kabs0 = &abs0[255];

////////////////////////////////////// function ////////////////////////////////////////////

int QUANTDIV(uint32_t n, uint32_t iQ, uint32_t B) {
  return (int)((n * iQ + B) >> QFIX);
}

void InitLeft(VP8EncIteratorPointer* const it,
              VP8EncIteratorVariable* const it_var) {
  int i;
  it->y_left_p[-1] = it->u_left_p[-1] = it->v_left_p[-1] =
      (it_var->y_ > 0) ? 129 : 127;

  for (i = 0; i < 16; i++) {
    it->y_left_p[i] = 129;
  }
  for (i = 0; i < 8; i++) {
    it->u_left_p[i] = 129;
  }
  for (i = 0; i < 8; i++) {
    it->v_left_p[i] = 129;
  }
  it->left_nz_[8] = 0;
}

void InitTop(VP8EncIteratorPointer* const it,
             __global uint8_t* y_top_p,
             __global uint8_t* uv_top_p,
             EncloopSizeData* const size) {
  int i;
  const size_t top_size = size->mb_w * 16;

  for (i = 0; i < top_size; i++) {
    y_top_p[i] = 127;
    uv_top_p[i] = 127;
  }

  for (i = 0; i < size->mb_w; i++) {
    it->nz_p[i] = 0;
  }
}

void VP8InitFilter(__local double* lf_stats_p,
                   int lf_stats_status) {
  if (lf_stats_status != 0) {
    int s, i;
    for (s = 0; s < NUM_MB_SEGMENTS; s++) {
      for (i = 0; i < MAX_LF_LEVELS; i++) {
        lf_stats_p[s * NUM_MB_SEGMENTS + i] = 0;
      }
    }
  }
}

void ImportBlock(__local const uint8_t* src, int src_stride,
                 __local uint8_t* dst, int w, int h) {
  int i, j;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j++) {
      dst[j] = src[j];
    }
    dst += BPS;
    src += src_stride;
  }
}

void VP8IteratorImport(VP8EncIteratorPointer* const it,
                       VP8EncIteratorVariable* const it_var,
                       __local uint8_t* y_l, __local uint8_t* u_l, __local uint8_t* v_l) {
  const int x = it_var->x_;
  const int stride_y = it_var->y_stride_;
  const int stride_uv = it_var->uv_stride_;

  __local const uint8_t* const ysrc = y_l + x * 16;
  __local const uint8_t* const usrc = u_l + x * 8;
  __local const uint8_t* const vsrc = v_l + x * 8;

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

void CopyScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  = src->D;
  dst->SD = src->SD;
  dst->R  = src->R;
  dst->H  = src->H;
  dst->nz = src->nz;      // note that nz is not accumulated, but just copied.
  dst->score = src->score;
}

void AddScore(VP8ModeScore* const dst, const VP8ModeScore* const src) {
  dst->D  += src->D;
  dst->SD += src->SD;
  dst->R  += src->R;
  dst->H  += src->H;
  dst->nz |= src->nz;     // here, new nz bits are accumulated.
  dst->score += src->score;
}

void Fill(__local uint8_t* dst, int value, int size) {
  int j, k;
  for (j = 0; j < size; ++j) {
    for (k = 0; k < size; k++) {
      dst[j * BPS + k] = value;
    }
  }
}

VerticalPred(__local uint8_t* dst,
             __global const uint8_t* top, int size,
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

void HorizontalPred(__local uint8_t* dst,
                    __local const uint8_t* left, int size,
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

void TrueMotion(__local uint8_t* dst, __local const uint8_t* left,
                __global const uint8_t* top, int size,
                int left_status, int top_status) {
  int y;
  if (left_status != 0) {
    if (top_status != 0) {
      const uint8_t* const clip = clip1 + 255 - left[-1];
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table = clip + left[y];
        int x;
        for (x = 0; x < size; ++x) {
          dst[x] = clip_table[top[x]];
        }
        dst += BPS;
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

void DCMode(__local uint8_t* dst,
            __local const uint8_t* left,
            __global const uint8_t* top,
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

void Intra16Preds(__local uint8_t* dst,
                  __local const uint8_t* left,
                  __global const uint8_t* top,
                  int left_status, int top_status) {
  DCMode(I16DC16 + dst, left, top, 16, 16, 5, left_status, top_status);
  VerticalPred(I16VE16 + dst, top, 16, top_status);
  HorizontalPred(I16HE16 + dst, left, 16, left_status);
  TrueMotion(I16TM16 + dst, left, top, 16, left_status, top_status);
}

void IntraChromaPreds(__local uint8_t* dst, __local const uint8_t* left,
                      __global const uint8_t* top,
                      int left_status, int top_status) {
  // U block
  DCMode(C8DC8 + dst, left, top, 8, 8, 4, left_status, top_status);
  VerticalPred(C8VE8 + dst, top, 8, top_status);
  HorizontalPred(C8HE8 + dst, left, 8, left_status);
  TrueMotion(C8TM8 + dst, left, top, 8, left_status, top_status);
  // V block
  dst += 8;
  if (top_status != 0) top += 8;
  if (left_status != 0) left += 16;
  DCMode(C8DC8 + dst, left, top, 8, 8, 4, left_status, top_status);
  VerticalPred(C8VE8 + dst, top, 8, top_status);
  HorizontalPred(C8HE8 + dst, left, 8, left_status);
  TrueMotion(C8TM8 + dst, left, top, 8, left_status, top_status);
}

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

void VE4(__local uint8_t* dst, __local const uint8_t* top) {    // vertical
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i, j;
  for (i = 0; i < 4; ++i) {
    for (j = 0; j < 4; j++) {
      dst[i * BPS + j] = vals[j];
    }
  }
}

void HE4(__local uint8_t* dst, __local const uint8_t* top) {    // horizontal
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  for (int i = 0; i < 4; i++) {
    dst[0 * BPS + i] = 0x01010101U * AVG3(X, I, J);                 // in source code, there is transfer from uint8_t to uint32_t. by wu
    dst[1 * BPS + i] = 0x01010101U * AVG3(I, J, K);                 // in source code, there is transfer from uint8_t to uint32_t. by wu
    dst[2 * BPS + i] = 0x01010101U * AVG3(J, K, L);                 // in source code, there is transfer from uint8_t to uint32_t. by wu
    dst[3 * BPS + i] = 0x01010101U * AVG3(K, L, L);                 // in source code, there is transfer from uint8_t to uint32_t. by wu
  }
}

void DC4(__local uint8_t* dst, __local const uint8_t* top) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += top[i] + top[-5 + i];
  Fill(dst, dc >> 3, 4);
}

void RD4(__local uint8_t* dst, __local const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 3)                                     = AVG3(J, K, L);
  DST(0, 2) = DST(1, 3)                         = AVG3(I, J, K);
  DST(0, 1) = DST(1, 2) = DST(2, 3)             = AVG3(X, I, J);
  DST(0, 0) = DST(1, 1) = DST(2, 2) = DST(3, 3) = AVG3(A, X, I);
  DST(1, 0) = DST(2, 1) = DST(3, 2)             = AVG3(B, A, X);
  DST(2, 0) = DST(3, 1)                         = AVG3(C, B, A);
  DST(3, 0)                                     = AVG3(D, C, B);
}

void LD4(__local uint8_t* dst, __local const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3)             = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3)                         = AVG3(F, G, H);
  DST(3, 3)                                     = AVG3(G, H, H);
}

void VR4(__local uint8_t* dst, __local const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0)             = AVG2(C, D);

  DST(0, 3) =             AVG3(K, J, I);
  DST(0, 2) =             AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) =             AVG3(B, C, D);
}

void VL4(__local uint8_t* dst, __local const uint8_t* top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0) =             AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);

  DST(0, 1) =             AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
              DST(3, 2) = AVG3(E, F, G);
              DST(3, 3) = AVG3(F, G, H);
}

void HU4(__local uint8_t* dst, __local const uint8_t* top) {
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
  DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

void HD4(__local uint8_t* dst, __local const uint8_t* top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3)             = AVG2(L, K);

  DST(3, 0)             = AVG3(A, B, C);
  DST(2, 0)             = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3)             = AVG3(L, K, J);
}

void TM4(__local uint8_t* dst, __local const uint8_t* top) {
  int x, y;
  const uint8_t* const clip = clip1 + 255 - top[-1];
  for (y = 0; y < 4; ++y) {
    const uint8_t* const clip_table = clip + top[-2 - y];
    for (x = 0; x < 4; ++x) {
      dst[x] = clip_table[top[x]];
    }
    dst += BPS;
  }
}

void Intra4Preds(__local uint8_t* dst, __local const uint8_t* top) {
  DC4(I4DC4 + dst, top);
  TM4(I4TM4 + dst, top);
  VE4(I4VE4 + dst, top);
  HE4(I4HE4 + dst, top);
  RD4(I4RD4 + dst, top);
  VR4(I4VR4 + dst, top);
  LD4(I4LD4 + dst, top);
  VL4(I4VL4 + dst, top);
  HD4(I4HD4 + dst, top);
  HU4(I4HU4 + dst, top);
}

void VP8MakeLuma16Preds(const VP8EncIteratorPointer* const it,
                        __global uint8_t* y_top_p,
                        VP8EncIteratorVariable* const it_var) {
  if (it_var->x_ == 0 && get_group_id(0) == 0) {
    Intra16Preds(it->yuv_p_p, it->y_left_p, y_top_p, 0, 0);
  }
  else if (it_var->x_ == 0 && get_group_id(0) != 0) {
    Intra16Preds(it->yuv_p_p, it->y_left_p, y_top_p, 0, 1);
  }
  else if (it_var->x_ != 0 && get_group_id(0) == 0) {
    Intra16Preds(it->yuv_p_p, it->y_left_p, y_top_p, 1, 0);
  }
  else {
    Intra16Preds(it->yuv_p_p, it->y_left_p, y_top_p, 1, 1);
  }
}

void VP8MakeChroma8Preds(const VP8EncIteratorPointer* const it,
                         __global uint8_t* uv_top_p,
                         VP8EncIteratorVariable* const it_var) {
  if (it_var->x_ == 0 && get_group_id(0) == 0) {
    IntraChromaPreds(it->yuv_p_p, it->u_left_p, uv_top_p, 0, 0);
  }
  else if (it_var->x_ == 0 && get_group_id(0) != 0) {
    IntraChromaPreds(it->yuv_p_p, it->u_left_p, uv_top_p, 0, 1);
  }
  else if (it_var->x_ != 0 && get_group_id(0) == 0) {
    IntraChromaPreds(it->yuv_p_p, it->u_left_p, uv_top_p, 1, 0);
  }
  else {
    IntraChromaPreds(it->yuv_p_p, it->u_left_p, uv_top_p, 1, 1);
  }
}

void VP8MakeIntra4Preds(const VP8EncIteratorPointer* const it) {
  Intra4Preds(it->yuv_p_p, it->i4_top_p);
}

uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(ref[(x) + (y) * BPS] + ((v) >> 3))

static const int kC1 = 20091 + (1 << 16);
static const int kC2 = 35468;
#define MUL(a, b) (((a) * (b)) >> 16)

void ITransformOne(__local const uint8_t* ref, const int16_t* in,
                   __local uint8_t* dst) {
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

void ITransform(__local const uint8_t* ref, const int16_t* in,
                __local  uint8_t* dst, int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

void FTransform(__local const uint8_t* src, __local const uint8_t* ref, int16_t* out) {
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

void FTransform2(__local const uint8_t* src, __local const uint8_t* ref, int16_t* out) {
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

int QuantizeBlockWHT(int16_t in[16], int16_t out[16],
                     __local const VP8Matrix* const mtx) {
  int n, last = -1;
  for (n = 0; n < 16; ++n) {
    const int j = kZigzag[n];
    const int sign = (in[j] < 0);
    const uint32_t coeff = sign ? -in[j] : in[j];
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

// Convert packed context to byte array
#define BIT(nz, n) (!!((nz) & (1 << (n))))

void VP8IteratorNzToBytes(VP8EncIteratorPointer* const it) {
  const int tnz = it->nz_p[0], lnz = it->nz_p[-1];
  __local int* const top_nz = it->top_nz_;
  __local int* const left_nz = it->left_nz_;

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

void VP8IteratorBytesToNz(VP8EncIteratorPointer* const it) {
  uint32_t nz = 0;
  __local const int* const top_nz = it->top_nz_;
  __local const int* const left_nz = it->left_nz_;
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

  *it->nz_p = nz;
}

// Cost of coding one event with probability 'proba'.
int VP8BitCost(int bit, uint8_t proba) {
  return !bit ? VP8EntropyCost[proba] : VP8EntropyCost[255 - proba];
}

void SetRDScore(int lambd, VP8ModeScore* const rd) {
  rd->score = (rd->R + rd->H) * lambd + RD_DISTO_MULT * (rd->D + rd->SD);
}

score_t RDScoreTrellis(int lambd, score_t rate,
                       score_t distortion) {
  return rate * lambd + RD_DISTO_MULT * distortion;
}

int VP8LevelCost(__local const uint16_t* const table, int level) {
  return VP8LevelFixedCosts[level]
       + table[(level > MAX_VARIABLE_LEVEL) ? MAX_VARIABLE_LEVEL : level];
}

int TrellisQuantizeBlock(__local uint8_t* coeffs_p,
                         __local uint16_t* level_cost_p,
                         int16_t in[16], int16_t out[16],
                         int ctx0, int coeff_type,
                         __local const VP8Matrix* const mtx,
                         int lambd) {
  __local const uint8_t* const probas = coeffs_p + coeff_type * TYPES_SIZE;         // the cast maybe wrong. by wu
  __local uint16_t* const costs = level_cost_p + coeff_type * LEVEL_TYPES_SIZE ;      // the cast maybe wrong. by wu
  const int first = (coeff_type == 0) ? 1 : 0;
  Node nodes[16][NUM_NODES];
  score_t ss_cur[NUM_NODES];
  score_t ss_prev[NUM_NODES];
  __local uint16_t* cs_cur[NUM_NODES];
  __local uint16_t* cs_prev[NUM_NODES];
  int best_path[3] = {-1, -1, -1};   // store best-last/best-level/best-previous
  score_t best_score;
  int n, m, p, last;
  int i;

  {
    score_t cost;
    const int thresh = mtx->q_[1] * mtx->q_[1] / 4;
    const int last_proba = probas[VP8EncBands[first] * BANDS_SIZE + ctx0 * CTX_SIZE + 0];

    // compute the position of the last interesting coefficient
    last = first - 1;
    for (n = 15; n >= first; --n) {
      const int j = kZigzag[n];
      const int err = in[j] * in[j];
      if (err > thresh) {
        last = n;
        break;
      }
    }
    // we don't need to go inspect up to n = 16 coeffs. We can just go up
    // to last + 1 (inclusive) without losing much.
    if (last < 15) ++last;

    // compute 'skip' score. This is the max score one can do.
    cost = VP8BitCost(0, last_proba);
    best_score = RDScoreTrellis(lambd, cost, 0);

    // initialize source node.
    for (m = -MIN_DELTA; m <= MAX_DELTA; ++m) {
      const score_t rate = (ctx0 == 0) ? VP8BitCost(1, last_proba) : 0;
      ss_cur[m] = RDScoreTrellis(lambd, rate, 0);
      cs_cur[m] = costs + VP8EncBands[first] * LEVEL_BANDS_SIZE + ctx0 * LEVEL_CTX_SIZE;
    }
  }

  // traverse trellis.
  for (n = first; n <= last; ++n) {
    const int j = kZigzag[n];
    const uint32_t Q  = mtx->q_[j];
    const uint32_t iQ = mtx->iq_[j];
    const uint32_t B = BIAS(0x00);     // neutral bias
    // note: it's important to take sign of the _original_ coeff,
    // so we don't have to consider level < 0 afterward.
    const int sign = (in[j] < 0);
    const uint32_t coeff0 = (sign ? -in[j] : in[j]) + mtx->sharpen_[j];
    int level0 = QUANTDIV(coeff0, iQ, B);
    if (level0 > MAX_LEVEL) level0 = MAX_LEVEL;

    {   // Swap current and previous score states
      score_t tmp_score;
      for (i = 0; i < NUM_NODES; i++) {
        tmp_score = ss_cur[i];
        ss_cur[i] = ss_prev[i];
        ss_prev[i] = tmp_score;
      }

      __local uint16_t* tmp_costs;
      for (i = 0; i < NUM_NODES; i++) {
        tmp_costs = cs_cur[i];
        cs_cur[i] = cs_prev[i];
        cs_prev[i] = tmp_costs;
      }
    }

    // test all alternate level values around level0.
    for (m = -MIN_DELTA; m <= MAX_DELTA; ++m) {
      Node* const cur = &NODE(n, m);
      int level = level0 + m;
      const int ctx = (level > 2) ? 2 : level;
      const int band = VP8EncBands[n + 1];
      score_t base_score, last_pos_score;
      score_t best_cur_score = MAX_COST;
      int best_prev = 0;   // default, in case

      ss_cur[m] = MAX_COST;
      cs_cur[m] = costs + VP8EncBands[n + 1] * LEVEL_BANDS_SIZE + ctx * LEVEL_CTX_SIZE;
      if (level > MAX_LEVEL || level < 0) {   // node is dead?
        continue;
      }

      // Compute extra rate cost if last coeff's position is < 15
      {
        const score_t last_pos_cost =
            (n < 15) ? VP8BitCost(0, probas[band * BANDS_SIZE + ctx * CTX_SIZE + 0]) : 0;
        last_pos_score = RDScoreTrellis(lambd, last_pos_cost, 0);
      }

      {
        // Compute delta_error = how much coding this level will
        // subtract to max_error as distortion.
        // Here, distortion = sum of (|coeff_i| - level_i * Q_i)^2
        const int new_error = coeff0 - level * Q;
        const int delta_error =
            kWeightTrellis[j] * (new_error * new_error - coeff0 * coeff0);
        base_score = RDScoreTrellis(lambd, 0, delta_error);
      }

      // Inspect all possible non-dead predecessors. Retain only the best one.
      for (p = -MIN_DELTA; p <= MAX_DELTA; ++p) {
        // Dead nodes (with ss_prev[p] >= MAX_COST) are automatically
        // eliminated since their score can't be better than the current best.
        const score_t cost = VP8LevelCost(cs_prev[p], level);
        // Examine node assuming it's a non-terminal one.
        const score_t score =
            base_score + ss_prev[p] + RDScoreTrellis(lambd, cost, 0);
        if (score < best_cur_score) {
          best_cur_score = score;
          best_prev = p;
        }
      }
      // Store best finding in current node.
      cur->sign = sign;
      cur->level = level;
      cur->prev = best_prev;
      ss_cur[m] = best_cur_score;

      // Now, record best terminal node (and thus best entry in the graph).
      if (level != 0) {
        const score_t score = best_cur_score + last_pos_score;
        if (score < best_score) {
          best_score = score;
          best_path[0] = n;                     // best eob position
          best_path[1] = m;                     // best node index
          best_path[2] = best_prev;             // best predecessor
        }
      }
    }
  }

  // Fresh start
  for (i = 0; i < (16 - first); i++) {
    in[first + i] = 0;
    out[first + i] = 0;
  }
  if (best_path[0] == -1) {
    return 0;   // skip!
  }

  {
    // Unwind the best path.
    // Note: best-prev on terminal node is not necessarily equal to the
    // best_prev for non-terminal. So we patch best_path[2] in.
    int nz = 0;
    int best_node = best_path[1];
    n = best_path[0];
    NODE(n, best_node).prev = best_path[2];   // force best-prev for terminal

    for (; n >= first; --n) {
      const Node* const node = &NODE(n, best_node);
      const int j = kZigzag[n];
      out[n] = node->sign ? -node->level : node->level;
      nz |= node->level;
      in[j] = out[n] * mtx->q_[j];
      best_node = node->prev;
    }
    return (nz != 0);
  }
}

// Simple quantization
int QuantizeBlock(int16_t in[16], int16_t out[16],
                  __local const VP8Matrix* const mtx) {
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
                    __local const VP8Matrix* const mtx) {
  int nz;
  nz  = QuantizeBlock(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= QuantizeBlock(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}

int ReconstructIntra16(__local EncloopSegmentData* const segment_data,
                       VP8EncIteratorPointer* const it,
                       VP8EncIteratorVariable* const it_var,
                       VP8EncLoopPointer* const encloop,
                       VP8ModeScore* const rd,
                       __local uint8_t* const yuv_out,
                       int mode) {
  __local const uint8_t* const ref = it->yuv_p_p + VP8I16ModeOffsets[mode];
  __local const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC;
  __local VP8Matrix* const dqm_y1 = &encloop->matrix_y1_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_y2 = &encloop->matrix_y2_p[it->mb_info_p[3]];
  int nz = 0;
  int n;
  int16_t tmp[16][16], dc_tmp[16];

  for (n = 0; n < 16; n += 2) {
    FTransform2(src + VP8Scan[n], ref + VP8Scan[n], tmp[n]);
  }
  FTransformWHT(tmp[0], dc_tmp);
  nz |= QuantizeBlockWHT(dc_tmp, rd->y_dc_levels, dqm_y2) << 24;

  if (DO_TRELLIS_I16 && it_var->do_trellis_) {
    int x, y;
    VP8IteratorNzToBytes(it);
    for (y = 0, n = 0; y < 4; ++y) {
      for (x = 0; x < 4; ++x, ++n) {
        const int ctx = it->top_nz_[x] + it->left_nz_[y];
        const int non_zero =
            TrellisQuantizeBlock(encloop->coeffs_p, encloop->level_cost_p,
                                 tmp[n], rd->y_ac_levels[n], ctx, 0,
                                 dqm_y1, segment_data->lambda_trellis_i16[it->mb_info_p[3]]);
        it->top_nz_[x] = it->left_nz_[y] = non_zero;
        rd->y_ac_levels[n][0] = 0;
        nz |= non_zero << n;
      }
    }
  } else {
    for (n = 0; n < 16; n += 2) {
      // Zero-out the first coeff, so that: a) nz is correct below, and
      // b) finding 'last' non-zero coeffs in SetResidualCoeffs() is simplified.
      tmp[n][0] = tmp[n + 1][0] = 0;
      nz |= Quantize2Blocks(tmp[n], rd->y_ac_levels[n], dqm_y1) << n;
      //assert(rd->y_ac_levels[n + 0][0] == 0);
      //assert(rd->y_ac_levels[n + 1][0] == 0);
    }
  }

  // Transform back
  TransformWHT(dc_tmp, tmp[0]);
  for (n = 0; n < 16; n += 2) {
    ITransform(ref + VP8Scan[n], tmp[n], yuv_out + VP8Scan[n], 1);
  }

  return nz;
}

int ReconstructIntra4(__local EncloopSegmentData* const segment_data,
                      VP8EncIteratorPointer* const it,
                      VP8EncIteratorVariable* const it_var,
                      VP8EncLoopPointer* const encloop,
                      int16_t levels[16],
                      __local const uint8_t* const src,
                      __local uint8_t* const yuv_out,
                      int mode) {
  __local const uint8_t* const ref = it->yuv_p_p + VP8I4ModeOffsets[mode];
  __local VP8Matrix* const dqm_y1 = &encloop->matrix_y1_p[it->mb_info_p[3]];
  int nz = 0;
  int16_t tmp[16];

  FTransform(src, ref, tmp);
  if (DO_TRELLIS_I4 && it_var->do_trellis_) {
    const int x = it_var->i4_ & 3, y = it_var->i4_ >> 2;
    const int ctx = it->top_nz_[x] + it->left_nz_[y];
    nz = TrellisQuantizeBlock(encloop->coeffs_p, encloop->level_cost_p,
                              tmp, levels, ctx, 3, dqm_y1,
                              segment_data->lambda_trellis_i4[it->mb_info_p[3]]);
  } else {
    nz = QuantizeBlock(tmp, levels, dqm_y1);
  }
  ITransform(ref, tmp, yuv_out, 0);
  return nz;
}

int ReconstructUV(__local EncloopSegmentData* const segment_data,
                  VP8EncIteratorPointer* const it,
                  VP8EncIteratorVariable* const it_var,
                  VP8EncLoopPointer* const encloop,
                  VP8ModeScore* const rd,
                  __local uint8_t* const yuv_out, int mode) {
  __local const uint8_t* const ref = it->yuv_p_p + VP8UVModeOffsets[mode];
  __local const uint8_t* const src = it->yuv_in_p + U_OFF_ENC;
  __local VP8Matrix* const dqm_uv = &encloop->matrix_uv_p[it->mb_info_p[3]];
  int nz = 0;
  int n;
  int16_t tmp[8][16];

  for (n = 0; n < 8; n += 2) {
    FTransform2(src + VP8ScanUV[n], ref + VP8ScanUV[n], tmp[n]);
  }

  if (DO_TRELLIS_UV && it_var->do_trellis_) {
    int ch, x, y;
    for (ch = 0, n = 0; ch <= 2; ch += 2) {
      for (y = 0; y < 2; ++y) {
        for (x = 0; x < 2; ++x, ++n) {
          const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
          const int non_zero =
              TrellisQuantizeBlock(encloop->coeffs_p, encloop->level_cost_p,
                                   tmp[n], rd->uv_levels[n], ctx, 2,
                                   dqm_uv, segment_data->lambda_trellis_uv[it->mb_info_p[3]]);
          it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] = non_zero;
          nz |= non_zero << n;
        }
      }
    }
  } else {
    for (n = 0; n < 8; n += 2) {
      nz |= Quantize2Blocks(tmp[n], rd->uv_levels[n], dqm_uv) << n;
    }
  }

  for (n = 0; n < 8; n += 2) {
    ITransform(ref + VP8ScanUV[n], tmp[n], yuv_out + VP8ScanUV[n], 1);
  }
  return (nz << 16);
}

int GetSSE(__local const uint8_t* a, __local const uint8_t* b,
           int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const int diff = (int)a[x] - b[x];
      count += diff * diff;
    }
    a += BPS;
    b += BPS;
  }
  return count;
}

int SSE16x16(__local const uint8_t* a, __local const uint8_t* b) {
  return GetSSE(a, b, 16, 16);
}

int SSE16x8(__local const uint8_t* a, __local const uint8_t* b) {
  return GetSSE(a, b, 16, 8);
}
int SSE8x8(__local const uint8_t* a, __local const uint8_t* b) {
  return GetSSE(a, b, 8, 8);
}
int SSE4x4(__local const uint8_t* a, __local const uint8_t* b) {
  return GetSSE(a, b, 4, 4);
}

int TTransform(__local const uint8_t* in, const uint16_t* w) {
  int sum = 0;
  int tmp[16];
  int i;
  // horizontal pass
  for (i = 0; i < 4; ++i, in += BPS) {
    const int a0 = in[0] + in[2];
    const int a1 = in[1] + in[3];
    const int a2 = in[1] - in[3];
    const int a3 = in[0] - in[2];
    tmp[0 + i * 4] = a0 + a1;
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  // vertical pass
  for (i = 0; i < 4; ++i, ++w) {
    const int a0 = tmp[0 + i] + tmp[8 + i];
    const int a1 = tmp[4 + i] + tmp[12+ i];
    const int a2 = tmp[4 + i] - tmp[12+ i];
    const int a3 = tmp[0 + i] - tmp[8 + i];
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;

    sum += w[ 0] * abs(b0);
    sum += w[ 4] * abs(b1);
    sum += w[ 8] * abs(b2);
    sum += w[12] * abs(b3);
  }
  return sum;
}

int Disto4x4(__local const uint8_t* const a,
             __local const uint8_t* const b,
             const uint16_t* const w) {
  const int sum1 = TTransform(a, w);
  const int sum2 = TTransform(b, w);
  return abs(sum2 - sum1) >> 5;
}

int Disto16x16(__local const uint8_t* const a,
               __local const uint8_t* const b,
               const uint16_t* const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4(a + x + y, b + x + y, w);
    }
  }
  return D;
}

int GetResidualCost(int ctx0, const VP8Residual* const res,
                    const int16_t* coeffs,
                    __local const uint8_t* const prob,
                    __local uint16_t* const costs) {
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const int p0 = prob[n * BANDS_SIZE + ctx0 * CTX_SIZE + 0];
  //CostArrayPtr const costs = costs;
  __local uint16_t* t = costs + VP8EncBands[n] * LEVEL_BANDS_SIZE + ctx0 * LEVEL_CTX_SIZE;
  // bit_cost(1, p0) is already incorporated in t[] tables, but only if ctx != 0
  // (as required by the syntax). For ctx0 == 0, we need to add it here or it'll
  // be missing during the loop.
  int cost = (ctx0 == 0) ? VP8BitCost(1, p0) : 0;

  if (res->last < 0) {
    return VP8BitCost(0, p0);
  }
  for (; n < res->last; ++n) {
    const int v = abs(coeffs[n]);
    const int ctx = (v >= 2) ? 2 : v;
    cost += VP8LevelCost(t, v);
    t = costs + VP8EncBands[n + 1] * LEVEL_BANDS_SIZE + ctx * LEVEL_CTX_SIZE;
  }
  // Last coefficient is always non-zero
  {
    const int v = abs(coeffs[n]);
    //assert(v != 0);
    cost += VP8LevelCost(t, v);
    if (n < 15) {
      const int b = VP8EncBands[n + 1];
      const int ctx = (v == 1) ? 1 : 2;
      const int last_p0 = prob[b * BANDS_SIZE + ctx * CTX_SIZE + 0];
      cost += VP8BitCost(0, last_p0);
    }
  }
  return cost;
}

void SetResidualCoeffs(const int16_t* const coeffs,
                       VP8Residual* const res) {
  int n;
  res->last = -1;
  //assert(res->first == 0 || coeffs[0] == 0);
  for (n = 15; n >= 0; --n) {
    if (coeffs[n]) {
      res->last = n;
      break;
    }
  }
}

int VP8GetCostLuma4(VP8EncIteratorPointer* const it,
                    VP8EncIteratorVariable* const it_var,
                    __local uint8_t* const coeffs_p,
                    __local uint16_t* const level_cost_p,
                    const int16_t levels[16]) {
  const int x = (it_var->i4_ & 3), y = (it_var->i4_ >> 2);
  __local const uint8_t* const proba_dc = coeffs_p + 3 * TYPES_SIZE;          // the cast maybe wrong. by wu
  __local uint16_t* const costs_dc = level_cost_p + 3 * LEVEL_TYPES_SIZE;      // the cast maybe wrong. by wu
  VP8Residual res;
  const int16_t* coeffs;
  int R = 0;
  int ctx;

  res.first = 0;
  res.coeff_type = 3;
  coeffs = levels;
  ctx = it->top_nz_[x] + it->left_nz_[y];
  SetResidualCoeffs(levels, &res);
  R += GetResidualCost(ctx, &res, coeffs, proba_dc, costs_dc);
  return R;
}

int VP8GetCostLuma16(VP8EncIteratorPointer* const it,
                     VP8EncIteratorVariable* const it_var,
                     __local uint8_t* const coeffs_p,
                     __local uint16_t* const level_cost_p,
                     const VP8ModeScore* const rd) {
  VP8Residual res;
  const int16_t* coeffs;
  __local const uint8_t* const proba_dc = coeffs_p + 1 * TYPES_SIZE;          // the cast maybe wrong. by wu
  __local uint16_t* const costs_dc = level_cost_p + 1 * LEVEL_TYPES_SIZE;      // the cast maybe wrong. by wu
  __local const uint8_t* const proba_ac = coeffs_p + 0 * TYPES_SIZE;          // the cast maybe wrong. by wu
  __local uint16_t* const costs_ac = level_cost_p + 0 * LEVEL_TYPES_SIZE;      // the cast maybe wrong. by wu
  int x, y;
  int R = 0;

  VP8IteratorNzToBytes(it);   // re-import the non-zero context
  int ctx = it->top_nz_[8] + it->left_nz_[8];
  // DC
  res.first = 0;
  res.coeff_type = 1;
  coeffs = rd->y_dc_levels;
  SetResidualCoeffs(rd->y_dc_levels, &res);
  R += GetResidualCost(it->top_nz_[8] + it->left_nz_[8], &res, coeffs, proba_dc, costs_dc);

  // AC
  res.first = 1;
  res.coeff_type = 0;
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      coeffs = rd->y_ac_levels[x + y * 4];
      SetResidualCoeffs(rd->y_ac_levels[x + y * 4], &res);
      R += GetResidualCost(ctx, &res, coeffs, proba_ac, costs_ac);
      it->top_nz_[x] = it->left_nz_[y] = (res.last >= 0);
    }
  }

  return R;
}

int VP8GetCostUV(VP8EncIteratorPointer* const it,
                 __local uint8_t* const coeffs_p,
                 __local uint16_t* const level_cost_p,
                 const VP8ModeScore* const rd) {
  VP8Residual res;
  const int16_t* coeffs;
  __local const uint8_t* const proba_dc = coeffs_p + 2 * TYPES_SIZE;          // the cast maybe wrong. by wu
  __local uint16_t* const costs_dc = level_cost_p + 2 * LEVEL_TYPES_SIZE;      // the cast maybe wrong. by wu
  int ch, x, y;
  int R = 0;

  VP8IteratorNzToBytes(it);  // re-import the non-zero context

  res.first = 0;
  res.coeff_type = 2;
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        coeffs = rd->uv_levels[ch * 2 + x + y * 2];
        SetResidualCoeffs(rd->uv_levels[ch * 2 + x + y * 2], &res);
        R += GetResidualCost(ctx, &res, coeffs, proba_dc, costs_dc);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] = (res.last >= 0);
      }
    }
  }
  return R;
}

score_t IsFlat(const int16_t* levels, int num_blocks, score_t thresh) {
  score_t score = 0;
  while (num_blocks-- > 0) {      // TODO(skal): refine positional scoring?
    int i;
    for (i = 1; i < 16; ++i) {    // omit DC, we're only interested in AC
      score += (levels[i] != 0);
      if (score > thresh) return 0;
    }
    levels += 16;
  }
  return 1;
}

void VP8SetIntra16Mode(const VP8EncIteratorPointer* const it,
                       EncloopSizeData* const size_data, int mode) {
  __local uint8_t* preds = it->preds_p;
  int y, z;
  for (y = 0; y < 4; ++y) {
    for (z = 0; z < 4; z++) {
      preds[z] = mode;
    }
    preds += size_data->preds_w;
  }
  it->mb_info_p[0] = 1;
}

void VP8SetIntra4Mode(const VP8EncIteratorPointer* const it,
                      EncloopSizeData* const size_data, const uint8_t* modes) {
  __local uint8_t* preds = it->preds_p;
  int y, z;
  for (y = 4; y > 0; --y) {
    for (z = 0; z < 4; z++) {
      preds[z] = modes[z];
    }
    preds += size_data->preds_w;
    modes += 4;
  }
  it->mb_info_p[0] = 0;
}

void VP8SetIntraUVMode(const VP8EncIteratorPointer* const it, int mode) {
  it->mb_info_p[1] = mode;
}

void VP8SetSkip(const VP8EncIteratorPointer* const it, int skip) {
  it->mb_info_p[2] = skip;
}

void VP8SetSegment(const VP8EncIteratorPointer* const it, int segment) {
  it->mb_info_p[3] = segment;
}

void StoreMaxDelta(__local int* max_edge_l, const int16_t DCs[16]) {
  // We look at the first three AC coefficients to determine what is the average
  // delta between each sub-4x4 block.
  const int v0 = abs(DCs[1]);
  const int v1 = abs(DCs[4]);
  const int v2 = abs(DCs[5]);
  int max_v = (v0 > v1) ? v1 : v0;
  max_v = (v2 > max_v) ? v2 : max_v;
  if (max_v > *max_edge_l) *max_edge_l = max_v;
}

void PickBestIntra16(EncloopInputData* const input_data,
                     __local EncloopSegmentData* const segment_data,
                     VP8EncIteratorPointer* const it,
                     VP8EncIteratorVariable* const it_var,
                     VP8EncLoopPointer* const encloop,
                     EncloopSizeData* const size_data,
                     __local int* max_edge_l,
                     VP8ModeScore* rd) {
  const int kNumBlocks = 16;
  __local VP8Matrix* const dqm_y1 = &encloop->matrix_y1_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_y2 = &encloop->matrix_y2_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_uv = &encloop->matrix_uv_p[it->mb_info_p[3]];
  const int lambd = segment_data->lambda_i16[it->mb_info_p[3]];
  const int tlambda = segment_data->tlambda[it->mb_info_p[3]];
  __local const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC;
  VP8ModeScore rd_tmp;
  VP8ModeScore* rd_cur = &rd_tmp;
  VP8ModeScore* rd_best = rd;
  int mode;
  int i, j;
  uint8_t tmp_out;
  uint64_t tmp_score;
  int16_t tmp_levels;
  int tmp_mode;
  uint32_t tmp_nz;
  uint8_t tmp_i4;

  rd->mode_i16 = -1;
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    __local uint8_t* const tmp_dst = it->yuv_out2_p + Y_OFF_ENC;  // scratch buffer
    rd_cur->mode_i16 = mode;

    // Reconstruct
    rd_cur->nz = ReconstructIntra16(segment_data, it, it_var, encloop, rd_cur, tmp_dst, mode);

    // Measure RD-score
    rd_cur->D = SSE16x16(src, tmp_dst);
    rd_cur->SD =
        tlambda ? MULT_8B(tlambda, Disto16x16(src, tmp_dst, kWeightY)) : 0;
    rd_cur->H = VP8FixedCostsI16[mode];
    rd_cur->R = VP8GetCostLuma16(it, it_var, encloop->coeffs_p, encloop->level_cost_p, rd_cur);
    if (mode > 0 &&
        IsFlat(rd_cur->y_ac_levels[0], kNumBlocks, FLATNESS_LIMIT_I16)) {
      // penalty to avoid flat area to be mispredicted by complex mode
      rd_cur->R += FLATNESS_PENALTY * kNumBlocks;
    }

    // Since we always examine Intra16 first, we can overwrite *rd directly.
    SetRDScore(lambd, rd_cur);
    if (mode == 0 || rd_cur->score < rd_best->score) {
      tmp_score = rd_cur->D;
      rd_cur->D = rd_best->D;
      rd_best->D = tmp_score;
      tmp_score = rd_cur->SD;
      rd_cur->SD = rd_best->SD;
      rd_best->SD = tmp_score;
      tmp_score = rd_cur->H;
      rd_cur->H = rd_best->H;
      rd_best->H = tmp_score;
      tmp_score = rd_cur->R;
      rd_cur->R = rd_best->R;
      rd_best->R = tmp_score;
      tmp_score = rd_cur->score;
      rd_cur->score = rd_best->score;
      rd_best->score = tmp_score;
      for (i = 0; i < 16; i++) {
        tmp_levels = rd_cur->y_dc_levels[i];
        rd_cur->y_dc_levels[i] = rd_best->y_dc_levels[i];
        rd_best->y_dc_levels[i] = tmp_levels;
      }
      for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
          tmp_levels = rd_cur->y_ac_levels[i][j];
          rd_cur->y_ac_levels[i][j] = rd_best->y_ac_levels[i][j];
          rd_best->y_ac_levels[i][j] = tmp_levels;
        }
      }
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 16; j++) {
          tmp_levels = rd_cur->uv_levels[i][j];
          rd_cur->uv_levels[i][j] = rd_best->uv_levels[i][j];
          rd_best->uv_levels[i][j] = tmp_levels;
        }
      }
      tmp_mode = rd_cur->mode_i16;
      rd_cur->mode_i16 = rd_best->mode_i16;
      rd_best->mode_i16 = tmp_mode;
      for (i = 0; i < 16; i++) {
        tmp_i4 = rd_cur->modes_i4[i];
        rd_cur->modes_i4[i] = rd_best->modes_i4[i];
        rd_best->modes_i4[i] = tmp_i4;
      }
      tmp_mode = rd_cur->mode_uv;
      rd_cur->mode_uv = rd_best->mode_uv;
      rd_best->mode_uv = tmp_mode;
      tmp_nz = rd_cur->nz;
      rd_cur->nz = rd_best->nz;
      rd_best->nz = tmp_nz;

      for (i = 0; i < YUV_SIZE_ENC; i++) {
        tmp_out = it->yuv_out_p[i];
        it->yuv_out_p[i] = it->yuv_out2_p[i];
        it->yuv_out2_p[i] = tmp_out;
      }
    }
  }

  SetRDScore(segment_data->lambda_mode[it->mb_info_p[3]], rd);   // finalize score for mode decision.
  VP8SetIntra16Mode(it, size_data, rd->mode_i16);

  // we have a blocky macroblock (only DCs are non-zero) with fairly high
  // distortion, record max delta so we can later adjust the minimal filtering
  // strength needed to smooth these blocks out.
  if ((rd->nz & 0xffff) == 0 && rd->D > segment_data->min_disto[it->mb_info_p[3]]) {
    StoreMaxDelta(&max_edge_l[it->mb_info_p[3]], rd->y_dc_levels);
  }
}

// Array to record the position of the top sample to pass to the prediction
// functions in dsp.c.
static const uint8_t VP8TopLeftI4[16] = {
  17, 21, 25, 29,
  13, 17, 21, 25,
  9,  13, 17, 21,
  5,   9, 13, 17
};

void VP8IteratorStartI4(VP8EncIteratorPointer* const it,
                        VP8EncIteratorVariable* const it_var,
                        EncloopSizeData* const size_data,
                        __global uint8_t* y_top_p) {
  int i;

  it_var->i4_ = 0;    // first 4x4 sub-block
  it->i4_top_p = it->i4_boundary_ + VP8TopLeftI4[0];

  // Import the boundary samples
  for (i = 0; i < 17; ++i) {    // left
    it->i4_boundary_[i] = it->y_left_p[15 - i];
  }
  for (i = 0; i < 16; ++i) {    // top
    it->i4_boundary_[17 + i] = y_top_p[i];
  }
  // top-right samples have a special case on the far right of the picture
  if (it_var->x_ < size_data->mb_w - 1) {
    for (i = 16; i < 16 + 4; ++i) {
      it->i4_boundary_[17 + i] = y_top_p[i];
    }
  } else {    // else, replicate the last valid pixel four times
    for (i = 16; i < 16 + 4; ++i) {
      it->i4_boundary_[17 + i] = it->i4_boundary_[17 + 15];
    }
  }
  VP8IteratorNzToBytes(it);  // import the non-zero context
}

int VP8IteratorRotateI4(VP8EncIteratorPointer* const it,
                        VP8EncIteratorVariable* const it_var,
                        __local const uint8_t* const yuv_out) {
  __local const uint8_t* const blk = yuv_out + VP8Scan[it_var->i4_];
  __local uint8_t* const top = it->i4_top_p;
  int i;

  // Update the cache with 7 fresh samples
  for (i = 0; i <= 3; ++i) {
    top[-4 + i] = blk[i + 3 * BPS];   // store future top samples
  }
  if ((it_var->i4_ & 3) != 3) {  // if not on the right sub-blocks #3, #7, #11, #15
    for (i = 0; i <= 2; ++i) {        // store future left samples
      top[i] = blk[3 + (2 - i) * BPS];
    }
  } else {  // else replicate top-right samples, as says the specs.
    for (i = 0; i <= 3; ++i) {
      top[i] = top[i + 4];
    }
  }
  // move pointers to next sub-block
  ++it_var->i4_;
  if (it_var->i4_ == 16) {    // we're done
    return 0;
  }

  it->i4_top_p = it->i4_boundary_ + VP8TopLeftI4[it_var->i4_];
  return 1;
}

uint16_t* GetCostModeI4(VP8EncIteratorPointer* const it,
                        VP8EncIteratorVariable* const it_var,
                        EncloopSizeData* const size_data,
                        const uint8_t modes[16]) {
  const int preds_w = size_data->preds_w;
  const int x = (it_var->i4_ & 3), y = it_var->i4_ >> 2;
  const int left = (x == 0) ? it->preds_p[y * preds_w - 1] : modes[it_var->i4_ - 1];
  const int top = (y == 0) ? it->preds_p[-preds_w + x] : modes[it_var->i4_ - 4];
  return VP8FixedCostsI4[top][left];
}

void Copy(__local const uint8_t* src, __local uint8_t* dst, int w, int h) {
  int y, z;
  for (y = 0; y < h; ++y) {
    for (z = 0; z < w; z++) {
      dst[z] = src[z];
    }
    src += BPS;
    dst += BPS;
  }
}

void Copy4x4(__local const uint8_t* src, __local uint8_t* dst) {
  Copy(src, dst, 4, 4);
}

void Copy16x8(__local const uint8_t* src, __local uint8_t* dst) {
  Copy(src, dst, 16, 8);
}

int PickBestIntra4(EncloopInputData* const input_data,
                   __local EncloopSegmentData* const segment_data,
                   VP8EncIteratorPointer* const it,
                   VP8EncIteratorVariable* const it_var,
                   VP8EncLoopPointer* const encloop,
                   EncloopSizeData* const size_data,
                   __local int* max_edge_l,
                   VP8ModeScore* rd,
                   __global uint8_t* y_top_p) {
  __local VP8Matrix* const dqm_y1 = &encloop->matrix_y1_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_y2 = &encloop->matrix_y2_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_uv = &encloop->matrix_uv_p[it->mb_info_p[3]];
  const int lambd = segment_data->lambda_i4[it->mb_info_p[3]];
  const int tlambda = segment_data->tlambda[it->mb_info_p[3]];
  __local const uint8_t* const src0 = it->yuv_in_p + Y_OFF_ENC;
  __local uint8_t* const best_blocks = it->yuv_out2_p + Y_OFF_ENC;
  int total_header_bits = 0;
  VP8ModeScore rd_best;
  uint8_t tmp_out;
  int i, j;
  // int copy_status = 0;

  if (input_data->max_i4_header_bits == 0) {
    return 0;
  }

  InitScore(&rd_best);
  rd_best.H = 211;  // '211' is the value of VP8BitCost(0, 145)
  SetRDScore(segment_data->lambda_mode[it->mb_info_p[3]], &rd_best);
  VP8IteratorStartI4(it, it_var, size_data, y_top_p);
  do {
    const int kNumBlocks = 1;
    VP8ModeScore rd_i4;
    int mode;
    int best_mode = -1;
    __local const uint8_t* const src = src0 + VP8Scan[it_var->i4_];
    const uint16_t* const mode_costs = GetCostModeI4(it, it_var, size_data, rd->modes_i4);
    __local uint8_t* best_block = best_blocks + VP8Scan[it_var->i4_];
    __local uint8_t* tmp_dst = it->yuv_p_p + I4TMP;    // scratch buffer.

    InitScore(&rd_i4);
    VP8MakeIntra4Preds(it);
    for (mode = 0; mode < NUM_BMODES; ++mode) {
      VP8ModeScore rd_tmp;
      int16_t tmp_levels[16];

      // Reconstruct
      rd_tmp.nz =
          ReconstructIntra4(segment_data, it, it_var, encloop, tmp_levels, src, tmp_dst, mode) << it_var->i4_;

      // Compute RD-score
      rd_tmp.D = SSE4x4(src, tmp_dst);
      rd_tmp.SD =
          tlambda ? MULT_8B(tlambda, Disto4x4(src, tmp_dst, kWeightY))
                  : 0;
      rd_tmp.H = mode_costs[mode];

      // Add flatness penalty
      if (mode > 0 && IsFlat(tmp_levels, kNumBlocks, FLATNESS_LIMIT_I4)) {
        rd_tmp.R = FLATNESS_PENALTY * kNumBlocks;
      } else {
        rd_tmp.R = 0;
      }

      // early-out check
      SetRDScore(lambd, &rd_tmp);
      if (best_mode >= 0 && rd_tmp.score >= rd_i4.score) {
        continue;
      }

      // finish computing score
      rd_tmp.R += VP8GetCostLuma4(it, it_var, encloop->coeffs_p, encloop->level_cost_p, tmp_levels);
      SetRDScore(lambd, &rd_tmp);

      if (best_mode < 0 || rd_tmp.score < rd_i4.score) {
        CopyScore(&rd_i4, &rd_tmp);
        best_mode = mode;
        for (i = 0; i < 4; i++) {
          for (j = 0; j < 4; j++) {                     // maybe wrong. by wu
            tmp_out = tmp_dst[j + i * BPS];
            tmp_dst[j + i * BPS] = best_block[j + i * BPS];
            best_block[j + i * BPS] = tmp_out;
          }
        }
        for (i = 0; i < 16; i++) {
          rd_best.y_ac_levels[it_var->i4_][i] = tmp_levels[i];
        }
      }
    }
    SetRDScore(segment_data->lambda_mode[it->mb_info_p[3]], &rd_i4);
    AddScore(&rd_best, &rd_i4);
    if (rd_best.score >= rd->score) {
      return 0;
    }
    total_header_bits += (int)rd_i4.H;   // <- equal to mode_costs[best_mode];
    if (total_header_bits > input_data->max_i4_header_bits) {
      return 0;
    }
    rd->modes_i4[it_var->i4_] = best_mode;
    it->top_nz_[it_var->i4_ & 3] = it->left_nz_[it_var->i4_ >> 2] = (rd_i4.nz ? 1 : 0);
  } while (VP8IteratorRotateI4(it, it_var, best_blocks));

  // finalize state
  CopyScore(rd, &rd_best);
  VP8SetIntra4Mode(it, size_data, rd->modes_i4);
  for (i = 0; i < YUV_SIZE_ENC; i++) {
    tmp_out = it->yuv_out_p[i];
    it->yuv_out_p[i] = it->yuv_out2_p[i];
    it->yuv_out2_p[i] = tmp_out;
  }
  for (i = 0; i < 16; i++) {
    for (j = 0; j < 16; j++) {
      rd->y_ac_levels[i][j] = rd_best.y_ac_levels[i][j];
    }
  }
  return 1;   // select intra4x4 over intra16x16
}

void PickBestUV(EncloopInputData* const input_data,
                __local EncloopSegmentData* const segment_data,
                VP8EncIteratorPointer* const it,
                VP8EncIteratorVariable* const it_var,
                VP8EncLoopPointer* const encloop,
                EncloopSizeData* const size_data,
                __local int* max_edge_l,
                VP8ModeScore* const rd) {
  const int kNumBlocks = 8;
  __local VP8Matrix* const dqm_y1 = &encloop->matrix_y1_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_y2 = &encloop->matrix_y2_p[it->mb_info_p[3]];
  __local VP8Matrix* const dqm_uv = &encloop->matrix_uv_p[it->mb_info_p[3]];
  const int lambd = segment_data->lambda_uv[it->mb_info_p[3]];
  __local const uint8_t* const src = it->yuv_in_p + U_OFF_ENC;
  __local uint8_t* tmp_dst = it->yuv_out2_p + U_OFF_ENC;  // scratch buffer
  __local uint8_t* dst0 = it->yuv_out_p + U_OFF_ENC;
  __local uint8_t* dst = dst0;
  VP8ModeScore rd_best;
  int mode;
  int i, j;
  int tmp;

  rd->mode_uv = -1;
  InitScore(&rd_best);
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    VP8ModeScore rd_uv;

    // Reconstruct
    rd_uv.nz = ReconstructUV(segment_data, it, it_var, encloop, &rd_uv, tmp_dst, mode);

    // Compute RD-score
    rd_uv.D  = SSE16x8(src, tmp_dst);
    rd_uv.SD = 0;    // not calling TDisto here: it tends to flatten areas.
    rd_uv.H  = VP8FixedCostsUV[mode];
    rd_uv.R  = VP8GetCostUV(it, encloop->coeffs_p, encloop->level_cost_p, &rd_uv);
    if (mode > 0 && IsFlat(rd_uv.uv_levels[0], kNumBlocks, FLATNESS_LIMIT_UV)) {
      rd_uv.R += FLATNESS_PENALTY * kNumBlocks;
    }

    SetRDScore(lambd, &rd_uv);
    if (mode == 0 || rd_uv.score < rd_best.score) {
      CopyScore(&rd_best, &rd_uv);
      rd->mode_uv = mode;
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 16; j++) {
          rd->uv_levels[i][j] = rd_uv.uv_levels[i][j];
        }
      }
      for (i = 0; i < 8; i++) {
        for (j = 0; j < 16; j++) {
          tmp = dst[j + i * BPS];
          dst[j + i * BPS] = tmp_dst[j + i * BPS];
          tmp_dst[j + i * BPS] = tmp;
        }
      }
    }
  }
  VP8SetIntraUVMode(it, rd->mode_uv);
  AddScore(rd, &rd_best);
}

void SimpleQuantize(__local EncloopSegmentData* const segment_data,
                    VP8EncIteratorPointer* const it,
                    VP8EncIteratorVariable* const it_var,
                    VP8EncLoopPointer* const encloop,
                    EncloopSizeData* const size_data,
                    VP8ModeScore* const rd,
                    __global uint8_t* y_top_p) {
  const int is_i16 = (it->mb_info_p[0] == 1);
  int nz = 0;

  if (is_i16) {
    nz = ReconstructIntra16(segment_data, it, it_var, encloop, rd, it->yuv_out_p + Y_OFF_ENC, it->preds_p[0]);
  } else {
    VP8IteratorStartI4(it, it_var, size_data, y_top_p);
    do {
      const int mode =
          it->preds_p[(it_var->i4_ & 3) + (it_var->i4_ >> 2) * size_data->preds_w];
      __local const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC + VP8Scan[it_var->i4_];
      __local uint8_t* const dst = it->yuv_out_p + Y_OFF_ENC + VP8Scan[it_var->i4_];
      VP8MakeIntra4Preds(it);
      nz |= ReconstructIntra4(segment_data, it, it_var, encloop, rd->y_ac_levels[it_var->i4_],
                              src, dst, mode) << it_var->i4_;
    } while (VP8IteratorRotateI4(it, it_var, it->yuv_out_p + Y_OFF_ENC));
  }

  nz |= ReconstructUV(segment_data, it, it_var, encloop, rd, it->yuv_out_p + U_OFF_ENC, it->mb_info_p[1]);
  rd->nz = nz;
}

void RefineUsingDistortion(__local EncloopSegmentData* const segment_data,
                           VP8EncIteratorPointer* const it,
                           VP8EncIteratorVariable* const it_var,
                           VP8EncLoopPointer* const encloop,
                           EncloopSizeData* const size_data,
                           int try_both_modes, int refine_uv_mode,
                           VP8ModeScore* const rd,
                           __global uint8_t* y_top_p) {
  score_t best_score = MAX_COST;
  score_t score_i4 = (score_t)I4_PENALTY;
  int16_t tmp_levels[16][16];
  uint8_t modes_i4[16];
  int nz = 0;
  int mode;
  int i, j;
  int is_i16 = try_both_modes || (it->mb_info_p[0] == 1);
  uint8_t tmp_out;

  if (is_i16) {   // First, evaluate Intra16 distortion
    int best_mode = -1;
    __local const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC;
    for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
      __local const uint8_t* const ref = it->yuv_p_p + VP8I16ModeOffsets[mode];
      const score_t score = SSE16x16(src, ref);
      if (score < best_score) {
        best_mode = mode;
        best_score = score;
      }
    }
    VP8SetIntra16Mode(it, size_data, best_mode);
    // we'll reconstruct later, if i16 mode actually gets selected
  }

  // Next, evaluate Intra4
  if (try_both_modes || !is_i16) {
    // We don't evaluate the rate here, but just account for it through a
    // constant penalty (i4 mode usually needs more bits compared to i16).
    is_i16 = 0;
    VP8IteratorStartI4(it, it_var, size_data, y_top_p);
    do {
      int best_i4_mode = -1;
      score_t best_i4_score = MAX_COST;
      __local const uint8_t* const src = it->yuv_in_p + Y_OFF_ENC + VP8Scan[it_var->i4_];

      VP8MakeIntra4Preds(it);
      for (mode = 0; mode < NUM_BMODES; ++mode) {
        __local const uint8_t* const ref = it->yuv_p_p + VP8I4ModeOffsets[mode];
        const score_t score = SSE4x4(src, ref);
        if (score < best_i4_score) {
          best_i4_mode = mode;
          best_i4_score = score;
        }
      }
      modes_i4[it_var->i4_] = best_i4_mode;
      score_i4 += best_i4_score;
      if (score_i4 >= best_score) {
        // Intra4 won't be better than Intra16. Bail out and pick Intra16.
        is_i16 = 1;
        break;
      } else {  // reconstruct partial block inside yuv_out2_ buffer
        __local uint8_t* const tmp_dst = it->yuv_out2_p + Y_OFF_ENC + VP8Scan[it_var->i4_];
        nz |= ReconstructIntra4(segment_data, it, it_var, encloop, tmp_levels[it_var->i4_],
                                src, tmp_dst, best_i4_mode) << it_var->i4_;
      }
    } while (VP8IteratorRotateI4(it, it_var, it->yuv_out2_p + Y_OFF_ENC));
  }

  // Final reconstruction, depending on which mode is selected.
  if (!is_i16) {
    VP8SetIntra4Mode(it, size_data, modes_i4);
    for (i = 0; i < 16; i++) {
      for (j = 0; j < 16; j++) {
        rd->y_ac_levels[i][j] = tmp_levels[i][j];
      }
    }
    for (i = 0; i < YUV_SIZE_ENC; i++) {
      tmp_out = it->yuv_out_p[i];
      it->yuv_out_p[i] = it->yuv_out2_p[i];
      it->yuv_out2_p[i] = tmp_out;
    }
    best_score = score_i4;
  } else {
    nz = ReconstructIntra16(segment_data, it, it_var, encloop, rd, it->yuv_out_p + Y_OFF_ENC, it->preds_p[0]);
  }

  // ... and UV!
  if (refine_uv_mode) {
    int best_mode = -1;
    score_t best_uv_score = MAX_COST;
    __local const uint8_t* const src = it->yuv_in_p + U_OFF_ENC;
    for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
      __local const uint8_t* const ref = it->yuv_p_p + VP8UVModeOffsets[mode];
      const score_t score = SSE16x8(src, ref);
      if (score < best_uv_score) {
        best_mode = mode;
        best_uv_score = score;
      }
    }
    VP8SetIntraUVMode(it, best_mode);
  }
  nz |= ReconstructUV(segment_data, it, it_var, encloop, rd, it->yuv_out_p + U_OFF_ENC, it->mb_info_p[1]);

  rd->nz = nz;
  rd->score = best_score;
}

int VP8Decimate(EncloopInputData* const input_data,
                __local EncloopSegmentData* const segment_data,
                VP8EncIteratorPointer* const it,
                VP8EncIteratorVariable* const it_var,
                VP8EncLoopPointer* const encloop,
                EncloopSizeData* const size_data,
                VP8ModeScore* const rd,
                __local int* max_edge_l,
                __global uint8_t* y_top_p,
                __global uint8_t* uv_top_p,
                int rd_opt) {
  int is_skipped;
  const int method = input_data->method;

  InitScore(rd);

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.
  VP8MakeLuma16Preds(it, y_top_p, it_var);
  VP8MakeChroma8Preds(it, uv_top_p, it_var);

  if (rd_opt > RD_OPT_NONE) {
    it_var->do_trellis_ = (rd_opt >= RD_OPT_TRELLIS_ALL);
    PickBestIntra16(input_data, segment_data, it, it_var, encloop, size_data, max_edge_l, rd);
    if (method >= 2) {
      PickBestIntra4(input_data, segment_data, it, it_var, encloop, size_data, max_edge_l, rd, y_top_p);
    }
    PickBestUV(input_data, segment_data, it, it_var, encloop, size_data, max_edge_l, rd);
    if (rd_opt == RD_OPT_TRELLIS) {   // finish off with trellis-optim now
      it_var->do_trellis_ = 1;
      SimpleQuantize(segment_data, it, it_var, encloop, size_data, rd, y_top_p);
    }
  } else {
    // At this point we have heuristically decided intra16 / intra4.
    // For method >= 2, pick the best intra4/intra16 based on SSE (~tad slower).
    // For method <= 1, we don't re-examine the decision but just go ahead with
    // quantization/reconstruction.
    RefineUsingDistortion(segment_data, it, it_var, encloop, size_data, (method >= 2), (method >= 1), rd, y_top_p);
  }
  is_skipped = (rd->nz == 0);
  VP8SetSkip(it, is_skipped);

  return is_skipped;
}

uint64_t VP8BitWriterPos(__global EncLoopOutputData* const output) {
  return (uint64_t)(output->pos + output->run) * 8 + 8 + output->nb_bits;
}

void Flush(__local uint8_t* const bw, __global EncLoopOutputData* const output) {
  const int s = 8 + output->nb_bits;
  const int32_t bits = output->value >> s;
  //assert(output->nb_bits >= 0);
  output->value -= bits << s;
  output->nb_bits -= 8;
  if ((bits & 0xff) != 0xff) {
    size_t pos = output->pos;
    //if (!BitWriterResize(bw, output, output->run + 1)) {                           // set buf size stable. by wu
    //  return;
    //}
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

int VP8PutBit(__local uint8_t* const bw, __global EncLoopOutputData* const output, int bit, int prob) {
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

int VP8PutBitUniform(__local uint8_t* const bw, __global EncLoopOutputData* const output, int bit) {
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

int PutCoeffs(__local uint8_t* const bw,
              __global EncLoopOutputData* const output, int ctx,
              const int16_t* coeffs, __local const uint8_t* const prob,
              const VP8Residual* res) {
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  __local const uint8_t* p = prob + n * BANDS_SIZE + ctx * CTX_SIZE;
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

void CodeResiduals(__global EncLoopOutputData* const output,
                   VP8EncIteratorPointer* const it,
                   VP8EncIteratorVariable* const it_var,
                   const VP8ModeScore* const rd,
                   __local uint8_t* bw_buf,
                   __local uint64_t* bit_count,
                   __local uint8_t* const coeffs_p) {
  int x, y, ch;
  VP8Residual res;
  uint64_t pos1, pos2, pos3;
  const int i16 = (it->mb_info_p[0] == 1);
  const int segment = it->mb_info_p[3];
  const int16_t* coeffs;
  __local const uint8_t* proba;          // the cast maybe wrong. by wu

  VP8IteratorNzToBytes(it);

  pos1 = VP8BitWriterPos(output);
  if (i16) {
    res.first = 0;
    res.coeff_type = 1;
    coeffs = rd->y_dc_levels;
    proba = coeffs_p + 1 * TYPES_SIZE;
    SetResidualCoeffs(rd->y_dc_levels, &res);
    it->top_nz_[8] = it->left_nz_[8] =
      PutCoeffs(bw_buf, output, it->top_nz_[8] + it->left_nz_[8], coeffs, proba, &res);
    res.first = 1;
    res.coeff_type = 0;
    proba = coeffs_p + 0 * TYPES_SIZE;
  } else {
    res.first = 0;
    res.coeff_type = 3;
    proba = coeffs_p + 3 * TYPES_SIZE;
  }

  // luma-AC
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      coeffs = rd->y_ac_levels[x + y * 4];
      SetResidualCoeffs(rd->y_ac_levels[x + y * 4], &res);
      it->top_nz_[x] = it->left_nz_[y] = PutCoeffs(bw_buf, output, ctx, coeffs, proba, &res);
    }
  }
  pos2 = VP8BitWriterPos(output);

  // U/V
  res.first = 0;
  res.coeff_type = 2;
  proba = coeffs_p + 2 * TYPES_SIZE;
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        coeffs = rd->uv_levels[ch * 2 + x + y * 2];
        SetResidualCoeffs(rd->uv_levels[ch * 2 + x + y * 2], &res);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] =
            PutCoeffs(bw_buf, output, ctx, coeffs, proba, &res);
      }
    }
  }
  pos3 = VP8BitWriterPos(output);
  it_var->luma_bits_ = pos2 - pos1;
  it_var->uv_bits_ = pos3 - pos2;
  bit_count[segment * 4 + i16] += it_var->luma_bits_;
  bit_count[segment * 4 + 2] += it_var->uv_bits_;
  VP8IteratorBytesToNz(it);
}

void ResetAfterSkip(VP8EncIteratorPointer* const it) {
  if (it->mb_info_p[0] == 1) {
    *it->nz_p = 0;  // reset all predictors
    it->left_nz_[8] = 0;
  } else {
    *it->nz_p &= (1 << 24);  // preserve the dc_nz bit
  }
}

#if SEGMENT_VISU
void SetBlock(__local uint8_t* p, int value, int size) {
  int y, z;
  for (y = 0; y < size; ++y) {
    for (z = 0; z < size; z++) {
      p[z] = value;
    }
    p += BPS;
  }
}
#endif

void StoreSSE(const VP8EncIteratorPointer* const it,
              __global uint64_t* sse_p,
              __global uint64_t* sse_count) {
  __local const uint8_t* const in = it->yuv_in_p;
  __local const uint8_t* const out = it->yuv_out_p;
  // Note: not totally accurate at boundary. And doesn't include in-loop filter.
  sse_p[0] += SSE16x16(in + Y_OFF_ENC, out + Y_OFF_ENC);
  sse_p[1] += SSE8x8(in + U_OFF_ENC, out + U_OFF_ENC);
  sse_p[2] += SSE8x8(in + V_OFF_ENC, out + V_OFF_ENC);
  *sse_count += 16 * 16;
}

void StoreSideInfo(const VP8EncIteratorPointer* const it,
                   const VP8EncIteratorVariable* const it_var,
                   const EncloopInputData* const input,
                   __global EncLoopOutputData* output,
                   __local const EncloopSegmentData* const segment_data,
                   VP8EncLoopPointer* encloop,
                   const EncloopSizeData* const size_data,
                   __global uint64_t* sse_p,
                   __global uint64_t* sse_count,
                   __global int* block_count) {
  __local uint8_t* mb = it->mb_info_p;
  if (input->stats_add != 0) {
    StoreSSE(it, sse_p, sse_count);
    block_count[0] += (mb[0] == 0);
    block_count[1] += (mb[0] == 1);
    block_count[2] += (mb[2] != 0);
  }

  if (input->extra_info_type > 0) {
    __local uint8_t* const info = &encloop->extra_info_p[it_var->x_ + it_var->y_ * size_data->mb_w];
    switch (input->extra_info_type) {
      case 1: *info = mb[0]; break;
      case 2: *info = mb[4]; break;
      case 3: *info = segment_data->quant[mb[3]]; break;
      case 4: *info = (mb[0] == 1) ? it->preds_p[0] : 0xff; break;
      case 5: *info = mb[1]; break;
      case 6: {
        const int b = (int)((it_var->luma_bits_ + it_var->uv_bits_ + 7) >> 3);
        *info = (b > 255) ? 255 : b; break;
      }
      case 7: *info = mb[4]; break;
      default: *info = 0; break;
    }
  }
#if SEGMENT_VISU  // visualize segments and prediction modes
  SetBlock(it->yuv_out_p + Y_OFF_ENC, mb[3] * 64, 16);
  SetBlock(it->yuv_out_p + U_OFF_ENC, it->preds_p[0] * 64, 8);
  SetBlock(it->yuv_out_p + V_OFF_ENC, mb[1] * 64, 8);
#endif

}

enum { KERNEL = 3 };
static const double kMinValue = 1.e-10;  // minimal threshold

void VP8SSIMAccumulate(__local const uint8_t* src1, int stride1,
                       __local const uint8_t* src2, int stride2,
                       int xo, int yo, int W, int H,
                       DistoStats* const stats) {
  const int ymin = (yo - KERNEL < 0) ? 0 : yo - KERNEL;
  const int ymax = (yo + KERNEL > H - 1) ? H - 1 : yo + KERNEL;
  const int xmin = (xo - KERNEL < 0) ? 0 : xo - KERNEL;
  const int xmax = (xo + KERNEL > W - 1) ? W - 1 : xo + KERNEL;
  int x, y;
  src1 += ymin * stride1;
  src2 += ymin * stride2;
  for (y = ymin; y <= ymax; ++y, src1 += stride1, src2 += stride2) {
    for (x = xmin; x <= xmax; ++x) {
      const int s1 = src1[x];
      const int s2 = src2[x];
      stats->w   += 1;
      stats->xm  += s1;
      stats->ym  += s2;
      stats->xxm += s1 * s1;
      stats->xym += s1 * s2;
      stats->yym += s2 * s2;
    }
  }
}

double VP8SSIMGet(const DistoStats* const stats) {
  const double xmxm = stats->xm * stats->xm;
  const double ymym = stats->ym * stats->ym;
  const double xmym = stats->xm * stats->ym;
  const double w2 = stats->w * stats->w;
  double sxx = stats->xxm * stats->w - xmxm;
  double syy = stats->yym * stats->w - ymym;
  double sxy = stats->xym * stats->w - xmym;
  double C1, C2;
  double fnum;
  double fden;
  // small errors are possible, due to rounding. Clamp to zero.
  if (sxx < 0.) sxx = 0.;
  if (syy < 0.) syy = 0.;
  C1 = 6.5025 * w2;
  C2 = 58.5225 * w2;
  fnum = (2 * xmym + C1) * (2 * sxy + C2);
  fden = (xmxm + ymym + C1) * (sxx + syy + C2);
  return (fden != 0.) ? fnum / fden : kMinValue;
}

double GetMBSSIM(__local const uint8_t* yuv1, __local const uint8_t* yuv2) {
  int x, y;
  DistoStats s = { .0, .0, .0, .0, .0, .0 };

  // compute SSIM in a 10 x 10 window
  for (x = 3; x < 13; x++) {
    for (y = 3; y < 13; y++) {
      VP8SSIMAccumulate(yuv1 + Y_OFF_ENC, BPS, yuv2 + Y_OFF_ENC, BPS,
                        x, y, 16, 16, &s);
    }
  }
  for (x = 1; x < 7; x++) {
    for (y = 1; y < 7; y++) {
      VP8SSIMAccumulate(yuv1 + U_OFF_ENC, BPS, yuv2 + U_OFF_ENC, BPS,
                        x, y, 8, 8, &s);
      VP8SSIMAccumulate(yuv1 + V_OFF_ENC, BPS, yuv2 + V_OFF_ENC, BPS,
                        x, y, 8, 8, &s);
    }
  }
  return VP8SSIMGet(&s);
}

int hev(__local const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (VP8kabs0[p1 - p0] > thresh) || (VP8kabs0[q1 - q0] > thresh);
}

int needs_filter(__local const uint8_t* p, int step, int t) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) <= t);
}

int needs_filter2(__local const uint8_t* p,
                  int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) > t) return 0;
  return VP8kabs0[p3 - p2] <= it && VP8kabs0[p2 - p1] <= it &&
         VP8kabs0[p1 - p0] <= it && VP8kabs0[q3 - q2] <= it &&
         VP8kabs0[q2 - q1] <= it && VP8kabs0[q1 - q0] <= it;
}

void do_filter2(__local uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0) + VP8ksclip1[p1 - q1];  // in [-893,892]
  const int a1 = VP8ksclip2[(a + 4) >> 3];            // in [-16,15]
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  p[-step] = VP8kclip1[p0 + a2];
  p[    0] = VP8kclip1[q0 - a1];
}

void do_filter4(__local uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  const int a3 = (a1 + 1) >> 1;
  p[-2*step] = VP8kclip1[p1 + a3];
  p[-  step] = VP8kclip1[p0 + a2];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a3];
}

void FilterLoop24(__local uint8_t* p,
                  int hstride, int vstride, int size,
                  int thresh, int ithresh, int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (needs_filter2(p, hstride, thresh2, ithresh)) {
      if (hev(p, hstride, hev_thresh)) {
        do_filter2(p, hstride);
      } else {
        do_filter4(p, hstride);
      }
    }
    p += vstride;
  }
}

void VFilter16i(__local uint8_t* p, int stride,
                int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    FilterLoop24(p, stride, 1, 16, thresh, ithresh, hev_thresh);
  }
}

void HFilter16i(__local uint8_t* p, int stride,
                int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    FilterLoop24(p, 1, stride, 16, thresh, ithresh, hev_thresh);
  }
}

void VFilter8i(__local uint8_t* u, __local uint8_t* v, int stride,
               int thresh, int ithresh, int hev_thresh) {
  FilterLoop24(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop24(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}

void HFilter8i(__local uint8_t* u, __local uint8_t* v, int stride,
               int thresh, int ithresh, int hev_thresh) {
  FilterLoop24(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop24(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}

void SimpleVFilter16(__local uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (needs_filter(p + i, stride, thresh2)) {
      do_filter2(p + i, stride);
    }
  }
}

void SimpleHFilter16(__local uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (needs_filter(p + i * stride, 1, thresh2)) {
      do_filter2(p + i * stride, 1);
    }
  }
}

void SimpleVFilter16i(__local uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16(p, stride, thresh);
  }
}

void SimpleHFilter16i(__local uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16(p, stride, thresh);
  }
}

int GetILevel(int sharpness, int level) {
  if (sharpness > 0) {
    if (sharpness > 4) {
      level >>= 2;
    } else {
      level >>= 1;
    }
    if (level > 9 - sharpness) {
      level = 9 - sharpness;
    }
  }
  if (level < 1) level = 1;
  return level;
}

void DoFilter(const VP8EncIteratorPointer* const it,
              const EncloopInputData* const input,
              int level) {
  const int ilevel = GetILevel(input->filter_sharpness, level);
  const int limit = 2 * level + ilevel;
  int i;

  __local uint8_t* const y_dst = it->yuv_out2_p + Y_OFF_ENC;
  __local uint8_t* const u_dst = it->yuv_out2_p + U_OFF_ENC;
  __local uint8_t* const v_dst = it->yuv_out2_p + V_OFF_ENC;

  // copy current block to yuv_out2_
  for (i = 0; i < YUV_SIZE_ENC; i++) {
    y_dst[i] = it->yuv_out_p[i];
  }

  if (input->simple == 1) {   // simple
    SimpleHFilter16i(y_dst, BPS, limit);
    SimpleVFilter16i(y_dst, BPS, limit);
  } else {    // complex
    const int hev_thresh = (level >= 40) ? 2 : (level >= 15) ? 1 : 0;
    HFilter16i(y_dst, BPS, limit, ilevel, hev_thresh);
    HFilter8i(u_dst, v_dst, BPS, limit, ilevel, hev_thresh);
    VFilter16i(y_dst, BPS, limit, ilevel, hev_thresh);
    VFilter8i(u_dst, v_dst, BPS, limit, ilevel, hev_thresh);
  }
}

void VP8StoreFilterStats(const VP8EncIteratorPointer* const it,
                         const VP8EncIteratorVariable* const it_var,
                         const EncloopInputData* const input,
                         __local const EncloopSegmentData* const segment_data,
                         __local double* lf_stats_p) {
  int d;
  const int s = it->mb_info_p[3];
  const int level0 = segment_data->fstrength[s];

  // explore +/-quant range of values around level0
  const int delta_min = -segment_data->quant[s];
  const int delta_max = segment_data->quant[s];
  const int step_size = (delta_max - delta_min >= 4) ? 4 : 1;

  if (input->lf_stats_status == 0) return;

  // NOTE: Currently we are applying filter only across the sublock edges
  // There are two reasons for that.
  // 1. Applying filter on macro block edges will change the pixels in
  // the left and top macro blocks. That will be hard to restore
  // 2. Macro Blocks on the bottom and right are not yet compressed. So we
  // cannot apply filter on the right and bottom macro block edges.
  if (it->mb_info_p[0] == 1 && it->mb_info_p[2]) return;

  // Always try filter level  zero
  lf_stats_p[s * NUM_MB_SEGMENTS + 0] += GetMBSSIM(it->yuv_in_p, it->yuv_out_p);              // need to check. by wu

  for (d = delta_min; d <= delta_max; d += step_size) {
    const int level = level0 + d;
    if (level <= 0 || level >= MAX_LF_LEVELS) {
      continue;
    }
    DoFilter(it, input, level);
    lf_stats_p[s * NUM_MB_SEGMENTS + level] += GetMBSSIM(it->yuv_in_p, it->yuv_out2_p);
  }
}

void ExportBlock(__local const uint8_t* src,
                 __local uint8_t* dst, int dst_stride,
                 int w, int h) {
  while (h-- > 0) {
    for (int i = 0; i < w; i++) {
      dst[i] = src[i];
    }
    dst += dst_stride;
    src += BPS;
  }
}

void VP8IteratorExport(const VP8EncIteratorPointer* const it,
                       const VP8EncIteratorVariable* const it_var,
                       const EncloopInputData* const input,
                       __local const uint8_t* const y_l,
                       __local const uint8_t* const u_l,
                       __local const uint8_t* const v_l) {
  if (input->show_compressed) {
    const int x = it_var->x_, y = it_var->y_;
    __local const uint8_t* const ysrc = it->yuv_out_p + Y_OFF_ENC;
    __local const uint8_t* const usrc = it->yuv_out_p + U_OFF_ENC;
    __local const uint8_t* const vsrc = it->yuv_out_p + V_OFF_ENC;
    __local uint8_t* const ydst = y_l + x * 16;
    __local uint8_t* const udst = u_l + x * 8;
    __local uint8_t* const vdst = v_l + x * 8;

    // Luma plane
    ExportBlock(ysrc, ydst, it_var->y_stride_, 16, 16);

    {   // U/V planes
      ExportBlock(usrc, udst, it_var->uv_stride_, 8, 8);
      ExportBlock(vsrc, vdst, it_var->uv_stride_, 8, 8);
    }
  }
}

void VP8IteratorSaveBoundary(const VP8EncIteratorPointer* const it,
                             const VP8EncIteratorVariable* const it_var,
                             const EncloopSizeData* const size_data,
                             __global uint8_t* y_top_p,
                             __global uint8_t* uv_top_p) {
  int i;
  const int x = it_var->x_, y = it_var->y_;
  __local const uint8_t* const ysrc = it->yuv_out_p + Y_OFF_ENC;
  __local const uint8_t* const uvsrc = it->yuv_out_p + U_OFF_ENC;
  if (x < size_data->mb_w - 1) {   // left
    int i;
    for (i = 0; i < 16; ++i) {
      it->y_left_p[i] = ysrc[15 + i * BPS];
    }
    for (i = 0; i < 8; ++i) {
      it->u_left_p[i] = uvsrc[7 + i * BPS];
      it->v_left_p[i] = uvsrc[15 + i * BPS];
    }
    // top-left (before 'top'!)
    it->y_left_p[-1] = y_top_p[15];
    it->u_left_p[-1] = uv_top_p[0 + 7];
    it->v_left_p[-1] = uv_top_p[8 + 7];
  }
  if (y < size_data->mb_h - 1) {  // top
    for (i = 0; i < 16; i++) {
      y_top_p[i] = ysrc[15 * BPS + i];
    }
    for (i = 0; i < 16; i++) {
      uv_top_p[i] = uvsrc[7 * BPS + i];
    }
  }
}

__kernel __attribute__ ((reqd_work_group_size(1, 1, 1)))
void encloop (EncloopInputData input_data,
              int rd_opt,
              __global uint8_t* y,                               // input/output
              __global uint8_t* u,                               // input/output
              __global uint8_t* v,                               // input/output
              __global uint8_t* mb_info,                         // input/output
              __global uint8_t* preds,                           // input/output
              __global uint32_t* nz,                             // output
              __global uint8_t* y_top,                           // output          // maybe no need to read to host, to be confirem.
              __global uint8_t* uv_top,                          // output
              __global double* lf_stats,                         // input/output ->output
              __global const uint16_t* const quant_matrix,       // input
              __global const uint8_t* const coeffs,              // input
              __global const uint32_t* const stats,              // input
              __global const uint16_t* const level_cost,         // input
              __global int* segment_data,                        // input
              __global uint8_t* bw_buf,                          // output
              __global uint64_t* sse,                            // output
              __global int* block_count,                         // output
              __global uint8_t* extra_info,                      // output
              __global int* max_edge,                            // output
              __global uint64_t* bit_count,                      // output
              __global uint64_t* sse_count,                      // output
              __global int32_t* output_data) {
  event_t event_read[13];
  event_t event_write[10];

  int i, j;
  int ok;

  int y_offset, uv_offset, mb_offset, preds_offset;
  int y_length, uv_length, mb_length, preds_length;

  EncloopSizeData size_data;
  VP8EncIteratorPointer it_pointer;
  VP8EncIteratorVariable it_variable;
  VP8EncLoopPointer loop_pointer;
  EncLoopOutputData output;

  const int width  = input_data.width;
  const int height = input_data.height;
  const int mb_w = (width + 15) >> 4;
  const int mb_h = (height + 15) >> 4;
  const int preds_w = 4 * mb_w + 1;
  const int preds_h = 4 * mb_h + 1;
  const int y_stride = width;
  const int uv_stride = (width + 1) >> 1;
  const int uv_width = (width + 1) >> 1;
  const int uv_height = (height + 1) >> 1;

  const int uv_size = uv_width * uv_height;
  const int lf_stats_size = NUM_MB_SEGMENTS * MAX_LF_LEVELS;
  const int coeffs_size = NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS;
  const int stats_size = NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS;
  const int level_cost_size = NUM_TYPES * NUM_BANDS * NUM_CTX * (MAX_VARIABLE_LEVEL + 1);

  size_data.mb_w = (width + 15) >> 4;
  size_data.mb_h = (height + 15) >> 4;
  size_data.preds_w = 4 * size_data.mb_w + 1;
  size_data.preds_h = 4 * size_data.mb_w + 1;
  size_data.uv_width  = uv_width;
  size_data.uv_height = uv_height;

  __local uint8_t y_l[LARGEST_Y_STRIDE * 16];
  __local uint8_t u_l[LARGEST_UV_STRIDE * 8];
  __local uint8_t v_l[LARGEST_UV_STRIDE * 8];

  __local uint8_t mb_info_l[5 * LARGEST_MB_W];              // type/uv_mode/skip/segment/alpha
  __local uint8_t preds_l[4 * LARGEST_PREDS_W];
  __local uint32_t nz_l[(LARGEST_MB_W + 1)];

  __local double lf_stats_l[NUM_MB_SEGMENTS * MAX_LF_LEVELS];
  __local uint16_t matrix_y1_l[NUM_MB_SEGMENTS * MATRIX_SIZE];
  __local uint16_t matrix_y2_l[NUM_MB_SEGMENTS * MATRIX_SIZE];
  __local uint16_t matrix_uv_l[NUM_MB_SEGMENTS * MATRIX_SIZE];
  __local uint8_t coeffs_l[NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS];               // 1056  bytes
  __local uint32_t stats_l[NUM_TYPES * NUM_BANDS * NUM_CTX * NUM_PROBAS];                // 4224  bytes
  __local uint16_t level_cost_l[NUM_TYPES * NUM_BANDS * NUM_CTX * (MAX_VARIABLE_LEVEL + 1)];   // 13056 bytes
  __local int segment_l[NUM_MB_SEGMENTS * 11];

  __local uint8_t bw_buf_l[408000];                       // The size is the largest size.(according print log)

  __local uint8_t extra_info_l[LARGEST_MB_W * LARGEST_MB_H];
  __local int max_edge_l[NUM_MB_SEGMENTS];
  __local int bit_count_l[4 * 3];

  // memory for storing y/u/v_left_
  __local uint8_t yuv_left_mem_[17 + 16 + 16 + 8 + WEBP_ALIGN_CST];
  // memory for yuv_*
  __local uint8_t yuv_mem_[3 * YUV_SIZE_ENC + PRED_SIZE_ENC + WEBP_ALIGN_CST];

  y_offset = get_group_id(0) * y_stride * 16;
  y_length = y_stride * 16;

  uv_offset = get_group_id(0) * uv_stride * 8;
  uv_length = uv_stride * 8;

  event_read[0] = async_work_group_copy(y_l, y + y_offset, y_length, 0);
  event_read[1] = async_work_group_copy(u_l, u + uv_offset, uv_length, 0);
  event_read[2] = async_work_group_copy(v_l, v + uv_offset, uv_length, 0);

  mb_offset = get_group_id(0) * 5 * mb_w;
  mb_length = 5 * mb_w;

  event_read[3] = async_work_group_copy(mb_info_l, mb_info + mb_offset, mb_length, 0);

  preds_offset = get_group_id(0) * 4 * preds_w;
  preds_length = 4 * preds_w;

  event_read[4] = async_work_group_copy(preds_l, preds + preds_offset, preds_length, 0);

  event_read[5] = async_work_group_copy(matrix_y1_l, quant_matrix, NUM_MB_SEGMENTS * MATRIX_SIZE, 0);
  event_read[6] = async_work_group_copy(matrix_y2_l, quant_matrix + NUM_MB_SEGMENTS * MATRIX_SIZE, NUM_MB_SEGMENTS * MATRIX_SIZE, 0);
  event_read[7] = async_work_group_copy(matrix_uv_l, quant_matrix + 2 * NUM_MB_SEGMENTS * MATRIX_SIZE, NUM_MB_SEGMENTS * MATRIX_SIZE, 0);

  event_read[8] = async_work_group_copy(coeffs_l, coeffs, coeffs_size, 0);
  event_read[9] = async_work_group_copy(stats_l, stats, stats_size, 0);
  event_read[10] = async_work_group_copy(level_cost_l, level_cost, level_cost_size, 0);
  event_read[11] = async_work_group_copy(segment_l, segment_data, NUM_MB_SEGMENTS * 11, 0);
  event_read[12] = async_work_group_copy(nz_l, nz, (mb_w + 1), 0);

  for (i = 0; i < NUM_MB_SEGMENTS; i++) {
    max_edge_l[i] = max_edge[i];
  }

  wait_group_events(13, event_read);
  barrier(CLK_LOCAL_MEM_FENCE);

  loop_pointer.matrix_y1_p = (__local VP8Matrix*)matrix_y1_l;
  loop_pointer.matrix_y2_p = (__local VP8Matrix*)matrix_y2_l;
  loop_pointer.matrix_uv_p = (__local VP8Matrix*)matrix_uv_l;
  loop_pointer.coeffs_p = coeffs_l;
  loop_pointer.stats_p = stats_l;
  loop_pointer.level_cost_p = level_cost_l;
  loop_pointer.bw_buf_p = bw_buf_l;
  loop_pointer.extra_info_p = extra_info_l;
  loop_pointer.bit_count_p = bit_count_l;

  __local EncloopSegmentData* segment_p = (__local EncloopSegmentData*)segment_l;

  it_pointer.yuv_in_p = yuv_mem_;
  it_pointer.yuv_out_p = yuv_mem_ + YUV_SIZE_ENC;
  it_pointer.yuv_out2_p = yuv_mem_ + 2 * YUV_SIZE_ENC;
  it_pointer.yuv_p_p = yuv_mem_ + 3 * YUV_SIZE_ENC;
  it_pointer.mb_info_p = mb_info_l;
  it_pointer.preds_p = preds_l;
  it_pointer.nz_p = nz_l + 1;
  it_pointer.y_left_p = yuv_left_mem_ + 1;
  it_pointer.u_left_p = yuv_left_mem_ + 1 + 16 + 16;
  it_pointer.v_left_p = yuv_left_mem_ + 1 + 16 + 16 + 16;

  __global uint8_t* y_top_p = y_top;
  __global uint8_t* uv_top_p = uv_top;

  for (i = 0; i < 12; i++) {
    bit_count_l[i] = 0;
  }
  it_variable.do_trellis_ = 0;
  it_variable.y_ = get_group_id(0);
  it_variable.y_stride_  = y_stride;
  it_variable.uv_stride_ = uv_stride;

  InitLeft(&it_pointer, &it_variable);

  for (int mb_index = 0; mb_index < mb_w; mb_index++) {
    it_variable.x_ = mb_index;
    VP8ModeScore info;
    const int dont_use_skip = input_data.use_skip_proba;

    VP8IteratorImport(&it_pointer, &it_variable, y_l, u_l, v_l);
    // Warning! order is important: first call VP8Decimate() and
    // *then* decide how to code the skip decision if there's one.
    int skip_status = !VP8Decimate(&input_data, segment_p, &it_pointer, &it_variable,
                      &loop_pointer, &size_data, &info, max_edge_l, y_top_p, uv_top_p, rd_opt) || dont_use_skip;
    if (skip_status) {
      CodeResiduals(output_data, &it_pointer, &it_variable, &info,
                    loop_pointer.bw_buf_p, loop_pointer.bit_count_p,
                    loop_pointer.coeffs_p);
    } else {   // reset predictors after a skip
      ResetAfterSkip(&it_pointer);
    }
    StoreSideInfo(&it_pointer, &it_variable, &input_data, output_data,
                  segment_p, &loop_pointer, &size_data, sse, sse_count, block_count);
    // VP8StoreFilterStats(&it_pointer, &it_variable, &input_data, segment_p, lf_stats_l);
    // VP8IteratorExport(&it_pointer, &it_variable, &input_data, y_l, u_l, v_l);
    VP8IteratorSaveBoundary(&it_pointer, &it_variable, &size_data, y_top_p, uv_top_p);

    it_pointer.preds_p += 4;
    it_pointer.mb_info_p += 5;
    it_pointer.nz_p += 1;
    y_top_p += 16;
    uv_top_p += 16;
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  event_write[0] = async_work_group_copy(y + y_offset, y_l, y_length, 0);
  event_write[1] = async_work_group_copy(u + uv_offset, u_l, uv_length, 0);
  event_write[2] = async_work_group_copy(v + uv_offset, v_l, uv_length, 0);
  event_write[3] = async_work_group_copy(mb_info + mb_offset, mb_info_l, mb_length, 0);
  event_write[4] = async_work_group_copy(preds + preds_offset, preds_l, preds_length, 0);
  event_write[5] = async_work_group_copy(nz, nz_l, (mb_w + 1), 0);
  event_write[8] = async_work_group_copy(bw_buf, bw_buf_l, 408000, 0);
  event_write[9] = async_work_group_copy(extra_info, extra_info_l, mb_w * mb_h, 0);
  wait_group_events(10, event_write);

  for (i = 0; i < lf_stats_size; i++) {
    lf_stats[i] = lf_stats_l[i];
  }
  for (i = 0; i < NUM_MB_SEGMENTS; i++) {
    max_edge[i] = max_edge_l[i];
  }
  for (i = 0; i < 12; i++) {
    bit_count[i] = bit_count_l[i];
  }
}
