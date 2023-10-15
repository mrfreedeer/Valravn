#include "Engine/Renderer/D3D12/D3D12TypeConversions.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

DXGI_FORMAT LocalToD3D12(TextureFormat textureFormat)
{
	switch (textureFormat) {
	case TextureFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case TextureFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case TextureFormat::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
	case TextureFormat::R24G8_TYPELESS: return DXGI_FORMAT_R24G8_TYPELESS;
	case TextureFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
	default: ERROR_AND_DIE("Unsupported format");
	}
}

D3D12_RESOURCE_FLAGS LocalToD3D12(ResourceBindFlag flags)
{
	D3D12_RESOURCE_FLAGS result = D3D12_RESOURCE_FLAG_NONE;

	if (flags & ResourceBindFlagBit::RESOURCE_BIND_SHADER_RESOURCE_BIT) {
		result |= D3D12_RESOURCE_FLAG_NONE;

	}

	if (flags & ResourceBindFlagBit::RESOURCE_BIND_RENDER_TARGET_BIT) {
		result |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	}

	if (flags & ResourceBindFlagBit::RESOURCE_BIND_DEPTH_STENCIL_BIT) {
		result |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if (flags & ResourceBindFlagBit::RESOURCE_BIND_UNORDERED_ACCESS_VIEW_BIT) {
		result |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	return result;
}


D3D12_DESCRIPTOR_HEAP_TYPE LocalToD3D12(DescriptorHeapType dHeapType)
{
	switch (dHeapType)
	{
	case DescriptorHeapType::SRV_UAV_CBV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case DescriptorHeapType::RTV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case DescriptorHeapType::SAMPLER:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	case DescriptorHeapType::DSV:
		return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	}
	ERROR_AND_DIE("UNKNOWN DESCRIPTOR HEAP TYPE");
}
