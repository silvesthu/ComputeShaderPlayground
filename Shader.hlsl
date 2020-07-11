// globallycoherent
	RWStructuredBuffer<uint> uav : register(u0, space0);
globallycoherent RWStructuredBuffer<uint> counter : register(u1, space0);

[RootSignature("RootFlags(0), UAV(u0), UAV(u1)")]
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

	// On group 0
	if (inGroupID.x == 0)
	{
		// map L1
		uav[inDispatchThreadID.x] = inDispatchThreadID.x;
	}

	// On group 1
	if (inGroupID.x == 1)
	{
		for (int i = 0; i < 100000; i++) // heavy lifting
		{
			uint dummy;
			InterlockedAdd(counter[1], 1, dummy);
		}

		// update uav with range used by group 0
		uav[inDispatchThreadID.x - GROUP_SIZE] = inDispatchThreadID.x;
		counter[0] = 1;
	}

	// Sync
	AllMemoryBarrierWithGroupSync();

	// On group 0
	if (inGroupID.x == 0)
	{
		// Wait group 1
		uint wait_counter = 0;
		while (counter[0] != 1)
		{
			wait_counter++;
		}

		// update uav (from stale L1 if w/o globallycoherent)
		uav[inDispatchThreadID.x] = uav[inDispatchThreadID.x];
		counter[1] = wait_counter;
	}
}
