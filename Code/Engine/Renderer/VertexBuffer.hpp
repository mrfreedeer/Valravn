#pragma once
#include "Engine/Core/Buffer.hpp"

class VertexBuffer : public Buffer
{
	friend class Renderer;
public:
	virtual ~VertexBuffer();
	
private:
	VertexBuffer(BufferDesc const& bufferDesc);
	VertexBuffer(const VertexBuffer& copy) = delete;
};
