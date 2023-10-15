#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"
#include "Engine/Renderer/D3D12/D3D12TypeConversions.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <d3d12.h>


Texture::Texture(TextureCreateInfo const& createInfo) :
	m_creationInfo(createInfo),
	m_owner(createInfo.m_owner),
	m_handle(createInfo.m_handle)
{
}

void Texture::CreateShaderResourceView()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	/*
	* SHADER COMPONENT MAPPING RGBA = 0,1,2,3
	* 4 Force a value of 0
	* 5 Force a value of 1
	*/
	srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, 3);
	srvDesc.Format = LocalToD3D12(m_creationInfo.m_format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	DescriptorHeap* srvDescriptorHeap = m_owner->GetDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);


	ComPtr<ID3D12Device2> device = m_owner->m_device;
	device->CreateShaderResourceView(m_handle, &srvDesc, srvDescriptorHeap->GetNextCPUHandle());

	m_createdViews |= TextureBindFlagBit::TEXTURE_BIND_SHADER_RESOURCE_BIT;
}

void Texture::CreateRenderTargetView()
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = LocalToD3D12(m_creationInfo.m_format);

	DescriptorHeap* rtvDescriptorHeap = m_owner->GetDescriptorHeap(DescriptorHeapType::RTV);
	ComPtr<ID3D12Device2> device = m_owner->m_device;
	device->CreateRenderTargetView(m_handle, &rtvDesc, rtvDescriptorHeap->GetNextCPUHandle());
	m_createdViews |= TextureBindFlagBit::TEXTURE_BIND_RENDER_TARGET_BIT;

}

void Texture::CreateDepthStencilView()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = LocalToD3D12(m_creationInfo.m_format);
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	DescriptorHeap* dsvDescriptorHeap = m_owner->GetDescriptorHeap(DescriptorHeapType::DSV);
	ComPtr<ID3D12Device2> device = m_owner->m_device;
	device->CreateDepthStencilView(m_handle, &dsvDesc, dsvDescriptorHeap->GetNextCPUHandle());
	m_createdViews |= TextureBindFlagBit::TEXTURE_BIND_DEPTH_STENCIL_BIT;
}

void Texture::CheckOrCreateView(TextureBindFlagBit viewType)
{
	// If bit is set, view has been created
	if (viewType & m_createdViews) return;
	if (!(m_creationInfo.m_bindFlags & viewType)) {
		ERROR_RECOVERABLE(Stringf("VIEW NOT COMPATIBLE: %d", viewType));
	}

	switch (viewType)
	{
	case TEXTURE_BIND_SHADER_RESOURCE_BIT:
		CreateShaderResourceView();
		break;
	case TEXTURE_BIND_RENDER_TARGET_BIT:
		CreateRenderTargetView();
		break;
	case TEXTURE_BIND_DEPTH_STENCIL_BIT:
		CreateDepthStencilView();
		break;
		/*case TEXTURE_BIND_UNORDERED_ACCESS_VIEW_BIT:
			break;*/
	default:
		ERROR_AND_DIE(Stringf("UNSUPPORTED TEXTURE VIEW: %d", viewType));
		break;
	}
}

IntVec2 Texture::GetDimensions() const
{
	return m_creationInfo.m_dimensions;
}
