#include "Common.hlsl"

struct STriVertex
{
    float4 vertex;
    float4 normal;
};

StructuredBuffer<STriVertex> BTriVertex : register(t0);
StructuredBuffer<int> indices : register(t1);
RaytracingAccelerationStructure SceneBVH : register(t2);

cbuffer Material : register(b0)
{
    float4 color;
    float4 emission;
    float type;
}

cbuffer Light : register(b1)
{
    float4 position;
    float4 box;
    float4 power;
}

static const float PI = 3.1415926535f;


static float NEnv = 1.0f;
static float NObj = 1.0f;

float Random(float2 co)
{
    return (frac(sin(dot(co.xy, float2(4.9898, 267.2433))) * 47321.5453));
}

float3 CastRays(float3 origin, float3 dir, float depth, float2 seed)
{
    HitInfo payload;
    payload.color = float4(0, 0, 0, 1);
    payload.depth = depth;
    payload.seed = float2(Random(seed.xy + 1.174), Random(seed.yx + 871.15));
    
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0;
    ray.TMax = 100000;

    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    return payload.color.rgb;
}

float Max(float3 vect)
{
    return max(max(vect.x, vect.y), vect.z);
}

float3 RandomPointOnSphere(float2 seed)
{
    float r1 = Random(seed.xy + 1.24554), r2 = Random(seed.yx + 2.25544), r3 = Random(float2(r1 + 2.47146, r2 + 3.74865));
    
    float theta = 2 * PI * r1;
    float v = r2;
    float phi = acos((2 * v) - 1);
    float r = pow(r3, 1 / 3);
    
    return float3(r * sin(phi) * cos(theta), r * sin(phi) * sin(theta), r * cos(phi));
}

float3 DirectLight(float3 hit, float3 normal, float2 seed)
{
    ShadowHitInfo spayload;
    spayload.isHit = spayload.isLightHit = false;
    
    float3 lightPos = position.xyz + RandomPointOnSphere(seed) * sqrt(box.x * box.x + box.y * box.y + box.z * box.z);
        
    RayDesc ray;
    ray.Origin = hit + 0.001f * normal;
    ray.Direction = normalize(lightPos - hit);
    ray.TMin = 0;
    ray.TMax = 100000;
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, spayload);
    
    if (spayload.isLightHit)
    {
        float cosTheta = max(dot(normalize(lightPos - hit), normalize(normal)), 0.0f);
        float dist = length(lightPos - hit);
        dist *= dist;
        
        if (dist < 0.0001f)
            dist = 0.0001f;
        
        return color * cosTheta * (power / dist);
    }
}

[shader("closesthit")]
void ObjectClosestHit(inout HitInfo payload, Attributes attrib)
{
    float depth = payload.depth;
    if (depth >= 10)
    {
        payload.color = float4(0, 0, 0, 1);
        return;
    }
    
    float3 rayDir = normalize(WorldRayDirection());
    float3 rayOrigin = WorldRayOrigin();
    float3 hitLocation = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        
    float3 barycentrics = float3(1.0f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    uint vertID = PrimitiveIndex() * 3;
    float3 vertex = (BTriVertex[indices[vertID + 0]].vertex.xyz * barycentrics.x + BTriVertex[indices[vertID + 1]].vertex.xyz * barycentrics.y + BTriVertex[indices[vertID + 2]].vertex.xyz * barycentrics.z).xyz;
    float3 normal = (BTriVertex[indices[vertID + 0]].normal.xyz * barycentrics.x + BTriVertex[indices[vertID + 1]].normal.xyz * barycentrics.y + BTriVertex[indices[vertID + 2]].normal.xyz * barycentrics.z).xyz;
      
    normal = normalize(normal);
    float3 normalCorrected = dot(rayDir, normal) < 0 ? normal : normal * -1;
    
    float3 calculatedColor = float3(0, 0, 0);
    
    
    if (type == 0)
    {
        calculatedColor = DirectLight(hitLocation, normalCorrected, payload.seed * 2.78946);

        float3 random = RandomPointOnSphere(payload.seed.yx + 2.8754);
        
        float3 v = normalCorrected;
        float3 u = cross(v, float3(1, 0, 0));
        if (length(u) < 0.1f)
            u = cross(v, float3(0, 0, 1));
        u = normalize(u);
        float3 w = cross(u, v);
        
        float3x3 mat = transpose(float3x3(u, v, w));
        float3 dir = mul(random, mat);
        
        
        float p = (1 / (2 * PI));
        float cos = dot(dir, normalCorrected);
        
        float3 radiance = CastRays(hitLocation + normalCorrected * 0.01f, dir, depth + 1, payload.seed + 4.4879) * color.rgb * cos * p;
        
        
        calculatedColor += emission.rgb + radiance;
    }
    
    if (type == 1)
    {
        float3 reflectDir = rayDir - (normalCorrected * dot(normalCorrected, rayDir) * 2.0f);
        calculatedColor += (emission.rgb + color.rgb * CastRays(hitLocation + 0.01f * normalCorrected, reflectDir, depth + 1, payload.seed + 1.7894));
    }
    
    /*if (type == 2)
    {
        float3 reflectDirection = rayDir - normal * 2 * dot(normal, rayDir);
        bool into = dot(normal, normalCorrected) > 0;

    
        float3 N = into ? NObj / NEnv : NEnv / NObj;
        float3 N_1 = 1 / N;
        float dotN = dot(rayDir, normalCorrected);
                   
        float cos2t = 1 - N_1.x * N_1.x * (1 - dotN * dotN);
        
        
        bool totalReflect = cos2t <= 0;
        if (totalReflect)
            calculatedColor += (emission.rgb + color.rgb * CastRays(hitLocation + normalCorrected * 0.01f, reflectDirection, depth + 1, payload.seed * 7.6942)).xyz;
        else
        {
            float3 inDir = normalize(rayDir * N_1 + normal * ((into ? 1 : -1) * (dotN * N_1 + sqrt(cos2t))));
            
            float a = NObj - NEnv;
            float b = NObj + NEnv;
            float F0 = a * a / (b * b);
            float c = 1 - (into ? (-dotN) : dot(inDir, normal));
            float Fe = F0 + (1 - F0) * pow(c, 5); // Fresnel
            
            float Tr = 1 - Fe, P = 0.25f + 0.5f * Fe, Rp = Fe / P, Tp = Tr / (1 - P); // Tr 

            float rnd = Random(payload.seed);
            if (rnd < P)
            {
                calculatedColor += (emission.rgb + color.rgb * CastRays(hitLocation + normalCorrected * 0.001f, reflectDirection, depth + 1, payload.seed * 98.7845) * Rp).rgb;
            }
            else
            {
                calculatedColor += (emission.rgb + color.rgb * CastRays(hitLocation + normalCorrected * 0.001f, inDir, depth + 1, payload.seed * 3.4324) * Tp).rgb;
            }
        }
    }
    */
    if (type == 3)
    {
        calculatedColor += emission.rgb;
    }
    
    payload.color = float4(calculatedColor, 1);
}
