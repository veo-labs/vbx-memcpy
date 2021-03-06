/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

        .text
        .fpu    neon

        .global memcpy_imx
        .type memcpy_imx, %function
        .align 4

/* a prefetch distance of 4 cache-lines works best experimentally */
#define CACHE_LINE_SIZE     64
#define PREFETCH_DISTANCE   (CACHE_LINE_SIZE*4)

memcpy_imx:
        .fnstart
        .save       {r0, lr}
        stmfd       sp!, {r0, lr}

        /* start preloading as early as possible */
        pld         [r1, #(CACHE_LINE_SIZE*0)]
        pld         [r1, #(CACHE_LINE_SIZE*1)]

        /* do we have at least 16-bytes to copy (needed for alignment below) */
        cmp         r2, #16
        blo         5f

        /* align destination to half cache-line for the write-buffer */
        rsb         r3, r0, #0
        ands        r3, r3, #0xF
        beq         0f

        /* copy up to 15-bytes (count in r3) */
        sub         r2, r2, r3
        movs        ip, r3, lsl #31
        ldrmib      lr, [r1], #1
        strmib      lr, [r0], #1
        ldrcsb      ip, [r1], #1
        ldrcsb      lr, [r1], #1
        strcsb      ip, [r0], #1
        strcsb      lr, [r0], #1
        movs        ip, r3, lsl #29
        bge         1f
        // copies 4 bytes, destination 32-bits aligned
        vld4.8      {d0[0], d1[0], d2[0], d3[0]}, [r1]!
        vst4.8      {d0[0], d1[0], d2[0], d3[0]}, [r0, :32]!
1:      bcc         2f
        // copies 8 bytes, destination 64-bits aligned
        vld1.8      {d0}, [r1]!
        vst1.8      {d0}, [r0, :64]!
2:

0:      /* preload immediately the next cache line, which we may need */
        pld         [r1, #(CACHE_LINE_SIZE*0)]
        pld         [r1, #(CACHE_LINE_SIZE*1)]

        /* make sure we have at least 64 bytes to copy */
        subs        r2, r2, #64
        blo         2f

        /* preload all the cache lines we need.
         * NOTE: the number of pld below depends on PREFETCH_DISTANCE,
         * ideally would would increase the distance in the main loop to
         * avoid the goofy code below. In practice this doesn't seem to make
         * a big difference.
         */
        pld         [r1, #(CACHE_LINE_SIZE*2)]
        pld         [r1, #(CACHE_LINE_SIZE*3)]
        pld         [r1, #(PREFETCH_DISTANCE)]

1:      /* The main loop copies 64 bytes at a time */
        vld1.8      {d0  - d3},   [r1]!
        vld1.8      {d4  - d7},   [r1]!
        pld         [r1, #(PREFETCH_DISTANCE)]
        subs        r2, r2, #64
        vst1.8      {d0  - d3},   [r0, :128]!
        vst1.8      {d4  - d7},   [r0, :128]!
        bhs         1b

2:      /* fix-up the remaining count and make sure we have >= 32 bytes left */
        add         r2, r2, #64
        subs        r2, r2, #32
        blo         4f

3:      /* 32 bytes at a time. These cache lines were already preloaded */
        vld1.8      {d0 - d3},  [r1]!
        subs        r2, r2, #32
        vst1.8      {d0 - d3},  [r0, :128]!
        bhs         3b

4:      /* less than 32 left */
        add         r2, r2, #32
        tst         r2, #0x10
        beq         5f
        // copies 16 bytes, 128-bits aligned
        vld1.8      {d0, d1}, [r1]!
        vst1.8      {d0, d1}, [r0, :128]!

5:      /* copy up to 15-bytes (count in r2) */
        movs        ip, r2, lsl #29
        bcc         1f
        vld1.8      {d0}, [r1]!
        vst1.8      {d0}, [r0]!
1:      bge         2f
        vld4.8      {d0[0], d1[0], d2[0], d3[0]}, [r1]!
        vst4.8      {d0[0], d1[0], d2[0], d3[0]}, [r0]!
2:      movs        ip, r2, lsl #31
        ldrmib      r3, [r1], #1
        ldrcsb      ip, [r1], #1
        ldrcsb      lr, [r1], #1
        strmib      r3, [r0], #1
        strcsb      ip, [r0], #1
        strcsb      lr, [r0], #1

        ldmfd       sp!, {r0, lr}
        bx          lr
        .fnend

