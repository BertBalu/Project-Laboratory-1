#include "Common.hlsl"

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gImage : register(u1);
RWTexture2D<float4> gGradX : register(u2);
RWTexture2D<float4> gGradY : register(u3);
RWTexture2D<float4> gRecon : register(u4);

cbuffer CameraParams : register(b0)
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewI;
    float4x4 projectionI;
}

cbuffer FrameParams : register(b1)
{
    uint framesCount;
}

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);


float Random(float2 co)
{
    return 0.5 - (frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453));
}

float2 InitRandom(float2 launchIndex)
{
    float randLeft = Random(launchIndex.xy + float(framesCount * 1.21568));
    float randRight = Random(launchIndex.yx + float(framesCount * 4.68416));
    float2 rnd = float2(randLeft, randRight);
    
    return float2(Random(rnd.xy), Random(rnd.yx));
}

float2 GetD(float2 offset = float2(0, 0))
{
    uint2 launchIndex = DispatchRaysIndex().xy + offset;
    float2 dims = float2(DispatchRaysDimensions().xy);
    return (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
}

float4 Generate(float2 state, float2 d)
{
    HitInfo payload;
    payload.color = float4(0, 0, 0, 1);
    payload.depth = 1;
    payload.seed = state;
            
    RayDesc ray;
    ray.Origin = mul(viewI, float4(0, 0, 0, 1)).xyz;
    
    float4 target = mul(projectionI, float4(d.x, -d.y, 1, 1));
    ray.Direction = mul(viewI, float4(target.xyz, 0)).xyz;
	
    ray.TMin = 0;
    ray.TMax = 100000;

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    return payload.color;
}

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 rnd = InitRandom(launchIndex);
    float nextFrameCount = float(framesCount + 1);
    
    if (framesCount == 1)
    {
        gOutput[launchIndex] = gImage[launchIndex] = gGradX[launchIndex] = gGradY[launchIndex] = float4(0, 0, 0, 1);
    }
    
    float2 d = GetD();
    float4 c = Generate(rnd, d);
    float4 prevC = gImage[launchIndex] * framesCount;
    gImage[launchIndex] = (c + prevC) / nextFrameCount;
    
    
    float4 prevGx = gGradX[launchIndex] * framesCount;
    float4 prevGy = gGradY[launchIndex] * framesCount;

    float2 up = float2(0, -1);
    float2 dup = GetD(up);
    float4 cup = Generate(rnd, dup);
    
    float2 down = float2(0, 1);
    float2 ddown = GetD(down);
    float4 cdown = Generate(rnd, ddown);
 
    float2 left = float2(-1, 0);
    float2 dleft = GetD(left);
    float4 cleft = Generate(rnd, dleft);

    float2 right = float2(1, 0);
    float2 dright = GetD(right);
    float4 cright = Generate(rnd, dright);
    
    gGradX[launchIndex] = (prevGx + (abs(cright - cleft) / 2)) / nextFrameCount;
    gGradY[launchIndex] = (prevGx + (abs(cdown - cup) / 2)) / nextFrameCount;
    
    gOutput[launchIndex] = gImage[launchIndex];

}


