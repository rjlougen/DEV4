// @TODO - use data oriented rendering
// @TODO - change from constant buffer to structured buffer, constant buffer will be too small

// vertex buffer/indexs contains every unique mesh
// need map of map<unique, offset in vertex buffer>. inside of unique is world matrix/mesh/etc...

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include "d3dx12.h" // official helper file provided by microsoft

#include "DXRPipeline.h"
#include "ModelImporter.h"

using namespace Microsoft::WRL;

// Creation, Rendering & Cleanup
class Renderer
{
	// proxy handles
	GW::SYSTEM::GWindow win;
	GW::GRAPHICS::GDirectX12Surface d3d;

	// Gateware required variables
	GW::MATH::GMatrix GMatrix;
	GW::INPUT::GInput GInput;

	// Imports models and game levels
	ModelImporter importer;

	// what we need at a minimum to draw a triangle
	D3D12_VERTEX_BUFFER_VIEW					vertexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		vertexBuffer;

	D3D12_INDEX_BUFFER_VIEW						indexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		indexBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource>		constantBuffer; // Holds information about scene and all the meshes
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>	rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>	pipeline;

	DXRPipeline									dxrPipeline;
	bool										bUseRaytracing = false;
	std::vector<MeshObject>						models;
	Camera*										mainCamera;

	SCENE_DATA scene; // @TODO - replace me
	MESH_DATA mesh01[2]; // @TODO - replace me

	// Data oriented variables
	unsigned int currentOffset = 0;
	std::map<unsigned int, H2B::Parser> meshMap;
	std::vector<OBJ_VERT> masterListOfVerts = {};
	std::vector<unsigned int> masterListOfIndices = {};
	std::vector<MESH_DATA> worldMeshes;
public:
	Renderer(GW::SYSTEM::GWindow _win, GW::GRAPHICS::GDirectX12Surface _d3d)
	{
		win = _win;
		d3d = _d3d;
		ID3D12Device5* creator;
		d3d.GetDevice((void**)&creator);

		GMatrix.Create();
		GInput.Create(_win);

		mainCamera = new Camera();
		mainCamera->InitCamera(creator, d3d, win);

		CreateLights();
		CreateMesh();
		CreateProjection();
		CreateView();

		// Import model information
		std::vector<GameLevel> gameLevel = importer.LoadGameLevel("../GameLevel.txt");

		for (size_t i = 0; i < gameLevel.size(); i++)
		{
			// load object
			// set object matrix
		}
		LoadObject("../FSLogo.h2b");

		// Raytracing
		dxrPipeline.InitializeDXRPipeline(win, d3d, models, mainCamera);

		// Non-raytracing
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

		// Descriptor heap is essentially an array of descriptors
		// 1 - Constant buffer
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NumDescriptors = 1;
		creator->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descHeap));

		D3D12_CONSTANT_BUFFER_VIEW_DESC constDesc;
		constDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
		constDesc.SizeInBytes = Helpers::CalculateConstantBufferByteSize((sizeof(SCENE_DATA) + (meshMap.size() * sizeof(MESH_DATA))) * desc.BufferCount);
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
		
		// Used in rootSignature
		CD3DX12_ROOT_PARAMETER rootParam[2];
		rootParam[0].InitAsConstantBufferView(0, 0); // camera & lights
		rootParam[1].InitAsConstantBufferView(1, 0); // mesh

		// create root signature
		// specifies the data types that shaders should expect from the application
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(2, rootParam, 0, nullptr, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		D3D12SerializeRootSignature(&rootSignatureDesc, 
			D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
		creator->CreateRootSignature(0, signature->GetBufferPointer(), 
			signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));

		// create pipeline state
		//used to describe how the graphics/compute pipeline will behave 
		//in every Pipeline Stage when we are going to render/dispatch something.
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

		// srv description
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = worldMeshes.size();
		srvDesc.Buffer.StructureByteStride = sizeof(worldMeshes[0].world); //cannot be larger than 1024

		// free temporary handle
		creator->Release();
		swapChain->Release();
	}

	void Render()
	{
		if (bUseRaytracing) {
			RenderRaytracing();
		}
		else {
			// grab the context & render target
			ID3D12GraphicsCommandList* cmd;
			D3D12_CPU_DESCRIPTOR_HANDLE rtv;
			D3D12_CPU_DESCRIPTOR_HANDLE dsv;
			d3d.GetCommandList((void**)&cmd);
			d3d.GetCurrentRenderTargetView((void**)&rtv);
			d3d.GetDepthStencilView((void**)&dsv);

			// setup the pipeline
			cmd->SetGraphicsRootSignature(rootSignature.Get());

			cmd->SetDescriptorHeaps(0, &descHeap);

			UINT sceneOffset = sizeof(SCENE_DATA);
			UINT frameOffset = sizeof(SCENE_DATA) + (masterListOfVerts.size() * sizeof(OBJ_VERT));
			UINT frameNumber = 0;

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
	}

	void RenderRaytracing() {
		dxrPipeline.StartFrame();
		// Logic here
		dxrPipeline.EndFrame();
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

		mesh01[1].material = FSLogo_materials[1].attrib;
		GMatrix.IdentityF(mesh01[1].world);
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

	~Renderer()
	{
		// ComPtr will auto release so nothing to do here
		if (mainCamera) {
			delete mainCamera;
		}
	}
};
