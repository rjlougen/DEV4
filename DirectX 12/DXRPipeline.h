#pragma once

#include <dxcapi.h>
#include "dxcapi.use.h"
#include "MeshObject.h"
#include "Camera.h"

class DXRPipeline {
	struct AccelerationStructureBuffer
	{
		ID3D12Resource* pScratch = nullptr;
		ID3D12Resource* pResult = nullptr;
		ID3D12Resource* pInstanceDesc = nullptr;	// only used in top-level AS
	};


	// Used in TLAS, BLAS
	D3D12_HEAP_PROPERTIES kUploadHeapProperties =
	{
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0,
	};

	D3D12_HEAP_PROPERTIES kDefaultHeapProps =
	{
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0,
		0
	};

	// Holding these things for efficiency
	GW::GRAPHICS::GDirectX12Surface					d3d;
	GW::SYSTEM::GWindow								win;
	ComPtr<IDXGISwapChain4>							swapChain;
	ComPtr<ID3D12Device5>							device;
	ComPtr<ID3D12GraphicsCommandList4>				commandList;
	ComPtr<ID3D12CommandQueue>						commandQueue;
	ComPtr<ID3D12Fence>								fence;
	ComPtr<ID3D12CommandAllocator>					commandAllocator;

	UINT											bufferSize = 2;
	ID3D12Resource*									backBuffer[2] = { nullptr, nullptr }; // Change this if the bufferSize changes

	// Compiling dxr shaders
	dxc::DxcDllSupport								dxcHelper;
	ComPtr <IDxcCompiler>							compiler = nullptr;
	ComPtr<IDxcLibrary>								library = nullptr;
	ComPtr<IDxcIncludeHandler>						dxcIncludeHandler = nullptr;

	ComPtr<ID3D12DescriptorHeap>					descriptorHeap = nullptr; // Describes the heap on the GPU
	ComPtr<ID3D12DescriptorHeap>					rtvHeap = nullptr; 
	UINT											rtvDescSize = 0;

	// For syncing cpu and gpu
	UINT64											fenceValues[2] = { 0, 0 };
	HANDLE											fenceEvent; // Used for frame synchronization
	UINT											frameIndex = 0;

	UINT											windowWidth = 800;
	UINT											windowHeight = 600;

	// Final output
	ComPtr<ID3D12Resource>							DXROutput;

	// TLAS is the top level which holds the instances
	// BLAS is the bottom level which holds the unique models
	AccelerationStructureBuffer						TLAS;
	AccelerationStructureBuffer						BLAS;
	uint64_t										TLASSize;

	// Shader stuff
	ComPtr<IDxcBlob>								raygenShader = nullptr;
	IDxcBlob*										missShader = nullptr;
	IDxcBlob*										chsShader = nullptr;
	IDxcBlob*										ahsShader = nullptr;

	// Holds all the shaders
	ComPtr<ID3D12Resource>							shaderTable;
	uint32_t										shaderTableEntrySize = 0;
public:
	DXRPipeline() {}
	DXRPipeline(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GDirectX12Surface _d3d) {
		d3d = _d3d;
		win = _win;

		unsigned int height, width;
		_win.GetHeight(height);
		_win.GetWidth(width);

		_d3d.GetSwapchain4((void**)&swapChain);
		_d3d.GetDevice((void**)&device);
		_d3d.GetCommandList((void**)&commandList);
		_d3d.GetCommandQueue((void**)&commandQueue);
		_d3d.GetFence((void**)&fence);
		_d3d.GetCommandAllocator((void**)&commandAllocator);
	}

	void InitializeDXRPipeline(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GDirectX12Surface _d3d, std::vector<MeshObject> meshObjects, Camera* camera) {
		d3d = _d3d;
		win = _win;

		unsigned int height, width;
		_win.GetHeight(height);
		_win.GetWidth(width);

		_d3d.GetSwapchain4((void**)&swapChain);
		_d3d.GetDevice((void**)&device);
		_d3d.GetCommandList((void**)&commandList);
		_d3d.GetCommandQueue((void**)&commandQueue);
		_d3d.GetFence((void**)&fence);
		_d3d.GetCommandAllocator((void**)&commandAllocator);

		// Compiling hlsl
		dxcHelper.Initialize();
		dxcHelper.CreateInstance(CLSID_DxcCompiler, compiler.GetAddressOf());
		dxcHelper.CreateInstance(CLSID_DxcLibrary, library.GetAddressOf());
		library->CreateIncludeHandler(&dxcIncludeHandler);

		if (meshObjects.size() <= 0) {
			return;
		}

		CreateFence();

		ResetCommandList();

		// @TODO - make it so it accepts multiple mesh objects
		CreateBottomLevelAccelStructure(meshObjects[0]); // Good for unique geometry
		CreateTopLevelAccelStructure(); // Good for instances of geometry
		CreateDXROutput();
		CreateDescriptorHeaps(meshObjects[0], camera); // @TODO - make this accept multiple objects
		//CreateShaderPipelineState();
		//CreateShaderTable();

	}

	// Clear command list/allocator for current frame
	void ResetCommandList()
	{
		HRESULT hr = commandAllocator->Reset();
		hr = commandList->Reset(commandAllocator.Get(), nullptr);

		if (hr != S_OK) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nFailed to reset command list: %ls\n", errMsg);
			exit(1);
		}
	}
	
	// Wait until GPU is ready for new commands
	void WaitForGPU()
	{
		HRESULT hr = commandQueue->Signal(fence.Get(), fenceValues[frameIndex]);

		// Wait until the fence has been processed
		hr = fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent);

		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

		// Increment the fence value for the current frame
		fenceValues[frameIndex]++;

		if (hr != S_OK) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nWait for GPU failed: %ls\n", errMsg);
			exit(1);
		}
	}

	void CreateBackBufferRTV()
	{
		HRESULT hr;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

		rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

		// Create a render target view for each back buffer which we use to draw on
		for (UINT n = 0; n < bufferSize; n++)
		{
			hr = swapChain->GetBuffer(n, IID_PPV_ARGS(&backBuffer[n]));

			device->CreateRenderTargetView(backBuffer[n], nullptr, rtvHandle);

			if (n == 0) backBuffer[n]->SetName(L"Back Buffer 0");
			else backBuffer[n]->SetName(L"Back Buffer 1");

			rtvHandle.ptr += rtvDescSize;
		}

		if (hr != S_OK) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nFailed to create dxr back buffer: %ls\n", errMsg);
			exit(1);
		}
	}

	void CreateFence() {
		fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		if (fenceEvent == nullptr)
		{
			printf("\nFence creation event null\n");
			exit(1);
		}
		fenceValues[frameIndex]++;
	}

	void CreateDXROutput()
	{
		D3D12_HEAP_PROPERTIES DefaultHeapProperties =
		{
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0, 0
		};

		// Describe the DXR output resource (texture)
		// Initialize as a copy source
		D3D12_RESOURCE_DESC desc = {};
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc.Width = windowWidth;
		desc.Height = windowHeight;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		// Create output buffer
		HRESULT hr = device->CreateCommittedResource(&DefaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&DXROutput));
		DXROutput->SetName(L"DXR Output Buffer");
		if (hr != S_OK) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nFailed to create dxr output buffer: %ls\n", errMsg);
			exit(1);
		}
	}

	// @TODO - modify me to create multiple geometry descs for unique objects if needed
	// https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial/dxr_tutorial_helpers
	void CreateBottomLevelAccelStructure(MeshObject meshObject) {
		// Input vertex buffer and index buffer
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.VertexBuffer.StartAddress = meshObject.vertexBuffer->GetGPUVirtualAddress();
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = meshObject.vertexView.StrideInBytes;
		geometryDesc.Triangles.VertexCount = static_cast<UINT>(meshObject.vertexCount);
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.IndexBuffer = meshObject.indexBuffer->GetGPUVirtualAddress();
		geometryDesc.Triangles.IndexFormat = meshObject.indexView.Format;
		geometryDesc.Triangles.IndexCount = static_cast<UINT>(meshObject.indexCount);
		geometryDesc.Triangles.Transform3x4 = 0;
		geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// Get the size requirements for the BLAS buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
		ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		ASInputs.pGeometryDescs = &geometryDesc;
		ASInputs.NumDescs = 1;
		ASInputs.Flags = buildFlags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &info);

		info.ScratchDataSizeInBytes = align_to(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, info.ScratchDataSizeInBytes);
		info.ResultDataMaxSizeInBytes = align_to(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, info.ResultDataMaxSizeInBytes);

		D3D12_HEAP_PROPERTIES kDefaultHeapProps =
		{
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0,
			0
		};

		D3D12_HEAP_PROPERTIES kUploadHeapProps =
		{
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0,
			0,
		};

		// Create the BLAS scratch buffer
		//D3D12BufferCreateInfo bufferInfo(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		//bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

		BLAS.pScratch = Helpers::CreateBuffer(device, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
		BLAS.pScratch->SetName(L"DXR BLAS Scratch");
		BLAS.pResult = Helpers::CreateBuffer(device, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
		BLAS.pResult->SetName(L"DXR BLAS");

		// Describe and build the bottom level acceleration structure
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs = ASInputs;
		buildDesc.ScratchAccelerationStructureData = BLAS.pScratch->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = BLAS.pResult->GetGPUVirtualAddress();

		commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		// Wait for the BLAS build to complete
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = BLAS.pResult;
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	void CreateTopLevelAccelStructure() {
		// Describe the TLAS geometry instance(s)
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.InstanceMask = 0xFF;
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.AccelerationStructure = BLAS.pResult->GetGPUVirtualAddress();
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

		// Create the TLAS instance buffer
		TLAS.pInstanceDesc = Helpers::CreateBuffer(device, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProperties);

		TLAS.pInstanceDesc->SetName(L"DXR TLAS Instance Descriptors");


		// Copy the instance data to the buffer
		UINT8* pData;
		TLAS.pInstanceDesc->Map(0, nullptr, (void**)&pData);
		memcpy(pData, &instanceDesc, sizeof(instanceDesc));
		TLAS.pInstanceDesc->Unmap(0, nullptr);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		// Get the size requirements for the TLAS buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
		ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		ASInputs.InstanceDescs = TLAS.pInstanceDesc->GetGPUVirtualAddress();
		ASInputs.NumDescs = 1;
		ASInputs.Flags = buildFlags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &info);


		info.ResultDataMaxSizeInBytes = align_to(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, info.ResultDataMaxSizeInBytes);
		info.ScratchDataSizeInBytes = align_to(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, info.ScratchDataSizeInBytes);

		// Set TLAS size
		TLASSize = info.ResultDataMaxSizeInBytes;

		TLAS.pScratch = Helpers::CreateBuffer(device, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
		TLAS.pScratch->SetName(L"DXR TLAS Scratch");
		TLAS.pResult = Helpers::CreateBuffer(device, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
		TLAS.pResult->SetName(L"DXR TLAS");
		TLASSize = info.ResultDataMaxSizeInBytes;

		// Describe and build the TLAS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
		buildDesc.Inputs = ASInputs;
		buildDesc.ScratchAccelerationStructureData = TLAS.pScratch->GetGPUVirtualAddress();
		buildDesc.DestAccelerationStructureData = TLAS.pResult->GetGPUVirtualAddress();

		commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

		// Wait for the TLAS build to complete
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = TLAS.pResult;
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	// @TODO - make this accept multiple objects
	void CreateDescriptorHeaps(MeshObject meshObject, Camera* camera)
	{
		// Describe the RTV descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.NumDescriptors = 2;
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		// Create the RTV heap
		HRESULT hr = device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap));
		rtvHeap->SetName(L"RTV Descriptor Heap");
		rtvDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Describe the CBV/SRV/UAV heaps
		// 5 entries:
		// 1 CBV for the ViewCB
		// 1 UAV for the RT output
		// 1 SRV for the Scene BVH
		// 1 SRV for the index buffer
		// 1 SRV for the vertex buffer

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 5;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		// Create the descriptor heap
		hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));

		// Get the descriptor heap handle and increment size
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		UINT handleIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		descriptorHeap->SetName(L"DXR Descriptor Heap");

		// Create the ViewCB CBV
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.SizeInBytes = align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(camera->cameraData));
		cbvDesc.BufferLocation = camera->viewBuffer->GetGPUVirtualAddress();

		device->CreateConstantBufferView(&cbvDesc, handle);

		// Create the DXR output buffer UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		handle.ptr += handleIncrement;
		device->CreateUnorderedAccessView(DXROutput.Get(), nullptr, &uavDesc, handle);

		// Create the DXR Top Level Acceleration Structure Shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.RaytracingAccelerationStructure.Location = TLAS.pResult->GetGPUVirtualAddress();

		handle.ptr += handleIncrement;
		device->CreateShaderResourceView(nullptr, &srvDesc, handle);

		// Create the index buffer Shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
		indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		indexSRVDesc.Buffer.StructureByteStride = 0;
		indexSRVDesc.Buffer.FirstElement = 0;
		indexSRVDesc.Buffer.NumElements = meshObject.indexCount;
		indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		device->CreateShaderResourceView(meshObject.indexBuffer.Get(), &indexSRVDesc, handle);

		// Create the vertex buffer Shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
		vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		vertexSRVDesc.Buffer.StructureByteStride = 0;
		vertexSRVDesc.Buffer.FirstElement = 0;
		vertexSRVDesc.Buffer.NumElements = meshObject.vertexCount;
		vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		handle.ptr += handleIncrement;
		device->CreateShaderResourceView(meshObject.vertexBuffer.Get(), &vertexSRVDesc, handle);

		if (hr != S_OK) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nCreateDescriptorHeaps failed: %ls\n", errMsg);
			exit(1);
		}
	}

	void StartFrame() {
		uint32_t rtvIndex = swapChain->GetCurrentBackBufferIndex();
	}

	void EndFrame() {
		uint32_t rtvIndex = swapChain->GetCurrentBackBufferIndex();
	}
};