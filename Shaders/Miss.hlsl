#include "Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    payload.color = float4(0, 0, 0, 1);
    
    //uint2 launchIndex = DispatchRaysIndex( ).xy;
    //float2 dims = float2(DispatchRaysDimensions( ).xy);

    //float ramp = launchIndex.y / dims.y;
    
    
    //payload.color = float4(0.8f - 0.3f * ramp, 0.2f, 0.2f, -1.0f);
}