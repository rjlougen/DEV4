#pragma once
#include "Helpers.h"
class Camera {
public:
	CameraData					cameraData;
	ID3D12Resource*				viewBuffer = nullptr;
	UINT8*						transferMemoryLocation;

	GW::MATH::GVECTORF eye;
	GW::MATH::GVECTORF at;
	GW::MATH::GVECTORF up;

	void InitCamera(ID3D12Device5* device, GW::GRAPHICS::GDirectX12Surface _d3d, GW::MATH::GMatrix GMatrix) {
		CreateViewBuffer(device, _d3d, GMatrix);
		CreateViewMatrix(device, _d3d, GMatrix);
		CreateProjectionMatrix(device, _d3d, GMatrix);
	}

	void CreateViewBuffer(ID3D12Device5* device, GW::GRAPHICS::GDirectX12Surface _d3d, GW::MATH::GMatrix GMatrix)
	{
		viewBuffer = Helpers::CreateConstantBuffer(device, &viewBuffer, align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(CameraData)));
		viewBuffer->SetName(L"Camera View Constant Buffer");

		D3D12_RESOURCE_DESC temp = viewBuffer->GetDesc();
		HRESULT hr = viewBuffer->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&transferMemoryLocation));

		// This is how you update what's in the buffer (can be used for dynamic cam stuff)
		memcpy(transferMemoryLocation, &cameraData, sizeof(cameraData));
	}

	void CreateProjectionMatrix(ID3D12Device5* device, GW::GRAPHICS::GDirectX12Surface _d3d, GW::MATH::GMatrix GMatrix) {
		GMatrix.IdentityF(cameraData.projectionMatrix);

		float NEAR_PLANE = 0.1f;
		float FAR_PLANE = 100.0f;
		float ratio = 0.0f;

		_d3d.GetAspectRatio(ratio);

		GMatrix.ProjectionDirectXLHF(1.5708f, ratio, NEAR_PLANE, FAR_PLANE, cameraData.projectionMatrix); //(float _fovY, float _aspect, float _zn, float _zf, GW::MATH::GMATRIXF& _outMatrix)
	}

	void CreateViewMatrix(ID3D12Device5* device, GW::GRAPHICS::GDirectX12Surface _d3d, GW::MATH::GMatrix GMatrix) {
		GMatrix.IdentityF(cameraData.viewMatrix);

		GW::MATH::GVECTORF Eye = { 0.75f, 0.25f, -1.5f, 1.0f };
		GW::MATH::GVECTORF At = { 0.15f, 0.75f, 0.0f, 1.0f };
		GW::MATH::GVECTORF Up = { 0.0f, 1.0f, 0.0f, 1.0f };

		GMatrix.LookAtLHF(Eye, At, Up, cameraData.viewMatrix);
	}
};