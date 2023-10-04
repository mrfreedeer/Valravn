#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"



VertexBuffer::VertexBuffer(Renderer* owner, size_t size, size_t strideSize /*= 0*/, MemoryUsage memoryUsage /*= MemoryUsage::Dynamic*/, void const* data /*= nullptr*/):
	Buffer(owner, size, strideSize, memoryUsage, data)
{

}

VertexBuffer::~VertexBuffer()
{
}

