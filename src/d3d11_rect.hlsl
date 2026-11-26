cbuffer RectColor : register(b0) {
    float4 color;
};

float4 vertex_main(uint vertex : SV_VertexID) : SV_Position
{
    float2 corner = float2((vertex << 1) & 2, vertex & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}

float4 pixel_main() : SV_Target
{
    return color;
}