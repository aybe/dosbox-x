/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>

#define LoadMbs(off) (int8_t)(LoadMb(off))
#define LoadMws(off) (int16_t)(LoadMw(off))
#define LoadMds(off) (int32_t)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=((uint8_t)(val))
#define SaveRw(reg,val)	reg=((uint16_t)(val))
#define SaveRd(reg,val)	reg=((uint32_t)(val))

static INLINE int8_t Fetchbs() {
	return (int8_t)Fetchb();
}
static INLINE int16_t Fetchws() {
	return (int16_t)Fetchw();
}

static INLINE int32_t Fetchds() {
	return (int32_t)Fetchd();
}


#define RUNEXCEPTION() {										\
	PRE_EXCEPTION \
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

#define EXCEPTION(blah)										\
	{														\
		PRE_EXCEPTION \
		CPU_Exception(blah);								\
		continue;											\
	}

/* NTS: At first glance, this looks like code that will only fetch the delta for conditional jumps
 *      if the condition is true. Further examination shows that DOSBox's core has two different
 *      CS:IP variables, reg_ip and core.cseip which Fetchb() modifies. */
//TODO Could probably make all byte operands fast?
#define JumpCond16_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond16_w(COND) {						\
	const uint32_t adj=(uint32_t)Fetchws();						\
	SAVEIP;								\
	if (COND) reg_ip+=adj;						\
	continue;							\
}

#define JumpCond32_b(COND) {						\
	const uint32_t adj=(uint32_t)Fetchbs();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define JumpCond32_d(COND) {						\
	const uint32_t adj=(uint32_t)Fetchds();						\
	SAVEIP;								\
	if (COND) reg_eip+=adj;						\
	continue;							\
}

#define MoveCond16(COND) {							\
	GetRMrw;										\
	if (rm >= 0xc0 ) {GetEArw; if (COND) *rmrw=*earw;}\
	else {GetEAa; if (COND) *rmrw=LoadMw(eaa);}		\
}

#define MoveCond32(COND) {							\
	GetRMrd;										\
	if (rm >= 0xc0 ) {GetEArd; if (COND) *rmrd=*eard;}\
	else {GetEAa; if (COND) *rmrd=LoadMd(eaa);}		\
}

#if CPU_CORE >= CPU_ARCHTYPE_80386

/* Most SSE instructions require memory access aligned to the SSE data type, else
 * an exception occurs. */
static INLINE bool SSE_REQUIRE_ALIGNMENT(const PhysPt v) {
	return ((unsigned int)v & 15u) == 0; /* 16 bytes * 8 bits = 128 bits */
}

/* Throw GPF on SSE misalignment [https://c9x.me/x86/html/file_module_x86_id_180.html] */
/* NTS: This macro intended for use in normal core */
#define SSE_ALIGN_EXCEPTION() EXCEPTION(EXCEPTION_GP)

#define STEP(i) SSE_MULPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MULPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v *= s.v;
}

static INLINE void SSE_MULPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MULSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_ANDPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ANDPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw &= s.raw;
}

static INLINE void SSE_ANDPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_ANDNPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ANDNPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = (~d.raw) & s.raw;
}

static INLINE void SSE_ANDNPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_XORPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_XORPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw ^= s.raw;
}

static INLINE void SSE_XORPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_SQRTPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_SQRTPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = sqrtf(s.v);
}

static INLINE void SSE_SQRTPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_SQRTSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_MOVAPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVAPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVAPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_MOVUPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_MOVUPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw = s.raw;
}

static INLINE void SSE_MOVUPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_MOVSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

static INLINE void SSE_MOVHLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[1];
}

static INLINE void SSE_MOVLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[0] = s.u64[0];
}

////

static INLINE void SSE_UNPCKLPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u32[0] = d.u32[1] = s.u32[0];
	d.u32[2] = d.u32[3] = s.u32[1];
}

////

static INLINE void SSE_UNPCKHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u32[0] = d.u32[1] = s.u32[2];
	d.u32[2] = d.u32[3] = s.u32[3];
}

////

static INLINE void SSE_MOVLHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[1] = s.u64[0];
}

static INLINE void SSE_MOVHPS(XMM_Reg &d,const XMM_Reg &s) {
	d.u64[1] = s.u64[0];
}

////

static INLINE void SSE_CVTPI2PS_i(FPU_Reg_32 &d,const int32_t s) {
	d.v = (float)s;
}

static INLINE void SSE_CVTPI2PS(XMM_Reg &d,const MMX_reg &s) {
	SSE_CVTPI2PS_i(d.f32[0],s.sd.d0);
	SSE_CVTPI2PS_i(d.f32[1],s.sd.d1);
}

static INLINE void SSE_CVTSI2SS(XMM_Reg &d,const uint32_t s) {
	SSE_CVTPI2PS_i(d.f32[0],(int32_t)s);
}

////

static INLINE void SSE_CVTTPS2PI_i(int32_t &d,const FPU_Reg_32 &s) {
	if (s.v < -0x7FFFFFFF || s.v > 0x7FFFFFFF)
		d = (int32_t)0x80000000;
	else if (s.v > 0) // truncate towards zero
		d = (int32_t)floor(s.v);
	else // truncate towards zero
		d = -((int32_t)floor(-s.v));
}

static INLINE void SSE_CVTTPS2PI(MMX_reg &d,const XMM_Reg &s) {
	SSE_CVTTPS2PI_i(d.sd.d0,s.f32[0]);
	SSE_CVTTPS2PI_i(d.sd.d1,s.f32[1]);
}

static INLINE void SSE_CVTTSS2SI(uint32_t &d,const XMM_Reg &s) {
	SSE_CVTTPS2PI_i((int32_t&)d,s.f32[0]);
}

////

static INLINE void SSE_CVTPS2PI_i(int32_t &d,const FPU_Reg_32 &s) {
	if (s.v < -0x7FFFFFFF || s.v > 0x7FFFFFFF)
		d = (int32_t)0x80000000;
	else // based on rounding mode in MXCSR (TODO)
		d = (int32_t)s.v;
}

static INLINE void SSE_CVTPS2PI(MMX_reg &d,const XMM_Reg &s) {
	SSE_CVTPS2PI_i(d.sd.d0,s.f32[0]);
	SSE_CVTPS2PI_i(d.sd.d1,s.f32[1]);
}

static INLINE void SSE_CVTSS2SI(uint32_t &d,const XMM_Reg &s) {
	SSE_CVTPS2PI_i((int32_t&)d,s.f32[0]);
}

////

static INLINE void SSE_COMISS_common(const XMM_Reg &d,const XMM_Reg &s) {
	FillFlags();
	reg_flags &= ~(FLAG_CF|FLAG_PF|FLAG_AF|FLAG_ZF|FLAG_SF|FLAG_OF);

	if (isnan(d.f32[0].v) || isnan(s.f32[0].v))
		reg_flags |= FLAG_ZF|FLAG_PF|FLAG_CF; /* unordered compare */
	else if (d.f32[0].v == s.f32[0].v)
		reg_flags |= FLAG_ZF;
	else if (d.f32[0].v > s.f32[0].v)
		{ /* no change */ }
	else /* d < s */
		reg_flags |= FLAG_CF;
}

static INLINE void SSE_UCOMISS(const XMM_Reg &d,const XMM_Reg &s) {
	SSE_COMISS_common(d,s);
}

static INLINE void SSE_COMISS(const XMM_Reg &d,const XMM_Reg &s) {
	SSE_COMISS_common(d,s);
}

////

static INLINE void SSE_MOVMSKPS(uint32_t &d,const XMM_Reg &s) {
	/* take sign bits from floats, stick into low bits of register, zero extend */
	d =
		(s.f32[0].f.sign ? 0x1 : 0x0) +
		(s.f32[1].f.sign ? 0x2 : 0x0) +
		(s.f32[2].f.sign ? 0x4 : 0x0) +
		(s.f32[3].f.sign ? 0x8 : 0x0);

}

////

#define STEP(i) SSE_RSQRTPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_RSQRTPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = 1.0f / sqrtf(s.v);
}

static INLINE void SSE_RSQRTPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_RSQRTSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_RCPPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_RCPPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v = 1.0f / s.v;
}

static INLINE void SSE_RCPPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_RCPSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_ORPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ORPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.raw |= s.raw;
}

static INLINE void SSE_ORPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}
#undef STEP

////

#define STEP(i) SSE_ADDPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_ADDPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v += s.v;
}

static INLINE void SSE_ADDPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_ADDSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

////

#define STEP(i) SSE_SUBPS_i(d.f32[i],s.f32[i])
static INLINE void SSE_SUBPS_i(FPU_Reg_32 &d,const FPU_Reg_32 &s) {
	d.v -= s.v;
}

static INLINE void SSE_SUBPS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
	STEP(1);
	STEP(2);
	STEP(3);
}

static INLINE void SSE_SUBSS(XMM_Reg &d,const XMM_Reg &s) {
	STEP(0);
}
#undef STEP

#endif // 386+

#define SETcc(cc)							\
	{								\
		GetRM;							\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}			\
	}

void CPU_FXSAVE(PhysPt eaa);
void CPU_FXRSTOR(PhysPt eaa);
bool CPU_LDMXCSR(PhysPt eaa);
bool CPU_STMXCSR(PhysPt eaa);

#include "helpers.h"
#if CPU_CORE <= CPU_ARCHTYPE_8086
# include "table_ea_8086.h"
#else
# include "table_ea.h"
#endif
#include "../modrm.h"


