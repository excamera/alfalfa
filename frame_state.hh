#ifndef FRAME_STATE_HH_
#define FRAME_STATE_HH_

#include "./dixie.h"

class FrameState {
 public:
  FrameState(const struct vp8_frame_hdr      &  t_frame_hdr,
             const struct vp8_segment_hdr    &  t_segment_hdr,
             const struct vp8_loopfilter_hdr &  t_loopfilter_hdr,
             const struct vp8_token_hdr      &  t_token_hdr,
             const struct vp8_quant_hdr      &  t_quant_hdr,
             const struct vp8_reference_hdr  &  t_reference_hdr,
             const struct vp8_entropy_hdr    &  t_entropy_hdr)
      : frame_hdr(t_frame_hdr),
        segment_hdr(t_segment_hdr),
        loopfilter_hdr(t_loopfilter_hdr),
        token_hdr(t_token_hdr),
        quant_hdr(t_quant_hdr),
        reference_hdr(t_reference_hdr),
        entropy_hdr(t_entropy_hdr) {}

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
  const struct vp8_frame_hdr      & frame_hdr;         /* Frame scaling */
  const struct vp8_segment_hdr    & segment_hdr;  /* Segmentation state */
  const struct vp8_loopfilter_hdr & loopfilter_hdr; /* Loop filter state */
  const struct vp8_token_hdr      & token_hdr;       /* # partitions and sizes */
  const struct vp8_quant_hdr      & quant_hdr;           /* Quantizing coeffs */
  const struct vp8_reference_hdr  & reference_hdr;   /* ref. frame */
  const struct vp8_entropy_hdr    & entropy_hdr;       /* Probabilities */
};

#endif  // FRAME_STATE_HH_
