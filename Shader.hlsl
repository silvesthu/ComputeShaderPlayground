globallycoherent RWStructuredBuffer<uint> uav : register(u0, space0);
// RWStructuredBuffer<uint> uav : register(u0, space0); // cause TDR

[RootSignature("RootFlags(0), UAV(u0)")]
[numthreads(GROUP_SIZE, 1, 1)]
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	// Initialize
	uav[inDispatchThreadID.x] = 0;

	// Sync
	AllMemoryBarrierWithGroupSync();

	// Work per group
	if (inGroupID.x != 0)
 	{
 		for (int i = 0; i < 10000; i++)  // heavy-lifting
 			uav[inDispatchThreadID.x] = uav[inDispatchThreadID.x] + i;

		if (inGroupThreadID.x == 0) // first thread in group
		{
			uint dummy;
			InterlockedAdd(uav[1], 1, dummy); // group counter
		}
	}

 	// Sync
 	AllMemoryBarrierWithGroupSync();

	// Sync across groups
	if (inDispatchThreadID.x == 0) // first thread in dispatch
	{
		int wait_counter = 0;
		while (uav[1] < (DISPATCH_COUNT - 1))
		{
			wait_counter++;
		}
		uav[2] = wait_counter;
	}
}
