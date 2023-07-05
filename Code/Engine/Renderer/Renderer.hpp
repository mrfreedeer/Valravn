#include "Engine/Core/Rgba8.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Vertex_PNCU.hpp"
#include "Engine/Renderer/Shader.hpp"
#include <filesystem>
#include <cstdint>
#include <vector>
#include <wrl.h>
#include <d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi1_6.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxguid.lib")

#pragma once

#define DX_SAFE_RELEASE(dxObject)			\
{											\
	if (( dxObject) != nullptr)				\
	{										\
		(dxObject)->Release();				\
		(dxObject) = nullptr;				\
	}										\
}

#undef OPAQUE

template <typename T> 
using ComPtr = Microsoft::WRL::ComPtr<T>;

enum class BlendMode
{
	ALPHA = 1,
	ADDITIVE,
	OPAQUE
};

enum class CullMode {
	NONE = 1,
	FRONT,
	BACK,
	NUM_CULL_MODES
};

enum class FillMode {
	SOLID = 1,
	WIREFRAME,
	NUM_FILL_MODES
};

enum class WindingOrder {
	CLOCKWISE = 1,
	COUNTERCLOCKWISE
};


enum class DepthTest // Transformed directly to DX11 (if standard changes, unexpected behavior might result) check when changing to > DX11
{
	NEVER = 1,
	LESS = 2,
	EQUAL = 3,
	LESSEQUAL = 4,
	GREATER = 5,
	NOTEQUAL = 6,
	GREATEREQUAL = 7,
	ALWAYS = 8,
};

enum class SamplerMode
{
	POINTCLAMP,
	POINTWRAP,
	BILINEARCLAMP,
	BILINEARWRAP,
	SHADOWMAPS,
};

enum class TopologyMode {// Transformed directly to DX12 (if standard changes, unexpected behavior might result) check when changing to > DX12
	UNDEFINED,
	POINTLIST,
	LINELIST,
	LINESTRIP,
	TRIANGLELIST,
	TRIANGLESTRIP,
	LINELIST_ADJ = 10,
	LINESTRIP_ADJ = 11,
	TRIANGLELIST_ADJ = 12,
	TRIANGLESTRIP_ADJ = 13,
	CONTROL_POINT_PATCHLIST_1 = 33,
	CONTROL_POINT_PATCHLIST_2 = 34,
	CONTROL_POINT_PATCHLIST_3 = 35,
	CONTROL_POINT_PATCHLIST_4 = 36,
	CONTROL_POINT_PATCHLIST_5 = 37,
	CONTROL_POINT_PATCHLIST_6 = 38,
	CONTROL_POINT_PATCHLIST_7 = 39,
	CONTROL_POINT_PATCHLIST_8 = 40,
	CONTROL_POINT_PATCHLIST_9 = 41,
	CONTROL_POINT_PATCHLIST_10 = 42,
	CONTROL_POINT_PATCHLIST_11 = 43,
	CONTROL_POINT_PATCHLIST_12 = 44,
	CONTROL_POINT_PATCHLIST_13 = 45,
	CONTROL_POINT_PATCHLIST_14 = 46,
	CONTROL_POINT_PATCHLIST_15 = 47,
	CONTROL_POINT_PATCHLIST_16 = 48,
	CONTROL_POINT_PATCHLIST_17 = 49,
	CONTROL_POINT_PATCHLIST_18 = 50,
	CONTROL_POINT_PATCHLIST_19 = 51,
	CONTROL_POINT_PATCHLIST_20 = 52,
	CONTROL_POINT_PATCHLIST_21 = 53,
	CONTROL_POINT_PATCHLIST_22 = 54,
	CONTROL_POINT_PATCHLIST_23 = 55,
	CONTROL_POINT_PATCHLIST_24 = 56,
	CONTROL_POINT_PATCHLIST_25 = 57,
	CONTROL_POINT_PATCHLIST_26 = 58,
	CONTROL_POINT_PATCHLIST_27 = 59,
	CONTROL_POINT_PATCHLIST_28 = 60,
	CONTROL_POINT_PATCHLIST_29 = 61,
	CONTROL_POINT_PATCHLIST_30 = 62,
	CONTROL_POINT_PATCHLIST_31 = 63,
	CONTROL_POINT_PATCHLIST_32 = 64,
};



class Window;

struct RendererConfig {
	Window* m_window = nullptr;
	unsigned int m_backBuffersCount = 2;
};


struct Light
{
	bool Enabled = false;
	Vec3 Position;
	//------------- 16 bytes
	Vec3 Direction;
	int LightType = 0; // 0 Point Light, 1 SpotLight
	//------------- 16 bytes
	float Color[4];
	//------------- 16 bytes // These are some decent default values
	float SpotAngle = 45.0f;
	float ConstantAttenuation = 0.1f;
	float LinearAttenuation = 0.2f;
	float QuadraticAttenuation = 0.5f;
	//------------- 16 bytes
	Mat44 ViewMatrix;
	//------------- 64 bytes
	Mat44 ProjectionMatrix;
	//------------- 64 bytes
};
//
//struct SODeclarationEntry {
//	std::string SemanticName = "";
//	unsigned int SemanticIndex = 0;
//	unsigned char StartComponent;
//	unsigned char ComponentCount;
//	unsigned char OutputSlot;
//};

constexpr int MAX_LIGHTS = 8;

typedef void* HANDLE;



class Renderer {
public:
	Renderer(RendererConfig const& config);
	void Startup();
	void BeginFrame();
	void EndFrame();
	void Shutdown();

	void ClearScreen(Rgba8 const& color);
	Shader* CreateOrGetShader(std::filesystem::path shaderName);

private:

	// DX12 Initialization
	void EnableDebugLayer() const;
	void CreateDXGIFactory();
	ComPtr<IDXGIAdapter4> GetAdapter();
	void CreateDevice(ComPtr<IDXGIAdapter4> adapter);
	void CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	bool HasTearingSupport();
	void CreateSwapChain();
	void CreateDescriptorHeap(ComPtr<ID3D12DescriptorHeap>& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors);
	void CreateRenderTargetViews();
	void CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>& commandAllocator);
	void CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> const& commandAllocator);
	void CreateFence();
	void CreateFenceEvent();
	void CreateRootSignature();

	// Fence signaling
	unsigned int SignalFence(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int& fenceValue);
	void WaitForFenceValue(ComPtr<ID3D12Fence1> fence, unsigned int fenceValue, HANDLE fenceEvent);
	void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int* fenceValues, HANDLE fenceEvent);

	ComPtr<ID3D12Resource2> GetActiveColorTarget() const;
	ComPtr<ID3D12Resource2> GetBackUpColorTarget() const;


	// Shaders
	bool CreateInputLayoutFromVS(std::vector<uint8_t>& shaderByteCode, D3D12_INPUT_LAYOUT_DESC& pInputLayout, std::vector<D3D12_INPUT_ELEMENT_DESC>& elementsDescs);
	bool CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, char const* name, char const* source, char const* entryPoint, char const* target, bool isAntialiasingOn);
	Shader* CreateShader(std::filesystem::path shaderName);
	Shader* CreateShader(char const* shaderName, char const* shaderSource);
	Shader* GetShaderForName(char const* shaderName);

	// General
	void SetDebugName(ID3D12Object* object, char const* name);
private:


	RendererConfig m_config = {};

	ComPtr<ID3D12Device2> m_device;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain4> m_swapChain;
	std::vector<ComPtr<ID3D12Resource2>> m_backBuffers;
	ComPtr<ID3D12GraphicsCommandList2> m_commandList;
	std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
	ComPtr<ID3D12DescriptorHeap> m_RTVdescriptorHeap;
	ComPtr<ID3D12Fence1> m_fence;
	ComPtr<IDXGIFactory4> m_dxgiFactory;

	std::vector<Shader*> m_loadedShaders;

	unsigned int m_currentBackBuffer = 0;
	unsigned int* m_fenceValues = nullptr;
	unsigned int m_RTVdescriptorSize = 0;
	HANDLE m_fenceEvent;
	bool m_useWARP = false;
	unsigned int m_antiAliasingLevel = 0;
};