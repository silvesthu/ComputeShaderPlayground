
// Inputs:
//	THREAD_GROUP_SIZE_X
//	THREAD_GROUP_SIZE_Y
//	THREAD_GROUP_SIZE_Z
//	THREAD_GROUP_SIZE
//	DISPATCH_SIZE
//	WAVE_LANE_COUNT_MIN

// #define WAVE_OPERATION_UNIFIED_FORCE_NATIVE
// #define WAVE_OPERATION_UNIFIED_FORCE_LDS
#include "WaveOperationUnified.h"

RWStructuredBuffer<float4> uav : register(u0, space0);
[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(THREAD_GROUP_SIZE_X, THREAD_GROUP_SIZE_Y, THREAD_GROUP_SIZE_Z)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	uav[inGroupIndex] = 0;
	
	uav[inGroupIndex] = float4(
					WaveActiveSumUnified(WaveGetLaneIndex(), inGroupIndex),
					inGroupIndex,
					0,
					WAVE_OPERATION_UNIFIED_USE_NATIVE ? 1 : 0);
}
