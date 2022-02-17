// minimalistic code to draw a single triangle, this is not part of the API.
// required for compiling shaders on the fly, consider pre-compiling instead
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include "d3dx12.h" // official helper file provided by microsoft

#include "FSLogo.h"
#include "MeshObject.h"
#include "build/ModelImporter.h"

// @TODO - use data oriented rendering
// @TODO - change from constant buffer to structured buffer, constant buffer will be too small

struct SCENE_DATA {
	GW::MATH::GVECTORF sunDir, sunColor, sunAmbient, camPos;
	GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	GW::MATH::GVECTORF padding[4];
};

struct MESH_DATA {
	GW::MATH::GMATRIXF world;
	OBJ_ATTRIBUTES material;
	unsigned padding[28];
};

// vertex buffer/indexs contains every unique mesh
// need map of map<unique, offset in vertex buffer>. inside of unique is world matrix/mesh/etc...

// @RAYTRACING
struct AccelerationStructureBuffers
{
	ID3D12Resource* pScratch;
	ID3D12Resource* pResult;
	ID3D12Resource* pInstanceDesc;    // Used only for top-level AS
};
D3D12_HEAP_PROPERTIES kUploadHeapProps;
D3D12_HEAP_PROPERTIES kDefaultHeapProps;
ID3D12Resource* mpTopLevelAS;
ID3D12Resource* mpBottomLevelAS;
uint64_t mTlasSize = 0;

// Creation, Rendering & Cleanup
class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GDirectX12Surface d3d;
	// what we need at a minimum to draw a triangle
	D3D12_VERTEX_BUFFER_VIEW					vertexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		vertexBuffer;

	D3D12_INDEX_BUFFER_VIEW						indexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		indexBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource>		constantBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>	rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>	pipeline;

	SCENE_DATA scene;
	MESH_DATA mesh01[2];

	GW::MATH::GMatrix GMatrix;
	GW::INPUT::GInput GInput;

	ModelImporter importer;

	unsigned int currentOffset = 0;

	std::map<unsigned int, H2B::Parser> meshMap;
	std::vector<OBJ_VERT> masterListOfVerts = {};
	std::vector<unsigned int> masterListOfIndices = {};

	std::vector<MESH_DATA> worldMeshes;

	// Saving these values from Gateware for efficiency for raytracing
	Microsoft::WRL::ComPtr<IDXGISwapChain4>		m_pSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device5>		m_pDevice;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_pCommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>  m_pCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence>			m_pFence;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
	UINT64										m_fenceValue;
public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GDirectX12Surface _d3d)
	{
		win = _win;
		d3d = _d3d;
		ID3D12Device5* creator;
		d3d.GetDevice((void**)&creator);
		// TODO: part 2a
		GMatrix.Create();
		GMatrix.IdentityF(mesh01[0].world);
		GMatrix.IdentityF(mesh01[1].world);

		GInput.Create(_win);

		IDXGISwapChain4* swapChainTemp;
		d3d.GetSwapchain4((void**)&swapChainTemp);
		m_pSwapChain = swapChainTemp;
		m_pDevice = creator;

		ID3D12GraphicsCommandList4* commandListTemp;
		d3d.GetCommandList((void**)&commandListTemp);
		m_pCommandList = commandListTemp;

		ID3D12CommandQueue* commandQueueTemp;
		d3d.GetCommandQueue((void**)&commandQueueTemp);
		m_pCommandQueue = commandQueueTemp;

		CreateLights();
		CreateMesh();
		CreateProjection();
		CreateView();

		// Import model information
		importer.LoadGameLevel("../GameLevel.txt");
		LoadObject("../FSLogo.h2b");

		creator->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(masterListOfVerts.size() * sizeof(OBJ_VERT)),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer));
		// Transfer triangle data to the vertex buffer.
		UINT8* transferMemoryLocation;
		vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, masterListOfVerts.data(), masterListOfVerts.size() * sizeof(OBJ_VERT));
		vertexBuffer->Unmap(0, nullptr);
		// Create a vertex View to send to a Draw() call.
		vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexView.StrideInBytes = sizeof(OBJ_VERT); // TODO: Part 1e
		vertexView.SizeInBytes = masterListOfVerts.size() * sizeof(OBJ_VERT); // TODO: Part 1d

		// TODO: Part 1g
		creator->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(masterListOfIndices.size() * sizeof(unsigned int)),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer));

		UINT8* transferMemoryLocation2;
		indexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation2));
		memcpy(transferMemoryLocation2, masterListOfIndices.data(), masterListOfIndices.size() * sizeof(unsigned int));
		indexBuffer->Unmap(0, nullptr);

		indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexView.Format = DXGI_FORMAT_R32_UINT;
		indexView.SizeInBytes = masterListOfIndices.size() * sizeof(unsigned int); // TODO: Part 1d

		// TODO: Part 2d
		IDXGISwapChain4 *swapChain = nullptr;
		d3d.GetSwapchain4(reinterpret_cast<void**>(&swapChain));
		DXGI_SWAP_CHAIN_DESC desc;
		swapChain->GetDesc(&desc);
		creator->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer((sizeof(SCENE_DATA) + (meshMap.size() * sizeof(MESH_DATA))) * desc.BufferCount),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer));
		
		UINT8* transferMemoryLocation3;
		UINT sceneOffset = 0;
		UINT frameOffset = sizeof(SCENE_DATA) + (meshMap.size() * sizeof(MESH_DATA));
		
		std::vector<H2B::Parser> key;
		std::vector<unsigned int> value;
		for (std::map<unsigned int, H2B::Parser>::iterator it = meshMap.begin(); it != meshMap.end(); ++it) {
			value.push_back(it->first);
			key.push_back(it->second);
		}

		constantBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation3));
		memcpy(&transferMemoryLocation3[sceneOffset], &scene, sizeof(scene));
		memcpy(&transferMemoryLocation3[sceneOffset + frameOffset], &scene, sizeof(scene));
		sceneOffset += sizeof(scene);
		memcpy(&transferMemoryLocation3[sceneOffset], key.data(), key.size() * sizeof(MESH_DATA));
		memcpy(&transferMemoryLocation3[sceneOffset + frameOffset], key.data(), key.size() * sizeof(MESH_DATA));
		constantBuffer->Unmap(0, nullptr);

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NumDescriptors = 1;
		creator->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descHeap));

		D3D12_CONSTANT_BUFFER_VIEW_DESC constDesc;
		constDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
		constDesc.SizeInBytes = CalculateConstantBufferByteSize((sizeof(SCENE_DATA) + (meshMap.size() * sizeof(MESH_DATA))) * desc.BufferCount);
		creator->CreateConstantBufferView(&constDesc, descHeap->GetCPUDescriptorHandleForHeapStart());

		// Create Vertex Shader
		UINT compilerFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
		compilerFlags |= D3DCOMPILE_DEBUG;
#endif
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, errors;
		if (FAILED(D3DCompileFromFile(L"../VertexShader.hlsl",
			nullptr, nullptr, "main", "vs_5_1", compilerFlags, 0, 
			vsBlob.GetAddressOf(), errors.GetAddressOf())))
		{
			std::cout << (char*)errors->GetBufferPointer() << std::endl;
			abort();
		}
		// Create Pixel Shader
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob; errors.Reset();
		if (FAILED(D3DCompileFromFile(L"../PixelShader.hlsl", nullptr,
			nullptr, "main", "ps_5_1", compilerFlags, 0, 
			psBlob.GetAddressOf(), errors.GetAddressOf())))
		{
			std::cout << (char*)errors->GetBufferPointer() << std::endl;
			abort();
		}	
		// TODO: Part 1e
		// Create Input Layout
		D3D12_INPUT_ELEMENT_DESC format[] = {
			{ 
				"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 
				D3D12_APPEND_ALIGNED_ELEMENT, 
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			},
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		// TODO: Part 2g
		CD3DX12_ROOT_PARAMETER rootParam[2];
		rootParam[0].InitAsConstantBufferView(0, 0); // camera & lights
		rootParam[1].InitAsConstantBufferView(1, 0); // mesh

		// create root signature
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(2, rootParam, 0, nullptr, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		D3D12SerializeRootSignature(&rootSignatureDesc, 
			D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
		creator->CreateRootSignature(0, signature->GetBufferPointer(), 
			signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

		// create pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psDesc;
		ZeroMemory(&psDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		psDesc.InputLayout = { format, ARRAYSIZE(format) };
		psDesc.pRootSignature = rootSignature.Get();
		psDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
		psDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
		psDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psDesc.SampleMask = UINT_MAX;
		psDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psDesc.NumRenderTargets = 1;
		psDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psDesc.SampleDesc.Count = 1;
		creator->CreateGraphicsPipelineState(&psDesc, IID_PPV_ARGS(&pipeline));

		// @RAYTRACING
		//uint32_t vertexCount[1] = { 3885 };
		//CreateAccelerationStructures(vertexBuffer.Get(), vertexCount, sizeof(OBJ_VERT));

		// free temporary handle
		creator->Release();
		swapChain->Release();
	}

	void Render()
	{
		// TODO: Part 4d
		// grab the context & render target
		ID3D12GraphicsCommandList* cmd;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv;
		D3D12_CPU_DESCRIPTOR_HANDLE dsv;
		d3d.GetCommandList((void**)&cmd);
		d3d.GetCurrentRenderTargetView((void**)&rtv);
		d3d.GetDepthStencilView((void**)&dsv);
		// setup the pipeline
		cmd->SetGraphicsRootSignature(rootSignature.Get());
		
		// TODO: Part 2h
		cmd->SetDescriptorHeaps(0, &descHeap);
		//cmd->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
		UINT sceneOffset = sizeof(SCENE_DATA);
		UINT frameOffset = sizeof(SCENE_DATA) + (masterListOfVerts.size() * sizeof(OBJ_VERT));
		UINT frameNumber = 0;
		//d3d.GetSwapChainBufferIndex(frameNumber);

		D3D12_GPU_VIRTUAL_ADDRESS address = constantBuffer->GetGPUVirtualAddress();

		// SCENE
		cmd->SetGraphicsRootConstantBufferView(0, address + (frameOffset * frameNumber));

		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
		cmd->SetPipelineState(pipeline.Get());
		// now we can draw
		cmd->IASetVertexBuffers(0, 1, &vertexView);
		cmd->IASetIndexBuffer(&indexView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		RotateLogo();
		
		for (std::map<unsigned int, H2B::Parser>::iterator it = meshMap.begin(); it != meshMap.end(); ++it) {
			for (size_t i = 0; i < it->second.meshes.size(); i++)
			{
				cmd->SetGraphicsRootConstantBufferView(1, address + sceneOffset + (frameOffset * frameNumber));
				cmd->DrawIndexedInstanced(it->second.meshes[i].drawInfo.indexCount, 1, it->second.meshes[i].drawInfo.indexOffset, 0, 0);
				sceneOffset += sizeof(MESH_DATA);
			}
		}
		// release temp handles
		cmd->Release();
	}

	void CreateLights() {
		scene.sunColor = { 0.9f, 0.9f, 1.0f };
		scene.sunDir = { -1.0f, -1.0f, 2.0f };
		scene.camPos = { 0.75f, 0.25f, -1.5f, 1.0f };
		scene.sunAmbient = { 0.25f, 0.25f, 0.35f, 1.0f };
	}

	void CreateMesh() {
		mesh01[0].material = FSLogo_materials[0].attrib;
		GMatrix.IdentityF(mesh01[0].world);

		MESH_DATA temp = mesh01[0];
		worldMeshes.push_back(temp);

		//meshMap.insert(meshMap.begin(), std::pair<unsigned int, MESH_DATA>(0, temp));

		mesh01[1].material = FSLogo_materials[1].attrib;
		GMatrix.IdentityF(mesh01[1].world);

		temp = mesh01[1];

		//meshMap.insert(meshMap.begin(), std::pair<unsigned int, MESH_DATA>(3885, temp));
	}

	void CreateProjection() {
		GMatrix.IdentityF(scene.projectionMatrix);

		float NEAR_PLANE = 0.1f;
		float FAR_PLANE = 100.0f;
		float ratio = 0.0f;

		d3d.GetAspectRatio(ratio);

		GMatrix.ProjectionDirectXLHF(1.5708f, ratio, NEAR_PLANE, FAR_PLANE, scene.projectionMatrix); //(float _fovY, float _aspect, float _zn, float _zf, GW::MATH::GMATRIXF& _outMatrix)

	}

	void CreateView() {
		GMatrix.IdentityF(scene.viewMatrix);

		GW::MATH::GVECTORF Eye = { 0.75f, 0.25f, -1.5f, 1.0f };
		GW::MATH::GVECTORF At = { 0.15f, 0.75f, 0.0f, 1.0f };
		GW::MATH::GVECTORF Up = { 0.0f, 1.0f, 0.0f, 1.0f };

		GMatrix.LookAtLHF(Eye, At, Up, scene.viewMatrix);
	}

	UINT CalculateConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
	};

	void RotateLogo()
	{
		GMatrix.RotateYGlobalF(mesh01[1].world, 0.01f, mesh01[1].world);

		UINT8* transferMemoryLocation3;
		UINT offset = 0;
		UINT frameOffset = sizeof(SCENE_DATA) + sizeof(MESH_DATA) + sizeof(MESH_DATA);
		constantBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation3));
		offset += sizeof(scene);
		memcpy(&transferMemoryLocation3[offset], &mesh01, sizeof(mesh01));
		memcpy(&transferMemoryLocation3[offset + frameOffset], &mesh01, sizeof(mesh01));
		constantBuffer->Unmap(0, nullptr);
	}

	std::chrono::steady_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
	void UpdateCamera() {
		GMatrix.InverseF(scene.viewMatrix, scene.viewMatrix);

		auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastTime);
		lastTime = std::chrono::high_resolution_clock::now();
		float totalYChange, totalXChange, totalZChange, totalPitch, totalYaw;
		const float Camera_Speed = 0.3f;

		float yUp, yDown, zUp, zDown, xUp, xDown, yContState, xContState;
		GInput.GetState(G_KEY_SPACE, yUp);
		GInput.GetState(G_KEY_LEFTSHIFT, yDown);
		GInput.GetState(G_KEY_D, xUp);
		GInput.GetState(G_KEY_A, xDown);
		GInput.GetState(G_KEY_W, zUp);
		GInput.GetState(G_KEY_S, zDown);
		GInput.GetState(VK_GAMEPAD_LEFT_THUMBSTICK_UP, xContState);
		GInput.GetState(VK_GAMEPAD_LEFT_THUMBSTICK_LEFT, yContState);

		totalXChange = xUp - xDown;
		totalYChange = yUp - yDown + xContState;
		totalZChange = zUp - zDown + yContState;

		float x = 0, y = 0;
		GW::GReturn result = GInput.GetMouseDelta(x, y);
		if (G_PASS(result) && result != GW::GReturn::REDUNDANT)
		{
			totalPitch = 1.5f * y / 500.0f;
			totalYaw = 1.5f * x / 500.0f;

			GW::MATH::GMATRIXF pitchMatrix;
			GMatrix.IdentityF(pitchMatrix);
			GMatrix.RotateXLocalF(pitchMatrix, totalPitch, pitchMatrix);
			GMatrix.MultiplyMatrixF(pitchMatrix, scene.viewMatrix, scene.viewMatrix);

			GW::MATH::GVECTORF TempPos;
			TempPos = scene.viewMatrix.row4;
			scene.viewMatrix.row4 = { 0,0,0,1 };

			GW::MATH::GMATRIXF yawMatrix;
			GMatrix.IdentityF(yawMatrix);
			GMatrix.RotateYLocalF(yawMatrix, totalYaw, yawMatrix);

			GMatrix.MultiplyMatrixF(scene.viewMatrix, yawMatrix, scene.viewMatrix);
			scene.viewMatrix.row4 = TempPos;
		}


		/*At the top of this function use std::chrono to query the amount
		of time that passes from one call of this function to the next.
		If you’re unsure how to use the standard libraries to achieve this,
		you can also grab the XTime class from CGS,
		just be aware that unlike std::chrono this class is Windows only.*/

		GW::MATH::GVECTORF Translate = { 0.0f, 0.0f, 0.0f, 0.0f };
		Translate.x += totalXChange * Camera_Speed * deltaTime.count() / 1000.0f;
		Translate.y += totalYChange * Camera_Speed * deltaTime.count() / 1000.0f;
		Translate.z += totalZChange * Camera_Speed * deltaTime.count() / 1000.0f;
		GMatrix.TranslateLocalF(scene.viewMatrix, Translate, scene.viewMatrix);

		GMatrix.InverseF(scene.viewMatrix, scene.viewMatrix);

		UINT8* transferMemoryLocation3;
		UINT offset = 0;
		UINT frameOffset = sizeof(SCENE_DATA) + sizeof(MESH_DATA) + sizeof(MESH_DATA);
		constantBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation3));
		memcpy(&transferMemoryLocation3[offset], &scene, sizeof(scene));
		memcpy(&transferMemoryLocation3[offset + frameOffset], &scene, sizeof(scene));
		constantBuffer->Unmap(0, nullptr);
	}

	void LoadObject(std::string path) {
		H2B::Parser tempParser = importer.LoadOBJ(path);

		for (size_t i = 0; i < tempParser.vertices.size(); i++)
		{
			OBJ_VERT tempVert;
			tempVert.pos = { tempParser.vertices[i].pos.x, tempParser.vertices[i].pos.y, tempParser.vertices[i].pos.z };
			tempVert.nrm = { tempParser.vertices[i].nrm.x, tempParser.vertices[i].nrm.y, tempParser.vertices[i].nrm.z };
			tempVert.uvw = { tempParser.vertices[i].uvw.x, tempParser.vertices[i].uvw.y, tempParser.vertices[i].uvw.z };

			masterListOfVerts.push_back(tempVert);
		}

		for (size_t i = 0; i < tempParser.indices.size(); i++)
		{
			masterListOfIndices.push_back(tempParser.indices[i]);
		}

		unsigned int currentOffset = masterListOfVerts.size() * sizeof(OBJ_VERT);

		meshMap.insert(std::pair<unsigned int, H2B::Parser>(currentOffset, tempParser));
	}

	// @RAYTRACING
	ID3D12Resource* CreateBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
	{
		D3D12_RESOURCE_DESC bufDesc = {};
		bufDesc.Alignment = 0;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Flags = flags;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.Height = 1;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufDesc.MipLevels = 1;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.SampleDesc.Quality = 0;
		bufDesc.Width = size;

		ID3D12Resource* pBuffer;
		HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
		if (FAILED(hr))
		{
			exit(1);
			return nullptr;
		}

		return pBuffer;
	}
	
	// @RAYTRACING
	void CreateAccelerationStructures(ID3D12Resource* vertexBuffer, uint32_t vertexCount[], uint32_t strideInBytes)
	{
		d3d.GetFence((void**)&m_pFence);

		AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS(m_pCommandList.Get(), (ID3D12Resource**)vertexBuffer, vertexCount, strideInBytes, 1);
		AccelerationStructureBuffers topLevelBuffers = CreateTopLevelAS(m_pCommandList.Get(), bottomLevelBuffers.pResult, mTlasSize);

		unsigned int fenceValue;
		d3d.GetFenceValue(fenceValue);

		HANDLE eventHandle;
		d3d.GetEventHandle(eventHandle);

		// The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
		SubmitCommandList(m_pCommandList.Get(), m_pCommandQueue.Get(), m_pFence.Get(), fenceValue, fenceValue);
		m_pFence->SetEventOnCompletion(fenceValue, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		uint32_t bufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
		
		// @ROB - this was removed because crashing
		//m_pCommandList->Reset(m_pCommandAllocator.Get(), nullptr);

		// Store the AS buffers. The rest of the buffers will be released once we exit the function
		mpTopLevelAS = topLevelBuffers.pResult;
		mpBottomLevelAS = bottomLevelBuffers.pResult;
	}

	// @RAYTRACING
	AccelerationStructureBuffers CreateBottomLevelAS(ID3D12GraphicsCommandList4* pCmdList, ID3D12Resource* pVB[], const uint32_t vertexCount[], uint32_t strideBytes, uint32_t geometryCount)
	{
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDesc;
		geomDesc.resize(geometryCount);

		for (uint32_t i = 0; i < geometryCount; i++)
		{
			geomDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geomDesc[i].Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
			geomDesc[i].Triangles.VertexBuffer.StrideInBytes = strideBytes;
			geomDesc[i].Triangles.VertexCount = vertexCount[i];
			geomDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geomDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		}

		// Get the size requirements for the scratch and AS buffers
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		inputs.NumDescs = geometryCount;
		inputs.pGeometryDescs = geomDesc.data();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		// Initialize these static values
		kDefaultHeapProps =
		{
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0,
			0
		};

		kUploadHeapProps =
		{
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			D3D12_MEMORY_POOL_UNKNOWN,
			0,
			0,
		};

		// Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
		AccelerationStructureBuffers buffers;

		buffers.pScratch = CreateBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
		buffers.pResult = CreateBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

		// Create the bottom-level AS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
		asDesc.Inputs = inputs;
		asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

		pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

		// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = buffers.pResult;
		pCmdList->ResourceBarrier(1, &uavBarrier);

		return buffers;
	}

	// @RAYTRACING
	AccelerationStructureBuffers CreateTopLevelAS(ID3D12GraphicsCommandList4* pCmdList, ID3D12Resource* pBottomLevelAS, uint64_t& tlasSize)
	{
		// First, get the size of the TLAS buffers and create them
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		inputs.NumDescs = 1;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		m_pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		// Create the buffers
		AccelerationStructureBuffers buffers;
		buffers.pScratch = CreateBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
		buffers.pResult = CreateBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
		tlasSize = info.ResultDataMaxSizeInBytes;

		// The instance desc should be inside a buffer, create and map the buffer
		buffers.pInstanceDesc = CreateBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

		D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
		buffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

		// Initialize the instance desc. We only have a single instance
		pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
		pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
		pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		GW::MATH::GMATRIXF m; // Identity matrix
		m.row1 = { 1, 0, 0, 0 };
		m.row2 = { 0, 1, 0, 0 };
		m.row3 = { 0, 0, 1, 0 };
		m.row4 = { 0, 0, 0, 1 };
		memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));
		pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
		pInstanceDesc->InstanceMask = 0xFF;

		// Unmap
		buffers.pInstanceDesc->Unmap(0, nullptr);

		// Create the TLAS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
		asDesc.Inputs = inputs;
		asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
		asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

		pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

		// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = buffers.pResult;
		pCmdList->ResourceBarrier(1, &uavBarrier);

		return buffers;
	}

	// @RAYTRACING
	void SubmitCommandList(ID3D12GraphicsCommandList4* pCmdList, ID3D12CommandQueue* pCmdQueue, ID3D12Fence* pFence, uint64_t fenceValue, uint64_t outFenceValue)
	{
		pCmdList->Close();
		ID3D12CommandList* pGraphicsList = pCmdList;
		pCmdQueue->ExecuteCommandLists(1, &pGraphicsList);
		fenceValue++;
		pCmdQueue->Signal(pFence, fenceValue);
		outFenceValue = fenceValue;
	}

	~Renderer()
	{
		// ComPtr will auto release so nothing to do here 
	}
};
