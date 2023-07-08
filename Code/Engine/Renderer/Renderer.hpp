#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include <filesystem>
#include <cstdint>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>


#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxguid.lib")

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#define DX_SAFE_RELEASE(dxObject)			\
{											\
	if (( dxObject) != nullptr)				\
	{										\
		(dxObject)->Release();				\
		(dxObject) = nullptr;				\
	}										\
}


class Window;
class Shader;
struct Rgba8;
struct Vertex_PCU;

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

	void DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes) const;
	void DrawVertexArray(std::vector<Vertex_PCU> const& vertexes) const;
	void SetModelMatrix(Mat44 const& modelMat);
	void SetModelColor(Rgba8 const& modelColor);
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
	void CreateDefaultRootSignature();

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
	void CreateViewport();
	// General
	void SetDebugName(ID3D12Object* object, char const* name);

	void PopulateCommandList();
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
	ComPtr<ID3D12PipelineState> m_pipelineState;

	std::vector<Shader*> m_loadedShaders;
	Shader* m_defaultShader = nullptr;
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	unsigned int m_currentBackBuffer = 0;
	std::vector<unsigned int> m_fenceValues;
	unsigned int m_RTVdescriptorSize = 0;
	HANDLE m_fenceEvent;
	bool m_useWARP = false;
	unsigned int m_antiAliasingLevel = 0;
};