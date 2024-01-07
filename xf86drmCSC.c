/**
 * file xf86drmcsc.c
 * User-level interface to DRM device
 * to control color space conversion
 * author Uma Shankar <uma.shankar@intel.com>
 */

/*
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#define stat_t struct stat
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdarg.h>
#include "i915_drm.h"
#include "xf86drm.h"
#include "intel/intel_chipset.h"

#define false 0
#define true 1

#define CSC_BIT_SHIFT(x) (1 << (x))
#define CSC_SETBIT(x,y) ((x) |= (y))
#define VLV2CSC_MAX_MANTISSA_PRECISION 10
#define CSC_TWOSCOMPLEMENT(x) ((~x)+1)
#define BIT10 (1<<10)

#define HSW_CSC_MAX_MANTISSA_PRECISION	9
#define HSW_CSC_SIGN_BIT		15
#define HSW_MANTISSA_MASK		0x1FF
#define HSW_EXPONENT_MASK		0x7
#define HSW_MANTISSA_OFFSET		3
#define HSW_EXPONENT_OFFSET		12
#define HSW_EXPONENT_MAGIC_NO		8

#define VLV_CSC_COEFF_MAX_RANGE		1.999f
#define HSW_CSC_COEFF_MAX_RANGE		2.999f
#define HSW_CSC_OFFSET_MAX_RANGE	0.999f
#define HSW_CSC_OFFSET_MASK		0x1FFF
#define HSW_CSC_OFFSET_BITS		13

static float g_defaultCSCInit[9] = { 1, 0 , 0, 0, 1, 0, 0, 0, 1};
static struct drm_intel_csc_params g_defaultCSCParamas;

static void BitReversal_func(unsigned short *pArg, unsigned short BitCount)
{
    unsigned short tmp1 = 0;
    unsigned short tmp2, cnt;

    tmp2 = *pArg;
    for (cnt = 0; cnt < BitCount; cnt++ ) {
        if(tmp2 & CSC_BIT_SHIFT(cnt))
            CSC_SETBIT(tmp1, CSC_BIT_SHIFT(BitCount -1 - cnt));
    }

    *pArg = tmp1;
}

/*
 * Converts the floating point pre and post offsets into binary format
 */
static void Convert_CSC_Offset_ToBSpecFormat(struct CSCCoeff_Matrix *CSC_Matrix,
						struct csc_coeff *CSC_Coeff_t)
{
    float offset = 0;
    unsigned short twosCompliment, Binary, count, post = 0;
    unsigned short Bit_Count = 0, NegativeOffset = 0, bRoundoff = false;
    unsigned int *pDest;

    do {
        for (count = 0; count < CSC_MAX_OFFSET_COUNT; count++) {
            if (!post)
                pDest = (CSC_Coeff_t->csc_preoffset + count);
            else
                pDest = (CSC_Coeff_t->csc_postoffset + count);

            if (!post)
                offset = CSC_Matrix->CSCPreoffset[count];
            else
                offset = CSC_Matrix->CSCPostoffset[count];

            Bit_Count = 0;
            Binary = 0;
            NegativeOffset = 0;

            if (offset == 0) {
                *pDest = 0;
                continue;
            }

            if (offset < 0) {
                offset = offset * - 1;
                NegativeOffset = 1;
            }

            /* Clip to valid range [-0.999 to +0.999] */
            if (offset > HSW_CSC_OFFSET_MAX_RANGE)
                offset = HSW_CSC_OFFSET_MAX_RANGE;

            do {
                offset = offset * 2;
                if (offset >= 1) {
                    CSC_SETBIT(Binary, CSC_BIT_SHIFT(Bit_Count));
                    offset = offset - 1;
                }

                Bit_Count++;
            } while(offset != 0 &&  Bit_Count <= HSW_CSC_OFFSET_BITS);

            /* Roundoff, if 14th bit(Ignored) is '1' */
            if (Binary & CSC_BIT_SHIFT(HSW_CSC_OFFSET_BITS))
                bRoundoff = true;

            /* Reverse  last 13 bits. */
            BitReversal_func(&Binary, HSW_CSC_OFFSET_BITS);

            if (bRoundoff == true) {
                Binary++;
                bRoundoff = false;
            }

            Binary &= HSW_CSC_OFFSET_MASK;
            twosCompliment = Binary;

            /* convert to 2's compliment */
            if (NegativeOffset)
                twosCompliment = CSC_TWOSCOMPLEMENT(Binary);

            /* mask other bits except  bit [12-0] */
            twosCompliment &= HSW_CSC_OFFSET_MASK;

            *pDest = twosCompliment;
        }
    post++;
    } while(post < 2);

    return;
}

/*
 * Converts the floating point coefficients into binary format
 * based on the platform expectation
 */
static void Convert_Coeff_ToBinary(struct drm_intel_csc_params *csc_params,
					unsigned short *Coeff_binary, int devid)
{
    float coeff = 0;
    unsigned short twosCompliment, Binary;
    unsigned short Bit_Count = 0, count, Exponent;
    unsigned short bGreaterThanOne, bInitial1Occured, bRoundoff = false;

    for (count = 0; count < CSC_MAX_COEFF_COUNT; count++) {
        coeff = csc_params->m_CSCCoeff[count];
        Bit_Count = 0;
        Binary = 0;
        bGreaterThanOne = false;
        bInitial1Occured = false;
        Exponent = 0;

        if (coeff == 0) {
            *(Coeff_binary+count) = 0;
            continue;
        }

        if (coeff < 0) {
            coeff = coeff * - 1;
        }

        if (IS_VALLEYVIEW(devid)) {

            /* Clip to valid range [-1.999 to +1.999] */
            if (coeff > VLV_CSC_COEFF_MAX_RANGE)
                coeff = VLV_CSC_COEFF_MAX_RANGE;

            if (coeff >= 1) {
                coeff = coeff - 1;
                bGreaterThanOne = true;
            }

            do {
                coeff = coeff * 2;
                if (coeff >= 1) {
                    CSC_SETBIT(Binary, CSC_BIT_SHIFT(Bit_Count));
                    coeff = coeff - 1;
                }

                Bit_Count++;
            } while(coeff != 0 &&  Bit_Count < VLV2CSC_MAX_MANTISSA_PRECISION);

            /* Reverse  last 10 bits. */
            BitReversal_func(&Binary, VLV2CSC_MAX_MANTISSA_PRECISION);
            /* 11th bit is for first digit before radix 1.xxxxx */
            if (bGreaterThanOne)
                CSC_SETBIT(Binary, BIT10);
            twosCompliment = Binary;

            /* convert to 2's compliment */
            if (csc_params->m_CSCCoeff[count] < 0) {
                twosCompliment = CSC_TWOSCOMPLEMENT(Binary);
                twosCompliment &= 0xFFF; /* mask other bits except bit [11-0] */
            }

            *(Coeff_binary + count) =  twosCompliment;
        } else if (IS_HASWELL(devid) || IS_BROADWELL(devid)) {

            /* Clip to valid range [-2.999 to +2.999] */
            if (coeff > HSW_CSC_COEFF_MAX_RANGE)
                coeff = HSW_CSC_COEFF_MAX_RANGE;

            /* If coeff is >=1 and >=2 actions are taken here */
            if (coeff >= 1) {
                Bit_Count += (unsigned short)coeff;
                Exponent = HSW_EXPONENT_MAGIC_NO - (unsigned short)coeff;
                coeff -= (unsigned short)coeff;
                bGreaterThanOne = true;
                bInitial1Occured = true;
            }

            do {
                coeff = coeff * 2;
                if (coeff >= 1) {
                    bInitial1Occured = true;
                    CSC_SETBIT(Binary, CSC_BIT_SHIFT(Bit_Count));
                    coeff = coeff - 1;
                    Bit_Count++;
                } else {
                    if((bInitial1Occured == false) && (Exponent < 3))
                        Exponent++;
                    else
                        Bit_Count++;
                }
            } while(coeff != 0 && Bit_Count <= HSW_CSC_MAX_MANTISSA_PRECISION);

            /* Roundoff, if 10th bit(Ignored) is '1' */
            if (Binary & CSC_BIT_SHIFT(HSW_CSC_MAX_MANTISSA_PRECISION))
                bRoundoff = true;

            BitReversal_func(&Binary, HSW_CSC_MAX_MANTISSA_PRECISION);

            if (bRoundoff == true) {
                Binary++;
                bRoundoff = false;
            }

            if (bGreaterThanOne)
                CSC_SETBIT(Binary,
                        CSC_BIT_SHIFT(HSW_CSC_MAX_MANTISSA_PRECISION - 1));
                        /* MSB is for first digit before radix 1.xxxxx */

            Binary &= HSW_MANTISSA_MASK;
            Binary = Binary << HSW_MANTISSA_OFFSET;
            Binary |= ((Exponent & HSW_EXPONENT_MASK) << HSW_EXPONENT_OFFSET);

            if (csc_params->m_CSCCoeff[count] < 0)
                CSC_SETBIT(Binary, CSC_BIT_SHIFT(15));  /* Sign Bit */

            *(Coeff_binary + count) = Binary;
        }
    }

    return;
}

/* Assigns the Binary CSC coefficients into BSpec register format */
static void Convert_Coeff_ToBSpecFormat(unsigned int *CSCCoeff,
					unsigned short *Coeff_binary, int devid)
{
    short int RegIndex;
    short int CoeffIndex;

    for(RegIndex = 0, CoeffIndex = 0; RegIndex < CSC_MAX_COEFF_REG_COUNT;
                                   RegIndex += 2, CoeffIndex += 3) {
        if (IS_HASWELL(devid) || IS_BROADWELL(devid)) {
            CSCCoeff[RegIndex] = (*(Coeff_binary + CoeffIndex) << 16 |
                                            *(Coeff_binary + (CoeffIndex + 1)));
            CSCCoeff[RegIndex+1] = *(Coeff_binary + (CoeffIndex + 2)) << 16;
        } else if (IS_VALLEYVIEW(devid)) {
            CSCCoeff[RegIndex] = (*(Coeff_binary + CoeffIndex + 1) << 16 |
                                            *(Coeff_binary + CoeffIndex));
            CSCCoeff[RegIndex+1] = *(Coeff_binary + (CoeffIndex + 2));
        }
    }
    return;
}

int Calc_CSC_Param(struct CSCCoeff_Matrix *CSC_Matrix,
				struct csc_coeff *CSC_Coeff_t, int devid)
{
    struct drm_intel_csc_params input_csc_params;
    unsigned short Hsw_Vlv_CSC_Coeff[CSC_MAX_COEFF_COUNT];

    memcpy(input_csc_params.m_CSCCoeff, CSC_Matrix->CoeffMatrix,
					sizeof(float) * CSC_MAX_COEFF_COUNT);
    Convert_Coeff_ToBinary(&input_csc_params, Hsw_Vlv_CSC_Coeff, devid);
    Convert_Coeff_ToBSpecFormat(CSC_Coeff_t->csc_coeff,
						Hsw_Vlv_CSC_Coeff, devid);

    if (IS_HASWELL(devid) || IS_BROADWELL(devid)) {
        if (CSC_Matrix->param_valid & CSC_OFFSET_VALID_MASK)
            Convert_CSC_Offset_ToBSpecFormat(CSC_Matrix, CSC_Coeff_t);

        if (CSC_Matrix->param_valid & CSC_MODE_VALID_MASK) {
            if(CSC_Matrix->CSCMode == 0x1)
                CSC_Coeff_t->csc_mode = 0x2; /* CSC is before Gamma */
            else
                CSC_Coeff_t->csc_mode = 0;
        }
        CSC_Coeff_t->param_valid = CSC_Matrix->param_valid;
    }

    return 0;
}
