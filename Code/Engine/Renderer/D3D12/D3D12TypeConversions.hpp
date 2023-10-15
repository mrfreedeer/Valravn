#include <dxgiformat.h>
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/D3D12/DescriptorHeap.hpp"
#include <d3d12.h>

typedef unsigned int        UINT;

DXGI_FORMAT LocalToD3D12(TextureFormat textureFormat);
DXGI_FORMAT LocalToColourD3D12(TextureFormat textureFormat);
D3D12_RESOURCE_FLAGS LocalToD3D12(ResourceBindFlag flags);
D3D12_DESCRIPTOR_HEAP_TYPE LocalToD3D12(DescriptorHeapType dHeapType);
