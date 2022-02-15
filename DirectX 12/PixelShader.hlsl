// an ultra simple hlsl pixel shader
// TODO: Part 2b
// TODO: Part 4f
// TODO: Part 4b

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

struct OUTPUT_TO_RASTERIZER
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
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

ConstantBuffer<SCENE_DATA> cameraAndLights : register(b0, space0);
ConstantBuffer<MESH_DATA> meshInfo : register(b1, space0);

float4 main(OUTPUT_TO_RASTERIZER inputPixel) : SV_TARGET
{
    float3 V = normalize(cameraAndLights.camPos - inputPixel.posW);
    float3 refl = reflect(normalize(cameraAndLights.sunDir), normalize(inputPixel.nrmW));
    float4 color = 1.0f * cameraAndLights.sunColor * pow(saturate(dot(refl, V)), max(meshInfo.material.Ns, 0.00001f));
    color += float4(meshInfo.material.Kd, 1.0f) * cameraAndLights.sunColor * clamp(saturate(dot(-cameraAndLights.sunDir, float4(inputPixel.nrmW, 1.0f))) + cameraAndLights.sunAmbient, 0.0f, 1.0f);
    return saturate(color /*+ float4(reflect, 1.0f)*/);
};