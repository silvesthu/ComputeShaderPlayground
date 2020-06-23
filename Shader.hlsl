RWStructuredBuffer<float> uav : register(u0, space0);

[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(32, 1, 1)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	int stride = 4096 / 32;
	int index = inDispatchThreadID.x * stride;
	for (int i = 0; i < stride; i++)
	{
		uav[index] = index;
		index++;
	}
}
