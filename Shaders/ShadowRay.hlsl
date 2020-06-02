struct ShadowHitInfo
{
    bool isHit;
    bool isLightHit;
};

struct Attributes
{
    float2 uv;
};

cbuffer Material : register(b0)
{
    float4 color;
    float4 emission;
    float type;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
    hit.isHit = true;
    hit.isLightHit = false;
    if (type == 3)
    {
        hit.isLightHit = true;
    }
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit : SV_RayPayload)
{
    hit.isHit = false;
    hit.isLightHit = false;
}