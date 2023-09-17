RWStructuredBuffer<float4> uav : register(u0, space0);

// https://www.jeremyong.com/graphics/2023/09/05/f32-interlocked-min-max-hlsl/
// http://stereopsis.com/radix.html

// Check isnan(value) before use.
uint order_preserving_float_map(float value)
{
    // For negative values, the mask becomes 0xffffffff.
    // For positive values, the mask becomes 0x80000000.
    uint uvalue = asuint(value);
    uint mask = -int(uvalue >> 31) | 0x80000000;
    return uvalue ^ mask;
}

float inverse_order_preserving_float_map(uint value)
{
    // If the msb is set, the mask becomes 0x80000000.
    // If the msb is unset, the mask becomes 0xffffffff.
    uint mask = ((value >> 31) - 1) | 0x80000000;
    return asfloat(value ^ mask);
}

groupshared uint lds_min;
groupshared uint lds_max;

[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(32, 1, 1)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	uav[inDispatchThreadID.x] = 0;

    if (inDispatchThreadID.x == 0)
    {
        lds_min = order_preserving_float_map(+1.#INF);
        lds_max = order_preserving_float_map(-1.#INF);
    }

    GroupMemoryBarrierWithGroupSync();

    float values[32] = (float[32])0;
    values[0] = -1.0;
    values[1] = +1.0;
    values[2] = -1.1;
    values[3] = +1.1;
    // values[4] = -1.#INF;     // Infinities are good
    // values[5] = +1.#INF;
    // values[6] = 0. / 0.;     // NaN needs extra handling

    float fvalue = values[inDispatchThreadID.x];
    uint uvalue = order_preserving_float_map(fvalue);

    uint original_min;
    InterlockedMin(lds_min, uvalue, original_min);

    uint original_max;
    InterlockedMax(lds_max, uvalue, original_max);

    GroupMemoryBarrierWithGroupSync();

    if (inDispatchThreadID.x == 0)
    {
        uav[0] = inverse_order_preserving_float_map(lds_min);
        uav[1] = inverse_order_preserving_float_map(lds_max);
    }

    // uav[inDispatchThreadID.x] = fvalue; // show input values
}
