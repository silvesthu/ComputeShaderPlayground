#pragma once

// Swizzle Threads within a ThreadGroup

// 1. https://github.com/NVIDIAGameWorks/rtxdi-runtime/blob/5ee2a55fedf865b0a19409ae8d2b6539b937d797/include/rtxdi/RtxdiMath.hlsli#L61

// "Explodes" an integer, i.e. inserts a 0 between each bit.  Takes inputs up to 16 bit wide.
//      For example, 0b11111111 -> 0b1010101010101010
uint RTXDI_IntegerExplode(uint x)
{
	x = (x | (x << 8)) & 0x00FF00FF;
	x = (x | (x << 4)) & 0x0F0F0F0F;
	x = (x | (x << 2)) & 0x33333333;
	x = (x | (x << 1)) & 0x55555555;
	return x;
}

// Reverse of RTXDI_IntegerExplode, i.e. takes every other bit in the integer and compresses
// those bits into a dense bit firld. Takes 32-bit inputs, produces 16-bit outputs.
//    For example, 0b'abcdefgh' -> 0b'0000bdfh'
uint RTXDI_IntegerCompact(uint x)
{
	x = (x & 0x11111111) | ((x & 0x44444444) >> 1);
	x = (x & 0x03030303) | ((x & 0x30303030) >> 2);
	x = (x & 0x000F000F) | ((x & 0x0F000F00) >> 4);
	x = (x & 0x000000FF) | ((x & 0x00FF0000) >> 8);
	return x;
}

uint RTXDI_ZCurveToLinearIndex(uint2 xy)
{
	return RTXDI_IntegerExplode(xy[0]) | (RTXDI_IntegerExplode(xy[1]) << 1);
}

// Converts a linear to a 2D position following a Z-curve pattern.
uint2 RTXDI_LinearIndexToZCurve(uint index)
{
	return uint2(
		RTXDI_IntegerCompact(index),
		RTXDI_IntegerCompact(index >> 1));
}

// 2. https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/sdk/include/FidelityFX/gpu/ffx_core_gpu_common.h#L2711

typedef uint32_t    FfxUInt32;
typedef uint32_t2   FfxUInt32x2;

FfxUInt32 ffxBitfieldExtract(FfxUInt32 src, FfxUInt32 off, FfxUInt32 bits)
{
	FfxUInt32 mask = (1u << bits) - 1;
	return (src >> off) & mask;
}

FfxUInt32 ffxBitfieldInsert(FfxUInt32 src, FfxUInt32 ins, FfxUInt32 mask)
{
	return (ins & mask) | (src & (~mask));
}

FfxUInt32 ffxBitfieldInsertMask(FfxUInt32 src, FfxUInt32 ins, FfxUInt32 bits)
{
	FfxUInt32 mask = (1u << bits) - 1;
	return (ins & mask) | (src & (~mask));
}

/// A remapping of 64x1 to 8x8 imposing rotated 2x2 pixel quads in quad linear.
///
/// Remap illustration:
///
///     543210
///     ~~~~~~
///     ..xxx.
///     yy...y
/// 
/// @param [in] a       The input 1D coordinates to remap.
///
/// @returns
/// The remapped 2D coordinates.
///
/// @ingroup GPUCore
FfxUInt32x2 ffxRemapForQuad(FfxUInt32 a)
{
	return FfxUInt32x2(ffxBitfieldExtract(a, 1u, 3u), ffxBitfieldInsertMask(ffxBitfieldExtract(a, 3u, 3u), a, 1u));
}

/// A helper function performing a remap 64x1 to 8x8 remapping which is necessary for 2D wave reductions.
///
/// The 64-wide lane indices to 8x8 remapping is performed as follows:
/// 
///     00 01 08 09 10 11 18 19
///     02 03 0a 0b 12 13 1a 1b
///     04 05 0c 0d 14 15 1c 1d
///     06 07 0e 0f 16 17 1e 1f
///     20 21 28 29 30 31 38 39
///     22 23 2a 2b 32 33 3a 3b
///     24 25 2c 2d 34 35 3c 3d
///     26 27 2e 2f 36 37 3e 3f
///
/// @param [in] a       The input 1D coordinate to remap.
/// 
/// @returns
/// The remapped 2D coordinates.
/// 
/// @ingroup GPUCore
FfxUInt32x2 ffxRemapForWaveReduction(FfxUInt32 a)
{
	return FfxUInt32x2(ffxBitfieldInsertMask(ffxBitfieldExtract(a, 2u, 3u), a, 1u), ffxBitfieldInsertMask(ffxBitfieldExtract(a, 3u, 3u), ffxBitfieldExtract(a, 1u, 2u), 2u));
}
