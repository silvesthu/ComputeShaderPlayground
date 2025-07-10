
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

	// ThreadSwizzle
	if (false)
	{
		{
			// [NOTE] RTXDI_LinearIndexToZCurve can also be used to swizzle ThreadGroup, although it seems to require size to be power of 2
			// group_id = RTXDI_LinearIndexToZCurve(group_id.x + group_id.y * DISPATCH_SIZE_X);
		}
		// Swizzle Threads within a ThreadGroup 
		{
			// Tile in row = Z-Curve
			// Tile in column = Reverse N-Curve
						
			// group_thread_id = RTXDI_LinearIndexToZCurve(inGroupIndex);		// Z-Curve for all, 30 instructions in DXIL
			group_thread_id = ffxRemapForWaveReduction(inGroupIndex);		// Z-Curve for 2x2, Reversed N-Curve for 4x4, Z-Curve for 8x8, 9 instructions in DXIL
			// group_thread_id = ffxRemapForQuad(inGroupIndex);					// Reversed N-Curve for 2x2, sequential for left, 6 instructions in DXIL

			dispatch_thread_id = group_thread_id + group_id.xy * uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y);
		}
		// Swizzle ThreadGroups within a Dispatch
		{
			// dispatch_thread_id = ThreadGroupTilingX(uint2(DISPATCH_SIZE_X, DISPATCH_SIZE_Y), uint2(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y), 2 /* new row after N group */, group_thread_id.xy, group_id.xy);
		}
		uav[dispatch_thread_index] = float4(dispatch_thread_id, 0, 0);
	}

	// WaveMatch
	if (true)
	{
		uint data_array[32] = {
			1, 0, 1, 0, 1, 0, 1, 0,
			2, 3, 2, 3, 2, 3, 2, 3,
			1, 0, 1, 0, 1, 0, 1, 0,
			2, 3, 2, 3, 2, 3, 2, 3,
		};
		uint data = data_array[inGroupIndex];
		uint mask = WaveMatch(data);
		uav[dispatch_thread_index] = float4(data, 0, 0, asfloat(mask));
	}
}
