/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "frame.hh"

using namespace std;

template <class FrameHeaderType, class MacroblockType>
Frame<FrameHeaderType, MacroblockType>::Frame( const bool show,
                                               const unsigned int width,
                                               const unsigned int height,
                                               BoolDecoder & first_partition )
  : show_( show ),
    display_width_( width ),
    display_height_( height ),
    header_( first_partition )
{}

template <class FrameHeaderType, class MacroblockType>
Frame<FrameHeaderType, MacroblockType>::Frame( Frame && other ) noexcept
: show_( other.show_ ),
  display_width_( other.display_width_ ),
  display_height_( other.display_height_ ),
  macroblock_width_( other.macroblock_width_ ),
  macroblock_height_( other.macroblock_height_ ),
  Y2_( move( other.Y2_ ) ),
  Y_( move( other.Y_ ) ),
  U_( move( other.U_ ) ),
  V_( move( other.V_ ) ),
  header_( move( other.header_ ) ),
  macroblock_headers_( move( other.macroblock_headers_ ) )
{}

template <class FrameHeaderType, class MacroblockType>
Frame<FrameHeaderType, MacroblockType> &
Frame<FrameHeaderType, MacroblockType>::operator=( Frame && other )
{
  show_ = other.show_;
  display_width_ = other.display_width_;
  display_height_ = other.display_height_;
  macroblock_width_ = other.macroblock_width_;
  macroblock_height_ = other.macroblock_height_;
  Y2_ = move( other.Y2_ );
  Y_ = move( other.Y_ );
  U_ = move( other.U_ );
  V_ = move( other.V_ );
  header_ = move( other.header_ );
  macroblock_headers_ = move( other.macroblock_headers_ );

  return *this;
}

template <class FrameHeaderType, class MacroblockType>
ProbabilityArray< num_segments > Frame<FrameHeaderType, MacroblockType>::calculate_mb_segment_tree_probs( void ) const
{
  /* calculate segment tree probabilities if map is updated by this frame */
  ProbabilityArray< num_segments > mb_segment_tree_probs;

  if ( header_.update_segmentation.initialized()
       and header_.update_segmentation.get().mb_segmentation_map.initialized() ) {
    const auto & seg_map = header_.update_segmentation.get().mb_segmentation_map.get();
    for ( unsigned int i = 0; i < mb_segment_tree_probs.size(); i++ ) {
      mb_segment_tree_probs.at( i ) = seg_map.at( i ).get_or( 255 );
    }
  }

  return mb_segment_tree_probs;
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::parse_macroblock_headers( BoolDecoder & rest_of_first_partition,
                                                                       const ProbabilityTables & probability_tables,
                                                                       const bool error_concealment )
{
  /* calculate segment tree probabilities if map is updated by this frame */
  const ProbabilityArray< num_segments > mb_segment_tree_probs = calculate_mb_segment_tree_probs();

  /* parse the macroblock headers */
  macroblock_headers_.initialize( macroblock_width_, macroblock_height_,
                                  rest_of_first_partition, header_,
                                  mb_segment_tree_probs,
                                  probability_tables,
                                  Y2_, Y_, U_, V_,
                                  error_concealment );

  /* repoint Y2 above/left pointers to skip missing subblocks */
  relink_y2_blocks();
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::update_segmentation( SegmentationMap & mutable_segmentation_map )
{
  macroblock_headers_.get().forall( [&] ( MacroblockType & mb ) { mb.update_segmentation( mutable_segmentation_map ); } );
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::parse_tokens( vector< Chunk > dct_partitions,
                                                           const ProbabilityTables & probability_tables )
{
  vector<BoolDecoder> dct_partition_decoders;
  for ( const auto & x : dct_partitions ) {
    dct_partition_decoders.emplace_back( x );
  }

  /* parse every macroblock's tokens */
  macroblock_headers_.get().forall_ij( [&]( MacroblockType & macroblock,
                                            const unsigned int,
                                            const unsigned int row )
                                       {
                                         macroblock.parse_tokens( dct_partition_decoders.at( row % dct_partition_decoders.size() ),
                                                                  probability_tables ); } );
}

template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::loopfilter( const Optional< Segmentation > & segmentation,
                                                         const Optional< FilterAdjustments > & filter_adjustments,
                                                         VP8Raster & raster ) const
{
  if ( header_.loop_filter_level ) {
    /* calculate per-segment filter adjustments if
       segmentation is enabled */

    const FilterParameters frame_loopfilter( header_.filter_type,
                                             header_.loop_filter_level,
                                             header_.sharpness_level );

    SafeArray< FilterParameters, num_segments > segment_loopfilters;

    if ( segmentation.initialized() ) {
      for ( uint8_t i = 0; i < num_segments; i++ ) {
        FilterParameters segment_filter( header_.filter_type,
                                         header_.loop_filter_level,
                                         header_.sharpness_level );
        segment_filter.filter_level = segmentation.get().segment_filter_adjustments.at( i )
          + ( segmentation.get().absolute_segment_adjustments
              ? 0
              : segment_filter.filter_level );

        segment_loopfilters.at( i ) = segment_filter;
      }
    }

    /* the macroblock needs to know whether the mode- and reference-based
       filter adjustments are enabled */

    macroblock_headers_.get().forall_ij( [&]( const MacroblockType & macroblock,
                                              const unsigned int column,
                                              const unsigned int row )
                                         {
                                           VP8Raster::Macroblock output = raster.macroblock( column, row );
                                           macroblock.loopfilter( filter_adjustments,
                                                                  segmentation.initialized()
                                                                  ? segment_loopfilters.at( macroblock.segment_id() )
                                                                  : frame_loopfilter,
                                                                  output ); } );
  }
}


template <class FrameHeaderType, class MacroblockType>
SafeArray<Quantizer, num_segments> Frame<FrameHeaderType, MacroblockType>::calculate_segment_quantizers( const Optional< Segmentation > & segmentation ) const
{
  /* calculate per-segment quantizer adjustments if
     segmentation is enabled */

  SafeArray< Quantizer, num_segments > segment_quantizers;

  if ( segmentation.initialized() ) {
    for ( uint8_t i = 0; i < num_segments; i++ ) {
      QuantIndices segment_indices( header_.quant_indices );
      segment_indices.y_ac_qi = segmentation.get().segment_quantizer_adjustments.at( i )
        + ( segmentation.get().absolute_segment_adjustments
            ? static_cast<Unsigned<7>>( 0 )
            : segment_indices.y_ac_qi );

      segment_quantizers.at( i ) = Quantizer( segment_indices );
    }
  }

  return segment_quantizers;
}

template <>
void KeyFrame::decode( const Optional< Segmentation > & segmentation, const References &,
                       VP8Raster & raster ) const
{
  const Quantizer frame_quantizer( header_.quant_indices );
  const auto segment_quantizers = calculate_segment_quantizers( segmentation );

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const KeyFrameMacroblock & macroblock,
                                            const unsigned int column,
                                            const unsigned int row ) {
                                         const auto & quantizer = segmentation.initialized()
                                           ? segment_quantizers.at( macroblock.segment_id() )
                                           : frame_quantizer;
                                         VP8Raster::Macroblock output = raster.macroblock( column, row );
                                         macroblock.reconstruct_intra( quantizer, output );
                                       } );
}

template <>
void InterFrame::decode( const Optional<Segmentation> & segmentation, const References & references,
                         VP8Raster & raster ) const
{
  const Quantizer frame_quantizer( header_.quant_indices );
  const auto segment_quantizers = calculate_segment_quantizers( segmentation );

  /* process each macroblock */
  macroblock_headers_.get().forall_ij( [&]( const InterFrameMacroblock & macroblock,
                                            const unsigned int column,
                                            const unsigned int row ) {
                                         const auto & quantizer = segmentation.initialized()
                                           ? segment_quantizers.at( macroblock.segment_id() )
                                           : frame_quantizer;
                                         VP8Raster::Macroblock output = raster.macroblock( column, row );
                                         if ( macroblock.inter_coded() ) {
                                           macroblock.reconstruct_inter( quantizer,
                                                                         references,
                                                                         output );
                                         } else {
                                           macroblock.reconstruct_intra( quantizer,
                                                                         output );
                                         } } );
}

/* "above" for a Y2 block refers to the first macroblock above that actually has Y2 coded */
/* here we relink the "above" and "left" pointers after we learn the prediction mode
   for the block */
template <class FrameHeaderType, class MacroblockType>
void Frame<FrameHeaderType, MacroblockType>::relink_y2_blocks( void )
{
  vector< Optional< const Y2Block * > > above_coded( macroblock_width_ );
  vector< Optional< const Y2Block * > > left_coded( macroblock_height_ );

  Y2_.forall_ij( [&]( Y2Block & block, const unsigned int column, const unsigned int row ) {
      block.set_above( above_coded.at( column ) );
      block.set_left( left_coded.at( row ) );
      if ( block.coded() ) {
        above_coded.at( column ) = &block;
        left_coded.at( row ) = &block;
      }
    } );
}

template <>
void KeyFrame::copy_to( const RasterHandle & raster, References & references ) const
{
  references.last = references.golden = references.alternative = raster;
}

template <>
void InterFrame::copy_to( const RasterHandle & raster, References & references ) const
{
  if ( header_.copy_buffer_to_alternate.initialized() ) {
    if ( header_.copy_buffer_to_alternate.get() == 1 ) {
      references.alternative = references.last;
    } else if ( header_.copy_buffer_to_alternate.get() == 2 ) {
      references.alternative = references.golden;
    }
  }

  if ( header_.copy_buffer_to_golden.initialized() ) {
    if ( header_.copy_buffer_to_golden.get() == 1 ) {
      references.golden = references.last;
    } else if ( header_.copy_buffer_to_golden.get() == 2 ) {
      references.golden = references.alternative;
    }
  }

  if ( header_.refresh_golden_frame ) {
    references.golden = raster;
  }

  if ( header_.refresh_alternate_frame ) {
    references.alternative = raster;
  }

  if ( header_.refresh_last ) {
    references.last = raster;
  }
}

template<>
string InterFrame::reference_update_stats( void ) const
{
  string stats = "\t";
  if ( header_.copy_buffer_to_alternate.initialized() ) {
    if ( header_.copy_buffer_to_alternate.get() == 1 ) {
      stats += "ALT with LAST ";
    } else if ( header_.copy_buffer_to_alternate.get() == 2 ) {
      stats += "ALT with GOLD ";
    }
  }

  if ( header_.copy_buffer_to_golden.initialized() ) {
    if ( header_.copy_buffer_to_golden.get() == 1 ) {
      stats += "GOLD with LAST ";
    } else if ( header_.copy_buffer_to_golden.get() == 2 ) {
      stats += "GOLD with ALT ";
    }
  }

  if ( header_.refresh_golden_frame ) {
    stats += "refresh GOLD ";
  }

  if ( header_.refresh_alternate_frame ) {
    stats += "refresh ALT ";
  }

  if ( header_.refresh_last ) {
    stats += "refresh LAST ";
  }

  return stats + "\n";
}

template<>
string KeyFrame::stats( void ) const
{
  return "";
}

template<>
string InterFrame::stats( void ) const
{
  int total_macroblocks = macroblock_width_ * macroblock_height_;
  int inter_coded_macroblocks = 0;
  /* Tracks the percentage of inter coded blocks that refer to a given reference */
  array<int, 4> ref_percentages;
  ref_percentages.fill( 0 );

  macroblock_headers_.get().forall( [&] ( const InterFrameMacroblock & macroblock )
                                    {
                                      if ( macroblock.inter_coded() ) {
                                        inter_coded_macroblocks++;
                                        ref_percentages[ macroblock.header().reference() ]++;
                                      }
                                    } );

  return "\tPercentage Inter Coded: " +
    to_string( (double)inter_coded_macroblocks * 100 / total_macroblocks ) + "%\n" +
    "\tLast: " + to_string( (double)ref_percentages[ 1 ] * 100 / inter_coded_macroblocks ) + "%" +
    " Golden: " + to_string( (double)ref_percentages[ 2 ] * 100 / inter_coded_macroblocks ) + "%" +
    " Alternate: " + to_string( (double)ref_percentages[ 3 ] * 100 / inter_coded_macroblocks ) + "%\n" +
    reference_update_stats();
}

template <class FrameHeaderType, class MacroblockType>
bool Frame<FrameHeaderType, MacroblockType>::operator==( const Frame & other ) const
{
  return Y2_ == other.Y2_ and Y_ == other.Y_ and U_ == other.U_ and V_ == other.V_ and
         macroblock_headers_ == other.macroblock_headers_ and
         header_ == other.header_;
}

/* empty frame */
template <class FrameHeaderType, class MacroblockType>
Frame<FrameHeaderType, MacroblockType>::Frame( const uint16_t width, const uint16_t height )
  : Frame( true, width, height, BoolDecoder::zero_decoder() )
{
  parse_macroblock_headers( BoolDecoder::zero_decoder(), ProbabilityTables {}, false );
}

template class Frame<KeyFrameHeader, KeyFrameMacroblock>;
template class Frame<InterFrameHeader, InterFrameMacroblock>;
