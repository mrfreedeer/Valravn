#pragma once
#include "Engine/Core/Buffer.hpp"

class VertexBuffer : public Buffer
{
	friend class Renderer;
public:
	virtual ~VertexBuffer();

private:
	VertexBuffer(Renderer* owner, size_t size, size_t strideSize = 0, MemoryUsage memoryUsage = MemoryUsage::Dynamic, void const* data = nullptr);
	VertexBuffer(const VertexBuffer& copy) = delete;
};
