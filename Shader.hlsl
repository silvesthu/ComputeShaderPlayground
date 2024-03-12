// GROUP_SIZE
// DISPATCH_SIZE
// WAVE_SIZE

const static bool kUseWaveOperation = true;
_Static_assert(!kUseWaveOperation || GROUP_SIZE <= WAVE_SIZE, "Wave operation only work when wave size is 32");

RWStructuredBuffer<float4> uav : register(u0, space0);

namespace LDSHelper
{
	template <typename T> void LDSStore(T inValue, uint inGroupThreadID);
	template <typename T> T LDSLoad(uint inGroupThreadID);
	
	#define LDS_HELPER_DEFINE_TYPE(T)																					\
	groupshared T LDS_##T[GROUP_SIZE];																					\
	template<> void LDSStore<T>(T inValue, uint inGroupThreadID) { LDS_##T[inGroupThreadID] = inValue; }				\
	template<> T LDSLoad<T>(uint inGroupThreadID) { return LDS_##T[inGroupThreadID]; }									\
	
	LDS_HELPER_DEFINE_TYPE(float)
	LDS_HELPER_DEFINE_TYPE(float2)
	LDS_HELPER_DEFINE_TYPE(float3)
	LDS_HELPER_DEFINE_TYPE(float4)

	LDS_HELPER_DEFINE_TYPE(int)
	LDS_HELPER_DEFINE_TYPE(int2)
	LDS_HELPER_DEFINE_TYPE(int3)
	LDS_HELPER_DEFINE_TYPE(int4)

	LDS_HELPER_DEFINE_TYPE(uint)
	LDS_HELPER_DEFINE_TYPE(uint2)
	LDS_HELPER_DEFINE_TYPE(uint3)
	LDS_HELPER_DEFINE_TYPE(uint4)
}

#define WAVE_REDUCTION_TEMPLATE(Name)																					\
template <typename T> T WaveActive##Name##LDS(T expr, uint inGroupThreadID)												\
{																														\
	using namespace LDSHelper;																							\
																														\
	LDSStore(expr, inGroupThreadID);																					\
	GroupMemoryBarrierWithGroupSync();																					\
																														\
	for (uint stride = GROUP_SIZE / 2; stride > 0; stride /= 2)															\
    {																													\
        if (inGroupThreadID < stride)																					\
            WAVE_ACTIVE_ACCUMULATION																					\
																														\
        GroupMemoryBarrierWithGroupSync();																				\
    }																													\
																														\
	return WAVE_ACTIVE_FINALIZATION;																					\
}																														\
																														\
template <typename T>																									\
T WaveActive##Name##Unified(T expr, uint inGroupThreadID)																\
{																														\
	if (kUseWaveOperation)																								\
		return WaveActive##Name(expr);																					\
	else																												\
		return WaveActive##Name##LDS(expr, inGroupThreadID);															\
}																														\

#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(LDSLoad<T>(inGroupThreadID) + LDSLoad<T>(inGroupThreadID + stride), inGroupThreadID);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Sum)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(LDSLoad<T>(inGroupThreadID) * LDSLoad<T>(inGroupThreadID + stride), inGroupThreadID);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Product)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(min(LDSLoad<T>(inGroupThreadID), LDSLoad<T>(inGroupThreadID + stride)), inGroupThreadID);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Min)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(max(LDSLoad<T>(inGroupThreadID), LDSLoad<T>(inGroupThreadID + stride)), inGroupThreadID);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Max)

[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(GROUP_SIZE, 1, 1)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	uav[inDispatchThreadID.x] = 0;
	
	uav[inDispatchThreadID.x] = WaveGetLaneIndex();
	uav[inDispatchThreadID.x] = WaveActiveSumUnified(WaveGetLaneIndex(), inGroupThreadID.x);
}
