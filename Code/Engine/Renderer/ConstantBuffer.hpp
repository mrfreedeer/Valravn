#pragma once
#include "Engine/Core/Buffer.hpp"

class ResourceView;

class ConstantBuffer : Buffer
{
	friend class Renderer;

public:
	ConstantBuffer(Renderer* owner, size_t size, size_t strideSize = 0, MemoryUsage memoryUsage = MemoryUsage::Dynamic, void const* data = nullptr);
	~ConstantBuffer();
	ResourceView* GetOrCreateView();

private:
	void CreateDefaultBuffer(void const* data) override {UNUSED(data)};
private:
	ResourceView* m_bufferView = nullptr;
};
