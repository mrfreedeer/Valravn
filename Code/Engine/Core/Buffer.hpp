#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include <stdint.h>

class Resource;
class Renderer;
struct ID3D12Resource2;

struct BufferView {
	size_t m_bufferLocation;
	size_t m_sizeInBytes;
	size_t m_strideInBytes;
};

class Buffer {
	friend class Renderer;
public:
	Buffer(Renderer* owner, size_t size, size_t strideSize = 0, MemoryUsage memoryUsage = MemoryUsage::Dynamic, void const* data = nullptr);

	/// <summary>
	/// Only works for dynamic buffers
	/// </summary>
	/// <param name="newsize"></param>
	/// <returns></returns>
	bool GuaranteeBufferSize(size_t newsize);
	size_t GetStride() const { return m_stride; }

	BufferView GetBufferView() const;
	~Buffer();

	void CopyCPUToGPU(void const* data, size_t sizeInBytes);
protected:
	virtual void CreateDynamicBuffer(void const* data);
	virtual void CreateDefaultBuffer(void const* data);
	void CreateAndCopyToUploadBuffer(ID3D12Resource2*& uploadBuffer, void const* data);

protected:
	Renderer* m_owner = nullptr;
	Resource* m_buffer = nullptr;
	size_t m_size = 0;
	size_t m_stride = 0;
};

