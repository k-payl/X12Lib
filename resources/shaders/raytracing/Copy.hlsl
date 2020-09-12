
Texture2D<float4> tex_in : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint w, h;
	tex_in.GetDimensions(w, h);
	
	if (dispatchThreadId.x > w || dispatchThreadId.y > h)
		return;
	
	gOutput[dispatchThreadId.xy] = float4(tex_in.Load(int3(dispatchThreadId.xy, 0)).rgb, 1);
}