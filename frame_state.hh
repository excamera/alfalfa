#ifndef FRAME_STATE_HH_
#define FRAME_STATE_HH_

#include "./dixie.h"

struct vp8_raster_buffer_ids {
  unsigned int lf_number;    /* Raster number of last_ref */
  unsigned int gf_number;    /* Raster number of golden */
  unsigned int ar_number;    /* Raster number of altref */
  unsigned int raster_num;   /* Raster number of current */
};

class FrameState {
 public:
  /* Constructor */
  FrameState(struct vp8_frame_hdr         t_frame_hdr,
             struct vp8_segment_hdr       t_segment_hdr,
             struct vp8_loopfilter_hdr    t_loopfilter_hdr,
             struct vp8_token_hdr         t_token_hdr,
             struct vp8_quant_hdr         t_quant_hdr,
             struct vp8_reference_hdr     t_reference_hdr,
             struct vp8_entropy_hdr       t_entropy_hdr,
             struct vp8_raster_buffer_ids t_raster_ids)
      : frame_hdr(t_frame_hdr),
        segment_hdr(t_segment_hdr),
        loopfilter_hdr(t_loopfilter_hdr),
        token_hdr(t_token_hdr),
        quant_hdr(t_quant_hdr),
        reference_hdr(t_reference_hdr),
        entropy_hdr(t_entropy_hdr),
        raster_ids(t_raster_ids) {}

  /* Print all state (per frame and intra frame) */
  void pretty_print_everything(void) const;

  /* Print only inter frame state (probability tables) */
  void pretty_print_inter_frame_state(void) const;

 private:
  /* Pretty printing */
  void pretty_print_frame_hdr(void)      const;
  void pretty_print_segment_hdr(void)    const;
  void pretty_print_loopfilter_hdr(void) const;
  void pretty_print_token_hdr(void)      const;
  void pretty_print_quant_hdr(void)      const;
  void pretty_print_reference_hdr(void)  const;
  void pretty_print_raster_numbers(void) const;
  void pretty_print_frame_deps(void)     const;
  void pretty_print_entropy_hdr(void)    const;

  /* Frame state */
  const struct vp8_frame_hdr      frame_hdr;         /* Frame scaling */
  const struct vp8_segment_hdr    segment_hdr;       /* Segmentation state */
  const struct vp8_loopfilter_hdr loopfilter_hdr;    /* Loop filter state */
  const struct vp8_token_hdr      token_hdr;         /* # partitions and sizes */
  const struct vp8_quant_hdr      quant_hdr;         /* Quantizing coeffs */
  const struct vp8_reference_hdr  reference_hdr;     /* ref. frame */
  const struct vp8_entropy_hdr    entropy_hdr;       /* Probabilities */
  const struct vp8_raster_buffer_ids raster_ids;       /* ids of current, golden, altref rasters */
};

#endif  // FRAME_STATE_HH_
