#pragma once

class MeshObject
{
public:
	GW::MATH::GMATRIXF worldMatrix = GW::MATH::GIdentityMatrixF;

	// what we need at a minimum to draw a triangle
	D3D12_VERTEX_BUFFER_VIEW					vertexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		vertexBuffer;
	// TODO: Part 1g
	D3D12_INDEX_BUFFER_VIEW						indexView;
	Microsoft::WRL::ComPtr<ID3D12Resource>		indexBuffer;

};