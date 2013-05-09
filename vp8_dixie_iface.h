#ifdef __cplusplus
extern "C" {
#endif

#ifndef VP8_DIXIE_IFACE_H_
#define VP8_DIXIE_IFACE_H_

#include "./vpx_codec.h"
#include "./vpx_decoder.h"
#include "./vpx_image.h"
#include "./dixie.h"

vpx_codec_err_t vp8_init(vpx_codec_ctx_t *ctx);

typedef vpx_codec_stream_info_t  vp8_stream_info_t;

struct vpx_codec_alg_priv
{
    vpx_codec_priv_t        base;
    vpx_codec_dec_cfg_t     cfg;
    vp8_stream_info_t       si;
    struct vp8_decoder_ctx  decoder_ctx;
    vpx_image_t            *img;
    int                     img_avail;
};

#endif  // VP8_DIXIE_IFACE_H_

#ifdef __cplusplus
}
#endif
