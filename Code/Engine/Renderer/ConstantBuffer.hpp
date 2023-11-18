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
	virtual void Initialize() override;
private:
	void CreateDefaultBuffer(void const* data) override {UNUSED(data)};
private:
	ResourceView* m_bufferView = nullptr;
	bool m_initialized = false;
};
