RWStructuredBuffer<uint> uav : register(u0, space0);
globallycoherent RWStructuredBuffer<uint> coherent_uav : register(u1, space0);
globallycoherent RWStructuredBuffer<uint> counter : register(u2, space0);
globallycoherent RWStructuredBuffer<uint> output : register(u3, space0);

[RootSignature("RootFlags(0), UAV(u0), UAV(u1), UAV(u2), UAV(u3)")]
[numthreads(1, 1, 1)] // Dispatched as (2, 1, 1)
void main(
	uint3 inGroupThreadID : SV_GroupThreadID,
	uint3 inGroupID : SV_GroupID,
	uint3 inDispatchThreadID : SV_DispatchThreadID,
	uint inGroupIndex : SV_GroupIndex)
{
	// On group 0
	if (inGroupID.x == 0)
	{
		// Group 0 writes 0 (and load it into L1)
		uav[0] = 0;
		coherent_uav[0] = 0;
	}

	// On group 1
	if (inGroupID.x == 1)
	{
		for (int i = 0; i < 100000; i++) // trivial heavy lifting to ensure group 1 writes after group 0
		{
			uint dummy;
			InterlockedAdd(counter[0], 1, dummy);
		}

		// Group 1 writes 1
		uav[0] = 1;
		coherent_uav[0] = 1;

		// Signal
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
			wait_counter++;

		// Group 0 reads
		output[0] = uav[0];				// = 0 without globallycoherent (stale L1)
		output[1] = coherent_uav[0];	// = 1 with globallycoherent
	}
}
