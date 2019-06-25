#pragma once
#include<stdint.h>
#define RC_SYMBOLS_MAX 58
//typedef uint16_t probability;
#define RC_SHIFT_BITS 8
#define RC_TOP_BITS 24
#define RC_TOP_VALUE (1U << RC_TOP_BITS)
#define RC_BIT_MODEL_TOTAL_BITS 11
#define RC_BIT_MODEL_TOTAL (1U << RC_BIT_MODEL_TOTAL_BITS)
#define RC_MOVE_BITS 5

#define STATES 12
#define LIT_STATES 7
#define LITERAL_CODER_SIZE 0x300            //786
#define LZMA_LCLP_MAX    4
#define LZMA_PB_MAX      4
#define LITERAL_CODERS_MAX (1 << LZMA_LCLP_MAX)   //16
#define POS_STATES_MAX (1 << LZMA_PB_MAX)        //16
#define START_POS_MODEL_INDEX 4

#define bit_reset(prob) \
        prob = RC_BIT_MODEL_TOTAL >> 1

#define bittree_reset(probs, bit_levels) \
        for (uint32_t bt_i = 0; bt_i < (1 << (bit_levels)); ++bt_i) \
                bit_reset((probs)[bt_i])

//#define UINT32_MAX -1 //(1<<32)-1
//#define RC_BIT_0 0
//#define RC_BIT_1 1
//#define RC_DIRECT_0 2
//#define RC_DIRECT_1 3
//#define RC_FLUSH 4

enum sym{
    RC_BIT_0,
    RC_BIT_1,
    RC_DIRECT_0,
    RC_DIRECT_1,
    RC_FLUSH,
};

enum lzma_lzma_state{
        STATE_LIT_LIT,
        STATE_MATCH_LIT_LIT,
        STATE_REP_LIT_LIT,
        STATE_SHORTREP_LIT_LIT,
        STATE_MATCH_LIT,
        STATE_REP_LIT,
        STATE_SHORTREP_LIT,
        STATE_LIT_MATCH,
        STATE_LIT_LONGREP,
        STATE_LIT_SHORTREP,
        STATE_NONLIT_MATCH,
        STATE_NONLIT_REP,
} ;


#define update_literal(state) \
        state = (lzma_lzma_state)((state) <= STATE_SHORTREP_LIT_LIT \
                        ? STATE_LIT_LIT \
                        : ((state) <= STATE_LIT_SHORTREP \
                                ? (state) - 3 \
                                : (state) - 6))
#define update_match(state) \
        state = (lzma_lzma_state)((state) < LIT_STATES ? STATE_LIT_MATCH : STATE_NONLIT_MATCH)

#define update_long_rep(state) \
        state = (lzma_lzma_state)((state) < LIT_STATES ? STATE_LIT_LONGREP : STATE_NONLIT_REP)

/// Indicate that the latest state was a short match.
#define update_short_rep(state) \
        state = (lzma_lzma_state)((state) < LIT_STATES ? STATE_LIT_SHORTREP : STATE_NONLIT_REP)

/// Test if the previous state was a literal.
#define is_literal_state(state) \
        ((state) < LIT_STATES)


#define literal_subcoder(probs, lc, lp_mask, pos, prev_byte) \
        ((probs)[(((pos) & lp_mask) << lc) + ((prev_byte) >> (8 - lc))])

#define POS_SLOT_BITS 6
#define POS_SLOTS (1 << POS_SLOT_BITS)

#define LEN_TO_POS_STATES 4
#define MATCH_LEN_MIN 2
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)
#define MATCH_LEN_MAX (MATCH_LEN_MIN + LEN_SYMBOLS - 1)

#define END_POS_MODEL_INDEX 14
#define FULL_DISTANCES_BITS (END_POS_MODEL_INDEX / 2)
#define FULL_DISTANCES (1 << FULL_DISTANCES_BITS)

#define ALIGN_BITS 4
#define ALIGN_TABLE_SIZE (1 << ALIGN_BITS)
#define ALIGN_MASK (ALIGN_TABLE_SIZE - 1)

#define get_len_to_pos_state(len) \
        ((len) < LEN_TO_POS_STATES + MATCH_LEN_MIN \
                ? (len) - MATCH_LEN_MIN \
                : LEN_TO_POS_STATES - 1)

#define START_POS_MODEL_INDEX 4
#define END_POS_MODEL_INDEX 14

#define REP_DISTANCES 4

