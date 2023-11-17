#pragma once
#include "Engine/Core/Buffer.hpp"

class ResourceView;

class ConstantBuffer : public Buffer
{
	friend class Renderer;

public:
	ConstantBuffer(BufferDesc const& bufferDesc);
	~ConstantBuffer();
	ResourceView* GetOrCreateView();

private:
	void CreateDefaultBuffer(void const* data) override {UNUSED(data)};
private:
	ResourceView* m_bufferView = nullptr;
};
