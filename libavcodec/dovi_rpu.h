/*
 * Dolby Vision RPU decoder
 *
 * Copyright (C) 2021 Jan Ekström
 * Copyright (C) 2021 Niklas Haas
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_DOVI_RPU_H
#define AVCODEC_DOVI_RPU_H

#include <ffmpeg/libavutil/dovi_meta.h>
#include <ffmpeg/libavutil/frame.h>

#include "avcodec.h"
#include "codec_par.h"

#define DOVI_MAX_DM_ID 15

typedef struct DOVIExt {
    AVDOVIDmData dm_static[7];   ///< static extension blocks
    AVDOVIDmData dm_dynamic[25]; ///< dynamic extension blocks
    int num_static;
    int num_dynamic;
} DOVIExt;

typedef struct DOVIContext {
    void *logctx;

    /**
     * Enable tri-state. For encoding only. FF_DOVI_AUTOMATIC enables Dolby
     * Vision only if avctx->decoded_side_data contains an AVDOVIMetadata.
     */
#define FF_DOVI_AUTOMATIC -1
    int enable;

    /**
     * Currently active dolby vision configuration, or {0} for none.
     * Set by the user when decoding. Generated by ff_dovi_configure()
     * when encoding.
     *
     * Note: sizeof(cfg) is not part of the libavutil ABI, so users should
     * never pass &cfg to any other library calls. This is included merely as
     * a way to look up the values of fields known at compile time.
     */
    AVDOVIDecoderConfigurationRecord cfg;

    /**
     * Currently active RPU data header, updates on every ff_dovi_rpu_parse()
     * or ff_dovi_rpu_generate().
     */
    AVDOVIRpuDataHeader header;

    /**
     * Currently active data mappings, or NULL. Points into memory owned by the
     * corresponding rpu/vdr_ref, which becomes invalid on the next call to
     * ff_dovi_rpu_parse() or ff_dovi_rpu_generate().
     */
    const AVDOVIDataMapping *mapping;
    const AVDOVIColorMetadata *color;

    /**
     * Currently active extension blocks, updates on every ff_dovi_rpu_parse()
     * or ff_dovi_rpu_generate().
     */
    DOVIExt *ext_blocks; ///< RefStruct, or NULL if no extension blocks

    /**
     * Private fields internal to dovi_rpu.c
     */
    AVDOVIColorMetadata *dm; ///< RefStruct
    AVDOVIDataMapping *vdr[DOVI_MAX_DM_ID+1]; ///< RefStruct references
    uint8_t *rpu_buf; ///< temporary buffer
    unsigned rpu_buf_sz;

} DOVIContext;

void ff_dovi_ctx_replace(DOVIContext *s, const DOVIContext *s0);

/**
 * Completely reset a DOVIContext, preserving only logctx.
 */
void ff_dovi_ctx_unref(DOVIContext *s);

/**
 * Partially reset the internal state. Resets per-frame state, but preserves
 * the stream-wide configuration record.
 */
void ff_dovi_ctx_flush(DOVIContext *s);

/**
 * Parse the contents of a Dolby Vision RPU and update the parsed values in the
 * DOVIContext struct. This function should receive the decoded unit payload,
 * without any T.35 or NAL unit headers.
 *
 * Returns 0 or an error code.
 *
 * Note: `DOVIContext.cfg` should be initialized before calling into this
 * function. If not done, the profile will be guessed according to HEVC
 * semantics.
 */
int ff_dovi_rpu_parse(DOVIContext *s, const uint8_t *rpu, size_t rpu_size,
                      int err_recognition);

/**
 * Get the decoded AVDOVIMetadata. Ownership passes to the caller.
 *
 * Returns the size of *out_metadata, a negative error code, or 0 if no
 * metadata is available to return.
 */
int ff_dovi_get_metadata(DOVIContext *s, AVDOVIMetadata **out_metadata);

/**
 * Attach the decoded AVDOVIMetadata as side data to an AVFrame.
 * Returns 0 or a negative error code.
 */
int ff_dovi_attach_side_data(DOVIContext *s, AVFrame *frame);

/**
 * Configure the encoder for Dolby Vision encoding. Generates a configuration
 * record in s->cfg, and attaches it to avctx->coded_side_data. Sets the correct
 * profile and compatibility ID based on the tagged AVCodecParameters colorspace
 * metadata, and the correct level based on the resolution and tagged framerate.
 *
 * `metadata` should point to the first frame's RPU, if available. If absent,
 * auto-detection will be performed, but this can sometimes lead to inaccurate
 * results (in particular for HEVC streams with enhancement layers).
 *
 * Returns 0 or a negative error code.
 */
int ff_dovi_configure_ext(DOVIContext *s, AVCodecParameters *codecpar,
                          const AVDOVIMetadata *metadata,
                          enum AVDOVICompression compression,
                          int strict_std_compliance);

/**
 * Helper wrapper around `ff_dovi_configure_ext` which infers the codec
 * parameters from an AVCodecContext.
 */
int ff_dovi_configure(DOVIContext *s, AVCodecContext *avctx);

enum {
    FF_DOVI_WRAP_NAL        = 1 << 0, ///< wrap inside NAL RBSP
    FF_DOVI_WRAP_T35        = 1 << 1, ///< wrap inside T.35+EMDF
    FF_DOVI_COMPRESS_RPU    = 1 << 2, ///< enable compression for this RPU
};

/**
 * Synthesize a Dolby Vision RPU reflecting the current state. By default, the
 * RPU is not encapsulated (see `flags` for more options). Note that this
 * assumes all previous calls to `ff_dovi_rpu_generate` have been
 * appropriately signalled, i.e. it will not re-send already transmitted
 * redundant data.
 *
 * Mutates the internal state of DOVIContext to reflect the change.
 * Returns 0 or a negative error code.
 */
int ff_dovi_rpu_generate(DOVIContext *s, const AVDOVIMetadata *metadata,
                         int flags, uint8_t **out_rpu, int *out_size);


/***************************************************
 * The following section is for internal use only. *
 ***************************************************/

enum {
    RPU_COEFF_FIXED = 0,
    RPU_COEFF_FLOAT = 1,
};

/**
 * Internal helper function to guess the correct DV profile for HEVC.
 *
 * Returns the profile number or 0 if unknown.
 */
int ff_dovi_guess_profile_hevc(const AVDOVIRpuDataHeader *hdr);

/* Default values for AVDOVIColorMetadata */
extern const AVDOVIColorMetadata ff_dovi_color_default;

static inline int ff_dovi_rpu_extension_is_static(int level)
{
    switch (level) {
    case 6:
    case 10:
    case 32: /* reserved as static by spec */
    case 254:
    case 255:
        return 1;
    default:
        return 0;
    }
}

#endif /* AVCODEC_DOVI_RPU_H */
