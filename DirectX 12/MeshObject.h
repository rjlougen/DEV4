#pragma once
using namespace Microsoft::WRL;

struct MeshInstance {
	GW::MATH::GMATRIXF worldMatrix = GW::MATH::GIdentityMatrixF;
};


class MeshObject
{
public:
	std::vector<MeshInstance>	meshInstances;

	ComPtr<ID3D12Resource>		vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	vertexView;
	ComPtr<ID3D12Resource>		indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW		indexView;
	unsigned int				vertexCount = 0;
	unsigned int				vertexSize = 0;
	unsigned int				indexCount = 0;

	D3D12_PRIMITIVE_TOPOLOGY	primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	void CreateVertexBuffer(ID3D12Device* device, float* verts, int size, int count) {
		HRESULT hr = device->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size * count),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer));

		// Transfer triangle data to the vertex buffer.
		UINT8* transferMemoryLocation;

		vertexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, verts, size * count);
		vertexBuffer->Unmap(0, nullptr);

		// Create a vertex View
		vertexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexView.StrideInBytes = size;
		vertexView.SizeInBytes = size * count;

		vertexCount = count;
		vertexSize = size;

		vertexBuffer->SetName(L"Vertex Buffer");

		if (hr != S_OK) {
			exit(1);
		}
	}

	void CreateIndexBuffer(ID3D12Device* device, const unsigned int* indices, int count) {
		HRESULT hr = device->CreateCommittedResource( // using UPLOAD heap for simplicity
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // DEFAULT recommend  
			D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(const unsigned int) * count),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer));

		// Transfer triangle data to the vertex buffer.
		UINT8* transferMemoryLocation;
		indexBuffer->Map(0, &CD3DX12_RANGE(0, 0),
			reinterpret_cast<void**>(&transferMemoryLocation));
		memcpy(transferMemoryLocation, indices, sizeof(const unsigned int) * count);
		indexBuffer->Unmap(0, nullptr);

		// Create a index view
		indexView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		indexView.Format = DXGI_FORMAT_R32_UINT;
		indexView.SizeInBytes = sizeof(const unsigned int) * count;

		indexCount = count;

		indexBuffer->SetName(L"Index Buffer");

		if (hr != S_OK) {
			exit(1);
		}
	}

	/*
	void Bind(ID3D12DeviceContext* context)
	{
		// Set shaders
		if (constantBufferVS)
			context->VSSetConstantBuffers(0, 1, constantBufferVS.GetAddressOf());
		if (constantBufferPS)
			context->PSSetConstantBuffers(0, 1, constantBufferPS.GetAddressOf());
		if (inputLayout)
			context->IASetInputLayout(inputLayout.Get());
		if (vertexShader)
			context->VSSetShader(vertexShader.Get(), nullptr, 0);
		if (pixelShader)
			context->PSSetShader(pixelShader.Get(), nullptr, 0);

		// Set texture and sampler.
		if (resourceView.Get())
			context->PSSetShaderResources(0, 1, resourceView.GetAddressOf());
		if (samplerState.Get())
			context->PSSetSamplers(0, 1, samplerState.GetAddressOf());

		// Set vertex buffer
		UINT stride = vertexSize;
		UINT offset = 0;
		if (vertexBuffer)
			context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride,
				&offset);
		// Set index buffer
		if (indexBuffer)
			context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		// Set primitive topology
		context->IASetPrimitiveTopology(primitiveTopology);
	}
	*/
};