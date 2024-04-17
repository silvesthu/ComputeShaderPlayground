#pragma once

#if !defined(THREAD_GROUP_SIZE) || !defined(WAVE_LANE_COUNT_MIN)
_Static_assert(false,											"#define THREAD_GROUP_SIZE and WAVE_LANE_COUNT_MIN before use");
#endif // !defined(THREAD_GROUP_SIZE) || !defined(WAVE_LANE_COUNT_MIN)

#define WAVE_OPERATION_UNIFIED_USE_NATIVE						(THREAD_GROUP_SIZE <= WAVE_LANE_COUNT_MIN)

#ifdef WAVE_OPERATION_UNIFIED_FORCE_NATIVE
_Static_assert(THREAD_GROUP_SIZE <= WAVE_LANE_COUNT_MIN,		"Native wave operation only works if THREAD_GROUP_SIZE <= WAVE_LANE_COUNT_MIN");
#endif // WAVE_OPERATION_UNIFIED_FORCE_NATIVE

#ifdef WAVE_OPERATION_UNIFIED_FORCE_LDS
#undef WAVE_OPERATION_UNIFIED_USE_NATIVE
#define WAVE_OPERATION_UNIFIED_USE_NATIVE false
#endif // WAVE_OPERATION_UNIFIED_FORCE_LDS

#if defined(WAVE_OPERATION_UNIFIED_FORCE_NATIVE) && defined(WAVE_OPERATION_UNIFIED_FORCE_LDS)
_Static_assert(false,											"WAVE_OPERATION_UNIFIED_FORCE_NATIVE and WAVE_OPERATION_UNIFIED_FORCE_LDS can not be used together");
#endif // defined(WAVE_OPERATION_UNIFIED_FORCE_NATIVE) && defined(WAVE_OPERATION_UNIFIED_FORCE_LDS)

namespace LDSHelper
{
	template <typename T> void LDSStore(T inValue, uint inGroupIndex);
	template <typename T> T LDSLoad(uint inGroupIndex);

#define LDS_HELPER_DEFINE_TYPE(T)																						\
	groupshared T LDS_##T[THREAD_GROUP_SIZE];																			\
	template<> void LDSStore<T>(T inValue, uint inGroupIndex)	{ LDS_##T[inGroupIndex] = inValue; }					\
	template<> T LDSLoad<T>(uint inGroupIndex)					{ return LDS_##T[inGroupIndex]; }						\

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

#undef LDS_HELPER_DEFINE_TYPE
}

#define WAVE_REDUCTION_TEMPLATE(Name)																					\
template <typename T> T WaveActive##Name##LDS(T expr, uint inGroupIndex)												\
{																														\
	using namespace LDSHelper;																							\
																														\
	LDSStore(expr, inGroupIndex);																						\
	GroupMemoryBarrierWithGroupSync();																					\
																														\
	for (uint stride = THREAD_GROUP_SIZE / 2; stride > 0; stride /= 2)													\
    {																													\
        if (inGroupIndex < stride)																						\
            WAVE_ACTIVE_ACCUMULATION																					\
																														\
        GroupMemoryBarrierWithGroupSync();																				\
    }																													\
																														\
	return WAVE_ACTIVE_FINALIZATION;																					\
}																														\
																														\
template <typename T>																									\
T WaveActive##Name##Unified(T expr, uint inGroupIndex)																	\
{																														\
	if (WAVE_OPERATION_UNIFIED_USE_NATIVE)																				\
		return WaveActive##Name(expr);																					\
	else																												\
		return WaveActive##Name##LDS(expr, inGroupIndex);																\
}																														\

#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(LDSLoad<T>(inGroupIndex) + LDSLoad<T>(inGroupIndex + stride), inGroupIndex);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Sum)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(LDSLoad<T>(inGroupIndex) * LDSLoad<T>(inGroupIndex + stride), inGroupIndex);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Product)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(min(LDSLoad<T>(inGroupIndex), LDSLoad<T>(inGroupIndex + stride)), inGroupIndex);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Min)
#undef WAVE_ACTIVE_ACCUMULATION
#undef WAVE_ACTIVE_FINALIZATION
#define WAVE_ACTIVE_ACCUMULATION		LDSStore<T>(max(LDSLoad<T>(inGroupIndex), LDSLoad<T>(inGroupIndex + stride)), inGroupIndex);
#define WAVE_ACTIVE_FINALIZATION		LDSLoad<T>(0);
WAVE_REDUCTION_TEMPLATE(Max)
