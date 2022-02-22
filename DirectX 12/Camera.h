#pragma once
#include "Helpers.h"

class Camera {
public:
	struct CameraData {
		GW::MATH::GVECTORF cameraLocation = GW::MATH::GIdentityVectorF;
		GW::MATH::GVECTORF cameraDirection = GW::MATH::GIdentityVectorF;
		GW::MATH::GMATRIXF viewMatrix = GW::MATH::GIdentityMatrixF;
		GW::MATH::GMATRIXF projectionMatrix = GW::MATH::GIdentityMatrixF;
	};
	Camera() {

	}

	CameraData camData;
	ID3D12Resource* viewCB = nullptr;
	UINT8* transferMemoryLocation;

	GW::MATH::GVECTORF m_eye;
	GW::MATH::GVECTORF m_at;
	GW::MATH::GVECTORF m_up;

	void InitCamera(ID3D12Device5* device) {
		CreateView(device);
	}

	void CreateView(ID3D12Device5* device)
	{
		viewCB = Helpers::CreateConstantBuffer(device, &viewCB, align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(CameraData)));
		viewCB->SetName(L"Camera View Constant Buffer");

		D3D12_RESOURCE_DESC temp = viewCB->GetDesc();
		HRESULT hr = viewCB->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));

		memcpy(transferMemoryLocation, &camData, sizeof(camData));
	}

};