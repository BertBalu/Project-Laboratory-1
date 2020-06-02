RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gImage : register(u1);
RWTexture2D<float4> gGradX : register(u2);
RWTexture2D<float4> gGradY : register(u3);
RWTexture2D<float4> gRecon : register(u4);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    for (int i = 0; i < 1280; ++i)
    {
        for (int j = 0; j < 720; ++j)
        {
            float2 index = float2(i, j);
            float4 color = gImage[index];
            
            gOutput[index] = gImage[index]; // TODO: Read OK, Write NOK
        }
    }
}