#pragma once

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define arraysize(a) (sizeof(a)/sizeof(a[0]))

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
			exit(1);
		}
		return ppBuffer;
	}
};