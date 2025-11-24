
// Inputs:
//	THREAD_GROUP_SIZE_X
//	THREAD_GROUP_SIZE_Y
//	THREAD_GROUP_SIZE_Z
//	THREAD_GROUP_SIZE
//	DISPATCH_SIZE_X
//	DISPATCH_SIZE_Y
//	DISPATCH_SIZE_Z
//	DISPATCH_SIZE
//	WAVE_LANE_COUNT_MIN

// #define WAVE_OPERATION_UNIFIED_FORCE_NATIVE
// #define WAVE_OPERATION_UNIFIED_FORCE_LDS
#include "WaveOperationUnified.h"

#include "ThreadSwizzle.h"
#include "ThreadGroupSwizzle.h"

RWStructuredBuffer<float4> uav : register(u0, space0);
[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	uint dispatch_thread_index = inGroupIndex + (inGroupID.x + inGroupID.y * DISPATCH_SIZE_X) * THREAD_GROUP_SIZE;
	uav[dispatch_thread_index] = 0;
	
	// uav[inGroupIndex] = float4(WaveActiveSumUnified(inGroupIndex, inGroupIndex), inGroupIndex, 0, WAVE_OPERATION_UNIFIED_USE_NATIVE ? 1 : 0);

	uint2 group_id = inGroupID.xy;
	uint2 group_thread_id = inGroupThreadID.xy;
	uint2 dispatch_thread_id = inDispatchThreadID.xy;
	
	// Draft
	if (MODE == 0)
	{
		uint a = inGroupIndex;
		group_thread_id = ffxRemapForWaveReduction(inGroupIndex);

		uav[dispatch_thread_index] = asfloat(uint4(
			inGroupIndex,
			group_thread_id,
			WaveGetLaneIndex()));
	}

	// ThreadSwizzle
	if (MODE == 1)
	{
		{
			// [NOTE] RTXDI_LinearIndexToZCurve can also be used to swizzle ThreadGroup, although it seems to require size to be power of 2
			// group_id = RTXDI_LinearIndexToZCurve(group_id.x + group_id.y * DISPATCH_SIZE_X);
		}

		// Swizzle Threads within a ThreadGroup 
		{
			// Unswizzled. Row first
			// Pixel-to-Pixel Distance Sum = 105.50

			// Z-Curve:
			//   0, 1
			//   2, 3
			// Reverse N-Curve:
			//   0, 2
			//   1, 3

			// Z-Curve for all levels, +28 instructions in DXIL
			// Pixel-to-Pixel Distance Sum = 91.99
			// group_thread_id = RTXDI_LinearIndexToZCurve(inGroupIndex);

			// Z-Curve for 2x2, Reversed N-Curve for 4x4, Z-Curve for 8x8, +7 instructions in DXIL. SPD use this.
			// Pixel-to-Pixel Distance Sum = 91.99
			// group_thread_id = ffxRemapForWaveReduction(inGroupIndex);

			// Reversed N-Curve for 2x2, +4 instructions in DXIL
			// Pixel-to-Pixel Distance Sum = 92.81
			// group_thread_id = ffxRemapForQuad(inGroupIndex);
			
			// ** As Unified Pattern **

			// Pattern: yxyxyx, +11 instructions in DXIL. RTXDI_LinearIndexToZCurve equivalent
			// Pixel-to-Pixel Distance Sum = 91.99
			uint group_x = ((inGroupIndex >> 4) & ((1 << 1) - 1)) * 4 + ((inGroupIndex >> 2) & ((1 << 1) - 1)) * 2 + ((inGroupIndex >> 0) & ((1 << 1) - 1));
			uint group_y = ((inGroupIndex >> 5) & ((1 << 1) - 1)) * 4 + ((inGroupIndex >> 3) & ((1 << 1) - 1)) * 2 + ((inGroupIndex >> 1) & ((1 << 1) - 1));
			group_thread_id = uint2(group_x, group_y);

			// Pattern: yxxyyx, +7 instructions in DXIL. ffxRemapForWaveReduction equivalent
			// Pixel-to-Pixel Distance Sum = 91.99
			// uint group_x = ((inGroupIndex >> 3) & ((1 << 2) - 1)) * 2 + ((inGroupIndex >> 0) & ((1 << 1) - 1));
			// uint group_y = ((inGroupIndex >> 5) & ((1 << 1) - 1)) * 4 + ((inGroupIndex >> 1) & ((1 << 2) - 1));
			// group_thread_id = uint2(group_x, group_y);

			// Pattern: yyxxxy, +4 instructions in DXIL. ffxRemapForQuad equivalent
			// Pixel-to-Pixel Distance Sum = 92.81
			// uint group_x = ((inGroupIndex >> 0) & ((1 << 0) - 1)) * 2 + ((inGroupIndex >> 1) & ((1 << 3) - 1));
			// uint group_y = ((inGroupIndex >> 4) & ((1 << 2) - 1)) * 2 + ((inGroupIndex >> 0) & ((1 << 1) - 1));
			// group_thread_id = uint2(group_x, group_y);

			// Pattern: yyxxyx, +6 instructions in DXIL. Z-Curve for 2x2
			// Pixel-to-Pixel Distance Sum = 92.81
			// uint group_x = ((inGroupIndex >> 2) & ((1 << 2) - 1)) * 2 + ((inGroupIndex >> 0) & ((1 << 1) - 1));
			// uint group_y = ((inGroupIndex >> 4) & ((1 << 2) - 1)) * 2 + ((inGroupIndex >> 1) & ((1 << 1) - 1));
			// group_thread_id = uint2(group_x, group_y);
			
			dispatch_thread_id = group_thread_id + group_id.xy * uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y);
		}

		// Swizzle ThreadGroups within a Dispatch
		{
			// dispatch_thread_id = ThreadGroupTilingX(uint2(DISPATCH_SIZE_X, DISPATCH_SIZE_Y), uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y), 2 /* new row after N group */, group_thread_id.xy, group_id.xy);
		}

		uav[dispatch_thread_index] = float4(dispatch_thread_id, 0, 0);
	}

	// WaveMatch
	if (MODE == 2)
	{
		uint data_array[32] = {
			1, 0, 1, 0, 1, 0, 1, 0,
			2, 3, 2, 3, 2, 3, 2, 3,
			1, 0, 1, 0, 1, 0, 1, 0,
			2, 3, 2, 3, 2, 3, 2, 3,
		};
		uint data = data_array[inGroupIndex];
		uint mask = WaveMatch(data).x; // WaveMatch returns uint4
		uav[dispatch_thread_index] = asfloat(uint4(data, 0, 0, mask));
	}
}
