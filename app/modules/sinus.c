/*
 *  fastSinCos16, a fast 16 bit sine and cosine approximator.
 *  Copyright (C) 2011 Lauri Viitanen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see http://www.gnu.org/licenses/ .
 */

/* Average and maximum relative and absolute deviations of this
   algorithm from libc's sin() and cos() values:

One multiplication, no extra accuracy:
fastSinCos16            ****************************************
        Max rel         Avg rel         Max abs         Avg abs
SINE  : 1.546518        0.085854        0.056234        0.030120
COSINE: 1.546518        0.085852        0.056173        0.030118

One multiplication with extra accuracy:
fastSinCos16            ****************************************
        Max rel         Avg rel         Max abs         Avg abs
SINE  : 0.273291        0.085637        0.056112        0.030089
COSINE: 0.273291        0.085635        0.056112        0.030088

Two multiplications, no extra accuracy:
fastSinCos16            ****************************************
        Max rel         Avg rel         Max abs         Avg abs
SINE  : 1.546518        0.040205        0.023725        0.013693
COSINE: 0.909888        0.040191        0.023664        0.013693

Two multiplications with extra accuracy:
fastSinCos16            ****************************************
        Max rel         Avg rel         Max abs         Avg abs
SINE  : 0.363371        0.040009        0.023603        0.013693
COSINE: 0.363371        0.040008        0.023603        0.013693 */

#define DOUBLE_ACCURACY
#define EXTRA_ACCURACY

/* @fn  	fastSinCos16
 * @brief	A fast 16 bit sine and cosine approximator.
 *
 * This is a direct 16 bit branch free fixed point implementation of Olli
 * Niemitalo's simultaneous parabolic approximation of sin and cos. A detailed
 * description of the algoritm can be seen (at least up to Jan 2011) in
 * http://www.dspguru.com/dsp/tricks/parabolic-approximation-of-sin-and-cos .
 *
 * The number presentation used in results is as follows:
 *     bit 15: sign
 *     bit 14: integer part
 *     13 - 0: decimal part
 *
 * @param	ux      Input angle: 0x0000 = 0, 0xffff ~= pi/2
 * @param	sin_out Address that will contain sin(x).
 * @param	cos_out Address that will contain cos(x).
 */
void fastSinCos16(unsigned short ux, short* sin_out, short* cos_out)
{
	short cos_b, sin_b;
	short cos_axxc, sin_axxc;

	short halfMask = (short)ux >> 15;
	short quarterMask = -((ux >> 14) & 1);

	ux &= 0x3fff; /* Convert to fixed point range 1 ... 0. */
	short x = (short)ux;

	x -= 0x1fff; /* Convert to range -0.5 ... 0.5. */
	sin_b = x;
	cos_b = -x;

	int lx = x - 0x2; /* A tiny offset adjustment... */
	lx *= lx;
	lx = (lx >> 14) & 0xffff;

	#ifdef DOUBLE_ACCURACY
		/* A = 2 - 4sin(pi/4) = 2 - 4*0.70710678118654752440 */
		lx *= 0x8000 - 4*0x2d41; /* x = Ax^2 */
		lx >>= 14;

		x = (short)(lx & 0xffff);
		cos_axxc = x + 0x2d41; /* = Ax^2 + C */
	#else
		/* A = 3/4 = 0.75 */
		lx = -lx; /* x = -x^2 */

		x = (short)(lx & 0xffff);
		cos_axxc = x + 0x3000; /* = Ax^2 + C */
	#endif

	sin_axxc = cos_axxc;

	cos_axxc ^= quarterMask;
	sin_b ^= quarterMask;

	#ifdef EXTRA_ACCURACY
		quarterMask &= 1;
		cos_b += quarterMask;
		sin_b += quarterMask;
	#endif

	cos_axxc ^= halfMask;
	cos_b ^= halfMask;
	sin_axxc ^= halfMask;
	sin_b ^= halfMask;

	#ifdef EXTRA_ACCURACY
		halfMask &= 2;
		cos_b += halfMask;
		sin_b += halfMask;
	#endif

	cos_axxc += cos_b; /* sin(x) ~= (2 - 4C)x^2 + Bx + C */
	sin_axxc += sin_b; /* cos(x) ~= (2 - 4C)x^2 - Bx + C */

	*cos_out = cos_axxc;
	*sin_out = sin_axxc;
}

int SIcosH_cos_H(int H, int S, int I)
{
  short sinR, cosH, cos60H;
  unsigned short ux1 = (H<<16)/360;
  unsigned short ux2 = ((60-H)<<16)/360;

  fastSinCos16(ux1, &sinR, &cosH);
  fastSinCos16(ux2, &sinR, &cos60H);	

  return (I*((S*cosH)/cos60H))>>10;
}
