/*
 * Copyright © 2023 Rémi Denis-Courmont.
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

#include "config.h"

#include <ffmpeg/libavutil/attributes.h>
#include <ffmpeg/libavutil/cpu.h>
#include <ffmpeg/libavcodec/lossless_videoencdsp.h>

void ff_llvidenc_diff_bytes_rvv(uint8_t *dst, const uint8_t *src1,
                                const uint8_t *src2, intptr_t w);

av_cold void ff_llvidencdsp_init_riscv(LLVidEncDSPContext *c)
{
#if HAVE_RVV
    int flags = av_get_cpu_flags();

    if (flags & AV_CPU_FLAG_RVV_I32) {
        c->diff_bytes = ff_llvidenc_diff_bytes_rvv;
    }
#endif
}
