//#pragma pack_matrix(row_major)

struct OBJ_ATTRIBUTES
{
    float3 Kd; // diffuse reflectivity
    float d; // dissolve (transparency) 
    float3 Ks; // specular reflectivity
    float Ns; // specular exponent
    float3 Ka; // ambient reflectivity
    float sharpness; // local reflection map sharpness
    float3 Tf; // transmission filter
    float Ni; // optical density (index of refraction)
    float3 Ke; // emissive reflectivity
	uint illum; // illumination model
};

struct SCENE_DATA
{
    float4 sunDir, sunColor, sunAmbient, camPos; // 24
    matrix viewMatrix, projectionMatrix; // 32
    float4 padding[4]; // 72
};

struct MESH_DATA
{
    matrix world; // 12
    OBJ_ATTRIBUTES material;
	uint padding[28]; // 28
};

struct OUTPUT_TO_RASTERIZER
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Tex : TEXCOORD;
    float3 Norm : NORMAL;
};

ConstantBuffer<SCENE_DATA> cameraAndLights : register(b0, space0);
ConstantBuffer<MESH_DATA> meshInfo : register(b1, space0);

OUTPUT_TO_RASTERIZER main(VS_INPUT inputVertex)
{
    OUTPUT_TO_RASTERIZER returnVal;
    returnVal.posH = float4(inputVertex.Pos, 1.0f);
    returnVal.posH = mul(meshInfo.world, returnVal.posH);
    returnVal.posH = mul(cameraAndLights.viewMatrix, returnVal.posH);
    returnVal.posH = mul(cameraAndLights.projectionMatrix, returnVal.posH);

    returnVal.nrmW = mul(meshInfo.world, float4(inputVertex.Norm, 1.0f));

    returnVal.posW = mul(meshInfo.world, float4(inputVertex.Pos, 1.0f));

    return returnVal;
}