/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef DIXIE_H
#define DIXIE_H
#include "./vpx_codec_internal.hh"
#include "bool_decoder.hh"

struct vp8_frame_hdr
{
    unsigned int is_keyframe;      /* Frame is a keyframe */
    unsigned int is_experimental;  /* Frame is a keyframe */
    unsigned int version;          /* Bitstream version */
    unsigned int is_shown;         /* Frame is to be displayed. */
    unsigned int part0_sz;         /* Partition 0 length, in bytes */

    struct vp8_kf_hdr
    {
        unsigned int w;        /* Width */
        unsigned int h;        /* Height */
        unsigned int scale_w;  /* Scaling factor, Width */
        unsigned int scale_h;  /* Scaling factor, Height */
    } kf;

    unsigned int frame_size_updated; /* Flag to indicate a resolution
                                      * update.
                                      */
};


enum
{
    MB_FEATURE_TREE_PROBS = 3,
    MAX_MB_SEGMENTS = 4
};


struct vp8_segment_hdr
{
    unsigned int         enabled;
    unsigned int         update_data;
    unsigned int         update_map;
    unsigned int         abs;    /* 0=deltas, 1=absolute values */
    unsigned int         tree_probs[MB_FEATURE_TREE_PROBS];
    int                  lf_level[MAX_MB_SEGMENTS];
    int                  quant_idx[MAX_MB_SEGMENTS];
};


enum
{
    BLOCK_CONTEXTS = 4
};


struct vp8_loopfilter_hdr
{
    unsigned int         use_simple;
    unsigned int         level;
    unsigned int         sharpness;
    unsigned int         delta_enabled;
    int                  ref_delta[BLOCK_CONTEXTS];
    int                  mode_delta[BLOCK_CONTEXTS];
};


enum
{
    MAX_PARTITIONS = 8
};

struct vp8_token_hdr
{
    unsigned int        partitions;
    unsigned int        partition_sz[MAX_PARTITIONS];
};


struct vp8_quant_hdr
{
    unsigned int       q_index;
    int                delta_update;
    int                y1_dc_delta_q;
    int                y2_dc_delta_q;
    int                y2_ac_delta_q;
    int                uv_dc_delta_q;
    int                uv_ac_delta_q;
};


struct vp8_reference_hdr
{
    unsigned int refresh_last;
    unsigned int refresh_gf;
    unsigned int refresh_arf;
    unsigned int copy_gf;
    unsigned int copy_arf;
    unsigned int sign_bias[4];
    unsigned int refresh_entropy;
};


enum
{
    BLOCK_TYPES        = 4,
    PREV_COEF_CONTEXTS = 3,
    COEF_BANDS         = 8,
    ENTROPY_NODES      = 11,
};
typedef unsigned char coeff_probs_table_t[BLOCK_TYPES][COEF_BANDS]
[PREV_COEF_CONTEXTS]
[ENTROPY_NODES];


enum
{
    MV_PROB_CNT = 2 + 8 - 1 + 10 /* from entropymv.h */
};
typedef unsigned char mv_component_probs_t[MV_PROB_CNT];


struct vp8_entropy_hdr
{
    coeff_probs_table_t   coeff_probs;
    mv_component_probs_t  mv_probs[2];
    unsigned int          coeff_skip_enabled;
    unsigned char         coeff_skip_prob;
    unsigned char         y_mode_probs[4];
    unsigned char         uv_mode_probs[3];
    unsigned char         prob_inter;
    unsigned char         prob_last;
    unsigned char         prob_gf;
};


enum reference_frame
{
    CURRENT_FRAME,
    LAST_FRAME,
    GOLDEN_FRAME,
    ALTREF_FRAME,
    NUM_REF_FRAMES
};


enum prediction_mode
{
    /* 16x16 intra modes */
    DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED,

    /* 16x16 inter modes */
    NEARESTMV, NEARMV, ZEROMV, NEWMV, SPLITMV,

    MB_MODE_COUNT,

    /* 4x4 intra modes */
    B_DC_PRED = 0, B_TM_PRED, B_VE_PRED, B_HE_PRED, B_LD_PRED,
    B_RD_PRED, B_VR_PRED, B_VL_PRED, B_HD_PRED, B_HU_PRED,

    /* 4x4 inter modes */
    LEFT4X4, ABOVE4X4, ZERO4X4, NEW4X4,

    B_MODE_COUNT
};


enum splitmv_partitioning
{
    SPLITMV_16X8,
    SPLITMV_8X16,
    SPLITMV_8X8,
    SPLITMV_4X4
};


typedef short filter_t[6];


typedef union mv
{
    struct
    {
        int16_t x, y;
    }  d;
    uint32_t               raw;
} mv_t;


struct mb_base_info
{
    unsigned char y_mode     : 4;
    unsigned char uv_mode    : 4;
    unsigned char segment_id : 2;
    unsigned char ref_frame  : 2;
    unsigned char skip_coeff : 1;
    unsigned char need_mc_border : 1;
    enum splitmv_partitioning  partitioning : 2;
    union mv      mv;
    unsigned int  eob_mask;
};


struct mb_info
{
    struct mb_base_info base;
    union
    {
        union mv              mvs[16];
        enum prediction_mode  modes[16];
    } split;
};


/* A "token entropy context" has 4 Y values, 2 U, 2 V, and 1 Y2 */
typedef int token_entropy_ctx_t[4 + 2 + 2 + 1];

struct token_decoder
{
    struct bool_decoder  boolean_dec;
    token_entropy_ctx_t  left_token_entropy_ctx;
    short               *coeffs;
};

enum token_block_type
{
    TOKEN_BLOCK_Y1,
    TOKEN_BLOCK_UV,
    TOKEN_BLOCK_Y2,
    TOKEN_BLOCK_TYPES,
};

struct dequant_factors
{
    int   quant_idx;
    short factor[TOKEN_BLOCK_TYPES][2]; /* [ Y1, UV, Y2 ] [ DC, AC ] */
};


struct ref_cnt_img
{
    vpx_image_t  img;
    unsigned int ref_cnt;
};


struct vp8_decoder_ctx
{
    struct vpx_internal_error_info  error;
    unsigned int                    frame_cnt;

    struct vp8_frame_hdr            frame_hdr;
    struct vp8_segment_hdr          segment_hdr;
    struct vp8_loopfilter_hdr       loopfilter_hdr;
    struct vp8_token_hdr            token_hdr;
    struct vp8_quant_hdr            quant_hdr;
    struct vp8_reference_hdr        reference_hdr;
    struct vp8_entropy_hdr          entropy_hdr;

    struct vp8_entropy_hdr          saved_entropy;
    unsigned int                    saved_entropy_valid;

    unsigned int                    mb_rows;
    unsigned int                    mb_cols;
    struct mb_info                 *mb_info_storage;
    struct mb_info                **mb_info_rows_storage;
    struct mb_info                **mb_info_rows;

    token_entropy_ctx_t            *above_token_entropy_ctx;
    struct token_decoder            tokens[MAX_PARTITIONS];
    struct dequant_factors          dequant_factors[MAX_MB_SEGMENTS];

    struct ref_cnt_img              frame_strg[NUM_REF_FRAMES];
    struct ref_cnt_img             *ref_frames[NUM_REF_FRAMES];
    ptrdiff_t                       ref_frame_offsets[4];

    const filter_t                 *subpixel_filters;
};


void
vp8_dixie_decode_init(struct vp8_decoder_ctx *ctx);


void
vp8_dixie_decode_destroy(struct vp8_decoder_ctx *ctx);


vpx_codec_err_t
vp8_parse_frame_header(const unsigned char   *data,
                       unsigned int           sz,
                       struct vp8_frame_hdr  *hdr);


vpx_codec_err_t
vp8_dixie_decode_frame(struct vp8_decoder_ctx *ctx,
                       const unsigned char    *data,
                       unsigned int            sz);

/* Decode entropy header in frame */
void
decode_entropy_header(struct vp8_decoder_ctx    *ctx,
                      struct bool_decoder       *br,
                      struct vp8_entropy_hdr    *hdr);

/* Decode reference header in frame */
void
decode_reference_header(struct vp8_decoder_ctx    *ctx,
                        struct bool_decoder       *br,
                        struct vp8_reference_hdr  *hdr);

/* Decode quantizer header in frame */
void
decode_quantizer_header(struct vp8_decoder_ctx    *ctx,
                        struct bool_decoder       *br,
                        struct vp8_quant_hdr      *hdr);

/* Decode token partitions */
void
decode_and_init_token_partitions(struct vp8_decoder_ctx    *ctx,
                                 struct bool_decoder       *br,
                                 const unsigned char       *data,
                                 unsigned int               sz,
                                 struct vp8_token_hdr      *hdr);

/* Decode loop filter header */
void
decode_loopfilter_header(struct vp8_decoder_ctx    *ctx,
                         struct bool_decoder       *br,
                         struct vp8_loopfilter_hdr *hdr);

/* Decode segmentation header */
void
decode_segmentation_header(struct vp8_decoder_ctx *ctx,
                           struct bool_decoder    *br,
                           struct vp8_segment_hdr *hdr);

/* Decode entire frame */
void
decode_frame(struct vp8_decoder_ctx *ctx,
             const unsigned char    *data,
             unsigned int            sz);

/* ARRAY COPY */
#define ARRAY_COPY(a,b) {\
    assert(sizeof(a)==sizeof(b));memcpy(a,b,sizeof(a));}

/* FRAME HEADER enums */
enum
{
    FRAME_HEADER_SZ = 3,
    KEYFRAME_HEADER_SZ = 7
};
#define CLAMP_255(x) ((x)<0?0:((x)>255?255:(x)))

#endif
