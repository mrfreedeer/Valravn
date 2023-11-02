#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/D3D12/Resource.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include <d3d12.h>

ConstantBuffer::ConstantBuffer(BufferDesc const& bufferDesc) :
	Buffer(bufferDesc)
{

	bool adjustSize = (m_size & 0xFF);
	if (adjustSize) {
		m_size &= ~0xFF;
		m_size += 256;

	}
	m_stride = m_size;
	CreateDynamicBuffer(m_data);

}

ConstantBuffer::~ConstantBuffer()
{
	if (m_bufferView) {
		delete m_bufferView;
	}
}

ResourceView* ConstantBuffer::GetOrCreateView()
{
	if (m_bufferView) return m_bufferView;

	BufferView bufferV = GetBufferView();

	D3D12_CONSTANT_BUFFER_VIEW_DESC* cBufferView = new D3D12_CONSTANT_BUFFER_VIEW_DESC();
	cBufferView->BufferLocation = bufferV.m_bufferLocation;
	cBufferView->SizeInBytes = (UINT)bufferV.m_sizeInBytes;

	ResourceViewInfo bufferViewInfo = {};
	bufferViewInfo.m_cbvDesc = cBufferView;
	bufferViewInfo.m_viewType = RESOURCE_BIND_CONSTANT_BUFFER_VIEW_BIT;

	m_bufferView = m_owner->CreateResourceView(bufferViewInfo, m_descriptorHeap);

	return m_bufferView;
}
