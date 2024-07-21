#include <string>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12shader.h>

#include <wrl.h>
using namespace Microsoft::WRL;

extern "C" { __declspec(dllexport) extern const UINT			D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char8_t*		D3D12SDKPath = u8".\\D3D12\\"; }

int main()
{
	const uint32_t kThreadGroupSizeX			= 8;
	const uint32_t kThreadGroupSizeY			= 8;
	const uint32_t kThreadGroupSizeZ			= 1;
	const uint32_t kThreadGroupSize				= kThreadGroupSizeX * kThreadGroupSizeY * kThreadGroupSizeZ;

	const uint32_t kDispatchSizeX				= 4;
	const uint32_t kDispatchSizeY				= 4;
	const uint32_t kDispatchSizeZ				= 1;
	const uint32_t kDispatchSize				= kDispatchSizeX * kDispatchSizeY * kDispatchSizeZ;

	const uint32_t kTotalSizeX					= kThreadGroupSizeX * kDispatchSizeX;
	const uint32_t kTotalSizeY					= kThreadGroupSizeY * kDispatchSizeY;
	const uint32_t kTotalSizeZ					= kThreadGroupSizeZ * kDispatchSizeZ;
	const uint32_t kTotalSize					= kThreadGroupSize * kDispatchSize;

	const bool kPrintDisassembly				= true;
	const bool kPrintThreadSwizzle				= true;

	//////////////////////////////////////////////////////////////////////////

	ComPtr<ID3D12Debug> d3d12_debug;
	D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12_debug));
	d3d12_debug->EnableDebugLayer();

	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

	ComPtr<IDXGIAdapter1> adapter;
	factory->EnumAdapters1(0, &adapter);

	DXGI_ADAPTER_DESC1 adapter_desc;
	adapter->GetDesc1(&adapter_desc);

	ComPtr<ID3D12Device2> device;
	D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

	D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1 = {};
	device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS1));

	//////////////////////////////////////////////////////////////////////////

	ComPtr<IDxcBlob> shader_blob;
	{
		ComPtr<IDxcLibrary> library;
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));

		ComPtr<IDxcCompiler> compiler;
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

		ComPtr<IDxcUtils> utils;
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

		uint32_t code_page = CP_UTF8;
		ComPtr<IDxcBlobEncoding> source_blob;
		library->CreateBlobFromFile(L"Shader.hlsl", &code_page, &source_blob);

		ComPtr<IDxcOperationResult> result;
		LPCWSTR arguments[] =
		{
			L"-O3",
			L"-HV 2021",
			// L"-Zi",
		};

		std::wstring thread_group_size_x_str	= std::to_wstring(kThreadGroupSizeX);
		std::wstring thread_group_size_y_str	= std::to_wstring(kThreadGroupSizeY);
		std::wstring thread_group_size_z_str	= std::to_wstring(kThreadGroupSizeZ);
		std::wstring thread_group_size_str		= std::to_wstring(kThreadGroupSize);
		std::wstring dispatch_size_x_str		= std::to_wstring(kDispatchSizeX);
		std::wstring dispatch_size_y_str		= std::to_wstring(kDispatchSizeY);
		std::wstring dispatch_size_z_str		= std::to_wstring(kDispatchSizeZ);
		std::wstring dispatch_size_str			= std::to_wstring(kDispatchSize);
		std::wstring wave_lane_count_min		= std::to_wstring(options1.WaveLaneCountMin);
		std::wstring wave_lane_count_max		= std::to_wstring(options1.WaveLaneCountMax);
		std::wstring total_lane_count			= std::to_wstring(options1.TotalLaneCount);
		DxcDefine defines[] =
		{
			{ L"THREAD_GROUP_SIZE_X",			thread_group_size_x_str.c_str() },
			{ L"THREAD_GROUP_SIZE_Y",			thread_group_size_y_str.c_str() },
			{ L"THREAD_GROUP_SIZE_Z",			thread_group_size_z_str.c_str() },
			{ L"THREAD_GROUP_SIZE",				thread_group_size_str.c_str() },
			{ L"DISPATCH_SIZE_X",				dispatch_size_x_str.c_str() },
			{ L"DISPATCH_SIZE_Y",				dispatch_size_y_str.c_str() },
			{ L"DISPATCH_SIZE_Z",				dispatch_size_z_str.c_str() },
			{ L"DISPATCH_SIZE",					dispatch_size_str.c_str() },
			{ L"WAVE_LANE_COUNT_MIN",			wave_lane_count_min.c_str() },
			{ L"WAVE_LANE_COUNT_MAX",			wave_lane_count_max.c_str() },
			{ L"TOTAL_LANE_COUNT",				total_lane_count.c_str() },
		};
		for (auto&& define : defines)
			printf("%ls = %ls\n", define.Name, define.Value);
		printf("\n");
		ComPtr<IDxcIncludeHandler> dxc_include_handler;
		utils->CreateDefaultIncludeHandler(&dxc_include_handler);
		HRESULT hr = compiler->Compile(source_blob.Get(), L"Shader.hlsl", L"main", L"cs_6_7", arguments, _countof(arguments), defines, _countof(defines), dxc_include_handler.Get(), &result);
		if (SUCCEEDED(hr))
			result->GetStatus(&hr);
		bool compile_succeed = SUCCEEDED(hr);
		ComPtr<IDxcBlobEncoding> error_blob;
		if (SUCCEEDED(result->GetErrorBuffer(&error_blob)) && error_blob)
		{
			printf("Shader compile %s\n", compile_succeed ? "succeed" : "failed");
			std::string message((const char*)error_blob->GetBufferPointer(), error_blob->GetBufferSize());
			printf("%s", message.c_str());
			printf("\n");
			if (!compile_succeed)
				return 0;
		}

		result->GetResult(&shader_blob);

		DxcBuffer dxc_buffer { .Ptr = shader_blob->GetBufferPointer(), .Size = shader_blob->GetBufferSize(), .Encoding = DXC_CP_ACP };
		ComPtr<ID3D12ShaderReflection> shader_reflection;
		utils->CreateReflection(&dxc_buffer, IID_PPV_ARGS(&shader_reflection));
		UINT64 shader_required_flags = shader_reflection->GetRequiresFlags();
		printf("D3D_SHADER_REQUIRES_WAVE_OPS = %d\n",			(shader_required_flags & D3D_SHADER_REQUIRES_WAVE_OPS) ? 1 : 0);
		printf("D3D_SHADER_REQUIRES_DOUBLES = %d\n",			(shader_required_flags & D3D_SHADER_REQUIRES_DOUBLES) ? 1 : 0);
		printf("\n");

		ComPtr<IDxcBlobEncoding> disassemble_blob;
		if (kPrintDisassembly && SUCCEEDED(compiler->Disassemble(shader_blob.Get(), &disassemble_blob)))
		{
			std::string message((const char*)disassemble_blob->GetBufferPointer(), disassemble_blob->GetBufferSize());
			printf("%s", message.c_str());
			printf("\n");
		}
	}

	//////////////////////////////////////////////////////////////////////////

	struct UAV
	{
		UAV()
		{
			mProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			mProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			mProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			mProperties.CreationNodeMask = 0;
			mProperties.VisibleNodeMask = 0;

			mDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			mDesc.Alignment = 0;
			mDesc.Width = 1;
			mDesc.Height = 1;
			mDesc.DepthOrArraySize = 1;
			mDesc.MipLevels = 1;
			mDesc.Format = DXGI_FORMAT_UNKNOWN;
			mDesc.SampleDesc.Count = 1;
			mDesc.SampleDesc.Quality = 0;
			mDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			mDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			mReadbackProperties = mProperties;
			mReadbackProperties.Type = D3D12_HEAP_TYPE_READBACK;

			mReadbackDesc = mDesc;
			mReadbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		D3D12_RESOURCE_DESC mDesc = {};		
		D3D12_HEAP_PROPERTIES mProperties = {};
		ComPtr<ID3D12Resource> mGPUResource;

		D3D12_RESOURCE_DESC mReadbackDesc = {};
		D3D12_HEAP_PROPERTIES mReadbackProperties = {};
		ComPtr<ID3D12Resource> mReadbackResource;
	};

	UAV uav;
	uav.mDesc.Width = kTotalSize * sizeof(float) * 4;
	uav.mReadbackDesc.Width = uav.mDesc.Width;
	device->CreateCommittedResource(&uav.mProperties, D3D12_HEAP_FLAG_NONE, &uav.mDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uav.mGPUResource));
	device->CreateCommittedResource(&uav.mReadbackProperties, D3D12_HEAP_FLAG_NONE, &uav.mReadbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&uav.mReadbackResource));
	
	ComPtr<ID3D12RootSignature> root_signature;
	device->CreateRootSignature(0, shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = root_signature.Get();
	pso_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
	pso_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
	ComPtr<ID3D12PipelineState> pso;
	device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso));

	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue> command_queue;
	device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue));

	ComPtr<ID3D12CommandAllocator> command_allocator;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));

	ComPtr<ID3D12GraphicsCommandList> command_list;
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list));

	//////////////////////////////////////////////////////////////////////////

	command_list->SetComputeRootSignature(root_signature.Get());
	command_list->SetComputeRootUnorderedAccessView(0, uav.mGPUResource->GetGPUVirtualAddress());
	command_list->SetPipelineState(pso.Get());
	command_list->Dispatch(kDispatchSizeX, kDispatchSizeY, kDispatchSizeZ);

	D3D12_RESOURCE_BARRIER barriers[1] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = uav.mGPUResource.Get();
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	command_list->ResourceBarrier(_countof(barriers), &barriers[0]);	
	command_list->CopyResource(uav.mReadbackResource.Get(), uav.mGPUResource.Get());

	command_list->Close();

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	ComPtr<ID3D12Fence> fence;
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	command_queue->Signal(fence.Get(), 1);

	//////////////////////////////////////////////////////////////////////////
	
	HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	fence->SetEventOnCompletion(1, handle);
	WaitForSingleObject(handle, INFINITE);

	float* data = nullptr;
	D3D12_RANGE range = { 0, uav.mReadbackDesc.Width };
	uav.mReadbackResource->Map(0, &range, (void**)&data);

	for (int i = 0; i < uav.mReadbackDesc.Width / sizeof(float) / 4; i++)
		printf("uav[%d] = %.3f, %.3f, %.3f, %.3f\n", i, data[i * 4 + 0], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
	printf("\n");

	if (kPrintThreadSwizzle)
	{
		int swizzled_index[kTotalSizeX][kTotalSizeY] = { 0 };
		for (int i = 0; i < kTotalSizeX * kTotalSizeY; i++)
		{
			int x = static_cast<int>(data[i * 4 + 0]);
			int y = static_cast<int>(data[i * 4 + 1]);
			swizzled_index[x][y] = i;
		}

		float distance = 0.0f;
		float current_x = 0.0f;
		float current_y = 0.0f;
		for (int i = 1; i < kTotalSizeX * kTotalSizeY; i++)
		{
			for (int y = 0; y < kTotalSizeY; y++)
				for (int x = 0; x < kTotalSizeX; x++)
					if (swizzled_index[x][y] == i)
					{
						distance += sqrtf((x - current_x) * (x - current_x) + (y - current_y) * (y - current_y));
						current_x = x * 1.0f;
						current_y = y * 1.0f;
						goto find_next;
					}
		find_next:
			;
		}

		printf("Thread Swizzle, Pixel-to-Pixel Distance Sum = %.2f\n", distance);
		for (int y = 0; y < kTotalSizeY; y++)
		{
			for (int x = 0; x < kTotalSizeX; x++)
			{
				printf("%*d ", 4, swizzled_index[x][y]);
				if ((x + 1) % kThreadGroupSizeX == 0)
					printf(" | ");
			}

			printf("\n");
			if ((y + 1) % kThreadGroupSizeY == 0)
				printf("\n");
		}
	}

	uav.mReadbackResource->Unmap(0, nullptr);

	return 0;
}