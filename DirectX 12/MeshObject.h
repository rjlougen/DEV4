#pragma once
using namespace Microsoft::WRL;
#include "Helpers.h"

class MeshObject
{
public:
	MESH_DATA					meshInstances;
	unsigned					instanceCount = 0;

	std::vector<H2B::VERTEX>	vertices;
	std::vector<unsigned>		indices;
	ComPtr<ID3D12Resource>		vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	vertexView;
	ComPtr<ID3D12Resource>		indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW		indexView;
	unsigned int				vertexCount = 0;
	unsigned int				indexCount = 0;

	D3D12_PRIMITIVE_TOPOLOGY	primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	std::vector<H2B::MESH> meshes;
	std::vector<H2B::MATERIAL> materials;

	ComPtr<ID3D12Resource>		constantBuffer;

	void CreateConstantBuffer(ID3D12Device* device, SCENE_DATA scene, UINT bufferCount, ComPtr<ID3D12DescriptorHeap> descHeap) {
		device->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(Helpers::CalculateConstantBufferByteSize((sizeof(SCENE_DATA) + vertices.size() * sizeof(H2B::VERTEX))) * bufferCount),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer));

		UINT8* transferMemoryLocation;
		UINT sceneOffset = 0;
		UINT frameOffset = sizeof(SCENE_DATA) + (sizeof(MESH_DATA));

		constantBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(&transferMemoryLocation[sceneOffset], &scene, sizeof(scene));
		memcpy(&transferMemoryLocation[sceneOffset + frameOffset], &scene, sizeof(scene));
		sceneOffset += sizeof(scene);
		memcpy(&transferMemoryLocation[sceneOffset], &meshInstances, (sizeof(MESH_DATA)));
		memcpy(&transferMemoryLocation[sceneOffset + frameOffset], &meshInstances, (sizeof(MESH_DATA)));
		constantBuffer->Unmap(0, nullptr);

		constantBuffer->SetName(L"MeshObject Constant Buffer");

		D3D12_CONSTANT_BUFFER_VIEW_DESC constDesc;
		constDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
		constDesc.SizeInBytes = Helpers::CalculateConstantBufferByteSize((sizeof(SCENE_DATA) + (sizeof(MESH_DATA))) * bufferCount);
		device->CreateConstantBufferView(&constDesc, descHeap->GetCPUDescriptorHandleForHeapStart());
	}

	void CreateVertexBuffer(ID3D12Device* device) {
		HRESULT hr = device->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(H2B::VERTEX) * vertexCount),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer));

		// Transfer triangle data to the vertex buffer.
		UINT8* transferMemoryLocation;

		vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, vertices.data(), sizeof(H2B::VERTEX) * vertexCount);
		vertexBuffer->Unmap(0, nullptr);

		// Create a vertex View
		vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexView.StrideInBytes = sizeof(H2B::VERTEX);
		vertexView.SizeInBytes = sizeof(H2B::VERTEX) * vertexCount;

		vertexBuffer->SetName(L"Vertex Buffer");

		if (hr != S_OK) {
			exit(1);
		}
	}

	void CreateIndexBuffer(ID3D12Device* device) {
		HRESULT hr = device->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(const unsigned int) * indexCount),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer));

		// Transfer triangle data to the vertex buffer.
		UINT8* transferMemoryLocation;
		indexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, indices.data(), sizeof(const unsigned int) * indexCount);
		indexBuffer->Unmap(0, nullptr);

		// Create a index view
		indexView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexView.Format = DXGI_FORMAT_R32_UINT;
		indexView.SizeInBytes = sizeof(const unsigned int) * indexCount;

		indexBuffer->SetName(L"Index Buffer");

		if (hr != S_OK) {
			exit(1);
		}
	}

	void Bind(ID3D12GraphicsCommandList* cmd)
	{
		// Set shaders
		if (constantBuffer) {
			UINT sceneOffset = sizeof(SCENE_DATA);
			UINT frameOffset = sizeof(SCENE_DATA) + sizeof(MESH_DATA);
			UINT frameNumber = 0;

			D3D12_GPU_VIRTUAL_ADDRESS address = constantBuffer->GetGPUVirtualAddress();

			cmd->SetGraphicsRootConstantBufferView(1, Helpers::CalculateConstantBufferByteSize(address + sceneOffset + (frameOffset * frameNumber)));
		}

		// Set vertex buffer
		if (vertexBuffer)
			cmd->IASetVertexBuffers(0, 1, &vertexView);
		// Set index buffer
		if (indexBuffer)
			cmd->IASetIndexBuffer(&indexView);
		// Set primitive topology
		cmd->IASetPrimitiveTopology(primitiveTopology);
	}

	void DrawObject(ID3D12GraphicsCommandList* cmd) {
		Bind(cmd);
		cmd->DrawIndexedInstanced(indices.size(), instanceCount, 0, 0, 0);
	}
};