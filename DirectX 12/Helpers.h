#pragma once

#include <comdef.h>
#include "h2bParser.h"

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define arraysize(a) (sizeof(a)/sizeof(a[0]))

using namespace Microsoft::WRL;

//  Used in raytracing
struct AccelerationStructureBuffers
{
	ID3D12Resource* pScratch;
	ID3D12Resource* pResult;
	ID3D12Resource* pInstanceDesc;    // Used only for top-level AS
};

// Used in camera
struct CameraData {
	GW::MATH::GVECTORF cameraLocation = GW::MATH::GIdentityVectorF;
	GW::MATH::GVECTORF cameraDirection = GW::MATH::GIdentityVectorF;
	GW::MATH::GMATRIXF viewMatrix = GW::MATH::GIdentityMatrixF;
	GW::MATH::GMATRIXF projectionMatrix = GW::MATH::GIdentityMatrixF;
};

// Used for shader information
struct SCENE_DATA {
	GW::MATH::GVECTORF sunDir, sunColor, sunAmbient, camPos;
	GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	GW::MATH::GVECTORF padding[4];
};

// Used for object shader information
struct MESH_DATA {
	GW::MATH::GMATRIXF world[64];
	H2B::ATTRIBUTES material;
	//UINT instanceOffset;
	//unsigned padding[28];
};

struct INSTANCE_DATA
{
	float KdX;
	float KdY;
	float KdZ;
	float Ns;
};

struct GameLevel {
	std::string type;
	std::string modelName;
	GW::MATH::GMATRIXF worldMatrix;
};

class Helpers {
public:
	static ID3D12Resource* CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Device5> device, ID3D12Resource** buffer, UINT64 size)
	{
		D3D12_RESOURCE_DESC bufDesc = {};
		bufDesc.Alignment = 0;
		bufDesc.DepthOrArraySize = 1;
		bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		bufDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufDesc.Height = 1;
		bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufDesc.MipLevels = 1;
		bufDesc.SampleDesc.Count = 1;
		bufDesc.SampleDesc.Quality = 0;
		bufDesc.Width = size;

		ID3D12Resource* ppBuffer = nullptr;

		HRESULT hr = device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ppBuffer));
		buffer = &ppBuffer;
		if (FAILED(hr))
		{
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nFailed to create dxr constant buffer: %ls\n", errMsg);
			exit(1);
		}
		return ppBuffer;
	}

	static UINT CalculateConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
	};

	static ID3D12Resource* CreateBuffer(ComPtr<ID3D12Device5> device, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
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
		HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
		if (FAILED(hr))
		{
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			printf("\nFailed to create dxr buffer: %ls\n", errMsg);
			exit(1);
			return nullptr;
		}

		return pBuffer;
	}
};