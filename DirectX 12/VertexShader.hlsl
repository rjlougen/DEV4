//#pragma pack_matrix(row_major)

struct OBJ_ATTRIBUTES
{
    float3 Kd;
    float d;
    float3 Ks;
    float Ns;
    float3 Ka;
    float sharpness;
    float3 Tf;
    float Ni;
    float3 Ke;
    unsigned int illum;
};

struct SCENE_DATA
{
    float4 sunDir, sunColor, sunAmbient, camPos; // 24
    matrix viewMatrix, projectionMatrix; // 32
    float4 padding[4]; // 72
};

struct MESH_DATA
{
    matrix world[64];
    OBJ_ATTRIBUTES material ;
	//uint padding[28];
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

struct INSTANCE_DATA
{
    float KdX;
    float KdY;
    float KdZ;
    float Ns;
};

ConstantBuffer<SCENE_DATA> cameraAndLights : register(b0, space0);
ConstantBuffer<MESH_DATA> meshInfo : register(b1, space0);
ConstantBuffer<INSTANCE_DATA> instanceInfo : register(b2, space0);

OUTPUT_TO_RASTERIZER main(VS_INPUT inputVertex, uint instanceID : SV_InstanceID)
{
    OUTPUT_TO_RASTERIZER returnVal;
    //inputVertex.Pos.x += instanceID * 2;
    returnVal.posH = float4(inputVertex.Pos, 1.0f);
    returnVal.posH = mul(meshInfo.world[instanceID], returnVal.posH);
    returnVal.posH = mul(cameraAndLights.viewMatrix, returnVal.posH);
    returnVal.posH = mul(cameraAndLights.projectionMatrix, returnVal.posH);

    returnVal.nrmW = mul(meshInfo.world[instanceID], float4(inputVertex.Norm, 1.0f));

    returnVal.posW = mul(meshInfo.world[instanceID], float4(inputVertex.Pos, 1.0f));

    return returnVal;
}