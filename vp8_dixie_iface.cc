/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>
#include <string.h>
#include "vpx_decoder.hh"
#include "vpx_codec_internal.hh"
#include "vpx_mem.hh"
#include "vpx_version.hh"
#include "dixie.hh"
#include "vp8_dixie_iface.hh"

static vpx_codec_err_t
update_error_state(vpx_codec_alg_priv_t                 *ctx,
                   const struct vpx_internal_error_info *error)
{
    vpx_codec_err_t res;

    if ((res = error->error_code))
        ctx->base.err_detail = error->has_detail
                               ? error->detail
                               : NULL;

    return res;
}


vpx_codec_err_t vp8_init(vpx_codec_ctx_t *ctx)
{
    vpx_codec_err_t        res = VPX_CODEC_OK;

    /* This function only allocates space for the vpx_codec_alg_priv_t
     * structure. More memory may be required at the time the stream
     * information becomes known.
     */
    if (!ctx->priv)
    {
        void *priv = vpx_calloc(1, sizeof(vpx_codec_alg_priv_t));

        ctx->priv = priv;

        if (!ctx->priv)
            return VPX_CODEC_MEM_ERROR;

        ctx->priv->sz = sizeof(vpx_codec_alg_priv_t);
        ctx->priv->iface = ctx->iface;
        ctx->priv->alg_priv = priv;
        ctx->priv->init_flags = ctx->init_flags;

        if (ctx->config.dec)
        {
            /* Update the reference to the config structure to our copy. */
            ctx->priv->alg_priv->cfg = *ctx->config.dec;
            ctx->config.dec = &ctx->priv->alg_priv->cfg;
        }
    }

    return res;
}


static vpx_codec_err_t vp8_destroy(vpx_codec_alg_priv_t *ctx)
{
    vp8_dixie_decode_destroy(&ctx->decoder_ctx);
    vpx_free(ctx->base.alg_priv);
    return VPX_CODEC_OK;
}


static vpx_codec_err_t vp8_peek_si(const uint8_t         *data,
                                   unsigned int           data_sz,
                                   vpx_codec_stream_info_t *si)
{
    struct vp8_frame_hdr  hdr;
    vpx_codec_err_t       res = VPX_CODEC_OK;


    if (!(res = vp8_parse_frame_header(data, data_sz, &hdr)))
    {
        si->is_kf = hdr.is_keyframe;

        if (si->is_kf)
        {
            si->w = hdr.kf.w;
            si->h = hdr.kf.h;
        }
        else
        {
            si->w = 0;
            si->h = 0;
        }
    }

    return res;

}


static vpx_codec_err_t vp8_get_si(vpx_codec_alg_priv_t    *ctx,
                                  vpx_codec_stream_info_t *si)
{

    unsigned int sz;

    if (si->sz >= sizeof(vp8_stream_info_t))
        sz = sizeof(vp8_stream_info_t);
    else
        sz = sizeof(vpx_codec_stream_info_t);

    memcpy(si, &ctx->si, sz);
    si->sz = sz;

    return VPX_CODEC_OK;
}


static vpx_codec_err_t vp8_decode(vpx_codec_alg_priv_t  *ctx,
                                  const uint8_t         *data,
                                  unsigned int            data_sz,
                                  void                    *user_priv,
                                  long                    deadline)
{
    vpx_codec_err_t res = VPX_CODEC_OK;

    res = vp8_dixie_decode_frame(&ctx->decoder_ctx, data, data_sz);
    if(res)
        update_error_state(ctx, &ctx->decoder_ctx.error);

    ctx->img_avail = ctx->decoder_ctx.frame_hdr.is_shown;
    ctx->img = &ctx->decoder_ctx.ref_frames[CURRENT_FRAME]->img;
    return res;
}


static vpx_image_t *vp8_get_frame(vpx_codec_alg_priv_t  *ctx,
                                  vpx_codec_iter_t      *iter)
{
    vpx_image_t *img = NULL;

    if (ctx->img_avail)
    {
        /* iter acts as a flip flop, so an image is only returned on the first
         * call to get_frame.
         */
        if (!(*iter))
        {
            img = ctx->img;
            *iter = img;
        }
    }

    return img;
}


static vpx_codec_ctrl_fn_map_t ctf_maps[] =
{
    { -1, NULL},
};


#ifndef VERSION_STRING
#define VERSION_STRING
#endif
vpx_codec_iface_t vpx_codec_vp8_dixie_algo =
{
    "VP8 \"Dixie\" Decoder" VERSION_STRING,
    VPX_CODEC_INTERNAL_ABI_VERSION,
    VPX_CODEC_CAP_DECODER,
    /* vpx_codec_caps_t          caps; */
    vp8_init,         /* vpx_codec_init_fn_t       init; */
    vp8_destroy,      /* vpx_codec_destroy_fn_t    destroy; */
    ctf_maps,         /* vpx_codec_ctrl_fn_map_t  *ctrl_maps; */
    NULL,             /* vpx_codec_get_mmap_fn_t   get_mmap; */
    NULL,             /* vpx_codec_set_mmap_fn_t   set_mmap; */
    {
        vp8_peek_si,      /* vpx_codec_peek_si_fn_t    peek_si; */
        vp8_get_si,       /* vpx_codec_get_si_fn_t     get_si; */
        vp8_decode,       /* vpx_codec_decode_fn_t     decode; */
        vp8_get_frame,    /* vpx_codec_frame_get_fn_t  frame_get; */
    },
    {NOT_IMPLEMENTED} /* encoder functions */
};
