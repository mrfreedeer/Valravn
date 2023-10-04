#include "Engine/Core/Buffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"
#include <d3d12.h>
#include <d3dx12.h> // Notice the X. These are the helper structures not the DX12 header

Buffer::Buffer(Renderer* owner, size_t size, size_t strideSize /*= 0*/, MemoryUsage memoryUsage /*= MemoryUsage::Dynamic*/, void const* data /*= nullptr*/) :
	m_owner(owner),
	m_size(size),
	m_stride(strideSize)
{
	switch (memoryUsage)
	{
	case MemoryUsage::Default:
		CreateDefaultBuffer(data);
		break;
	case MemoryUsage::Dynamic:
		CreateDynamicBuffer(data);
		break;
	default:
		break;
	}
}

bool Buffer::GuaranteeBufferSize(size_t newsize)
{
	if (m_size >= newsize) return false;

	DX_SAFE_RELEASE(m_buffer);
	m_size = newsize;
	CreateDynamicBuffer(nullptr);

	return true;
}

BufferView Buffer::GetBufferView() const
{
	BufferView bView = {
		m_buffer->GetGPUVirtualAddress(),
		m_size,
		m_stride
	};

	return bView;
}

Buffer::~Buffer()
{
	DX_SAFE_RELEASE(m_buffer);
}

void Buffer::CopyCPUToGPU(void const* data, size_t sizeInBytes)
{
	ComPtr<ID3D12Device2>& device = m_owner->m_device;
	ComPtr<ID3D12Resource1> uploadBuffer;

	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)), "COULD NOT CREATE COMMITED VERTEX BUFFER RESOURCE");

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)), "COULD NOT MAP VERTEX BUFFER");
	memcpy(pVertexDataBegin, data, sizeof(sizeInBytes));
	uploadBuffer->Unmap(0, nullptr);
}

void Buffer::CreateDynamicBuffer(void const* data)
{
	CreateAndCopyToUploadBuffer(m_buffer, data);
}

void Buffer::CreateDefaultBuffer(void const* data)
{
	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_size);
	ThrowIfFailed(m_owner->m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_buffer)), "COULD NOT CREATE GPU BUFFER");
	m_owner->SetDebugName(m_buffer, "BUFFER");

	ID3D12Resource1* intermediateBuffer = nullptr;
	CreateAndCopyToUploadBuffer(intermediateBuffer, data);
	ComPtr<ID3D12GraphicsCommandList2> bufferCommList = m_owner->GetBufferCommandList();
	ID3D12CommandList* ppCommandList[] = { bufferCommList.Get() };
	// Copy to final buffer destination
	bufferCommList->CopyBufferRegion(m_buffer, 0, intermediateBuffer, 0, m_size);
	m_owner->ExecuteCommandLists(ppCommandList, 1);
	DX_SAFE_RELEASE(intermediateBuffer);
}

void Buffer::CreateAndCopyToUploadBuffer(ID3D12Resource1*& uploadBuffer, void const* data)
{
	ComPtr<ID3D12Device2>& device = m_owner->m_device;

	CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_size);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadBuffer)), "COULD NOT CREATE COMMITED VERTEX BUFFER RESOURCE");

	if (data) {
		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, m_size);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)), "COULD NOT MAP VERTEX BUFFER");
		memcpy(pVertexDataBegin, data, m_size);
		uploadBuffer->Unmap(0, nullptr);

		m_owner->WaitForGPU();
	}
}
