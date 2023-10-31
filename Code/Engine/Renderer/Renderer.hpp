#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Renderer/D3D12/DescriptorHeap.hpp"
#include "Engine/Renderer/ResourceView.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"
#include "Game/EngineBuildPreferences.hpp"
#include <filesystem>
#include <cstdint>
#include <vector>
#include <map>
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



struct ShaderByteCode;
struct ShaderLoadInfo;

class Window;
class Material;
class VertexBuffer;
struct Rgba8;
struct Vertex_PCU;
class Texture;
struct TextureCreateInfo;
class Image;
class Camera;
class ConstantBuffer;

struct RendererConfig {
	Window* m_window = nullptr;
	unsigned int m_backBuffersCount = 2;
};

struct ModelConstants {
	Mat44 ModelMatrix = Mat44();
	float ModelColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float ModelPadding[4];
};

struct CameraConstants {
	Mat44 ProjectionMatrix;
	Mat44 ViewMatrix;
	Mat44 InvertedMatrix;
};



struct ImmediateContext {
	CameraConstants m_cameraConstants = {};
	ModelConstants m_modelConstants = {};
	std::map<unsigned int, Texture*> m_boundTextures;
	std::map<unsigned int, ConstantBuffer*> m_boundCBuffers;
	VertexBuffer* m_immediateBuffer = nullptr;
	Material* m_material = nullptr;
	Texture* m_renderTargets[8] = {};
	Texture* m_depthTarget = nullptr;
	unsigned int m_srvHandleStart = 0;
	unsigned int m_cbvHandleStart = 0;
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

/// <summary>
/// Object that reports all live objects
/// NEEDS TO BE THE LAST DESTROYED THING, OTHERWISE IT REPORTS FALSE POSITIVES
/// </summary>
struct LiveObjectReporter {
	~LiveObjectReporter();
};

class Renderer {
	friend class Buffer;
	friend class Texture;
	friend class DescriptorHeap;
public:
	Renderer(RendererConfig const& config);
	~Renderer();
	void Startup();
	void BeginFrame();
	void EndFrame();
	void Shutdown();

	void BeginCamera(Camera const& camera);
	void EndCamera(Camera const& camera);

	void ClearScreen(Rgba8 const& color);
	Material* CreateOrGetMaterial(std::filesystem::path materialPathNoExt);
	Texture* CreateOrGetTextureFromFile(char const* imageFilePath);
	void DrawVertexArray(unsigned int numVertexes, const Vertex_PCU* vertexes);
	void DrawVertexArray(std::vector<Vertex_PCU> const& vertexes);
	void SetModelMatrix(Mat44 const& modelMat);
	void SetModelColor(Rgba8 const& modelColor);
	void ExecuteCommandLists(ID3D12CommandList** commandLists, unsigned int count);
	void WaitForGPU();
	DescriptorHeap* GetDescriptorHeap(DescriptorHeapType descriptorHeapType) const;
	DescriptorHeap* GetGPUDescriptorHeap(DescriptorHeapType descriptorHeapType) const;
	ResourceView* CreateResourceView(ResourceViewInfo const& resourceViewInfo) const;

	// Binds
	void BindConstantBuffer(ConstantBuffer* cBuffer, unsigned int slot = 0);
	void BindTexture(Texture* texture, unsigned int slot = 0);

	void BindMaterial(Material* mat);
	void SetMaterialPSO(Material* mat);

	// General
	void SetDebugName(ID3D12Object* object, char const* name);
	template<typename T_Object>
	void SetDebugName(ComPtr<T_Object> object, char const* name);

private:

	// DX12 Initialization & Render Initialization
	void EnableDebugLayer() const;
	void CreateDXGIFactory();
	ComPtr<IDXGIAdapter4> GetAdapter();
	void CreateDevice(ComPtr<IDXGIAdapter4> adapter);
	void CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	bool HasTearingSupport();
	void CreateSwapChain();
	void CreateDescriptorHeap(ID3D12DescriptorHeap*& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors, bool visibleFromGPU = false);
	void CreateRenderTargetViewsForBackBuffers();
	void CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>& commandAllocator);
	void CreateCommandList(ComPtr<ID3D12GraphicsCommandList2>& commList, D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> const& commandAllocator);
	void CreateFence();
	void CreateFenceEvent();
	void CreateDefaultRootSignature();
	void CreateDefaultTextureTargets();

	// Fence signaling
	unsigned int SignalFence(ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int& fenceValue);
	void WaitForFenceValue(ComPtr<ID3D12Fence1>& fence, unsigned int fenceValue, HANDLE fenceEvent);
	void Flush(ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int* fenceValues, HANDLE fenceEvent);

	Texture* GetActiveColorTarget() const;
	Texture* GetBackUpColorTarget() const;


	// Shaders & Resources
	bool CreateInputLayoutFromVS(std::vector<uint8_t>& shaderByteCode, std::vector<D3D12_SIGNATURE_PARAMETER_DESC>& elementsDescs);
	bool CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, char const* source, ShaderLoadInfo const& loadInfo);

	Material* CreateMaterial(std::string const& materialXMLFile);
	void CreatePSOForMaterial(Material* material);
	ShaderByteCode* CompileOrGetShaderBytes(ShaderLoadInfo const& shaderLoadInfo);
	ShaderByteCode* GetByteCodeForShaderSrc(ShaderLoadInfo const& shaderLoadInfo);
	Material* GetMaterialForName(char const* materialName);
	void CreateViewport();	
	Texture* CreateTexture(TextureCreateInfo& creationInfo);
	void DestroyTexture(Texture* textureToDestroy);
	Texture* GetTextureForFileName(char const* imageFilePath);
	Texture* CreateTextureFromFile(char const* imageFilePath);
	Texture* CreateTextureFromImage(Image const& image);
	ResourceView* CreateShaderResourceView(ResourceViewInfo const& viewInfo) const;
	ResourceView* CreateRenderTargetView(ResourceViewInfo const& viewInfo) const;
	ResourceView* CreateDepthStencilView(ResourceViewInfo const& viewInfo) const;
	ResourceView* CreateConstantBufferView(ResourceViewInfo const& viewInfo) const;
	void SetSamplerMode(SamplerMode samplerMode);
	void SetBlendMode(BlendMode blendMode, D3D12_BLEND_DESC& blendDesc);

	void ResetGPUDescriptorHeaps();
	void CopyTextureToHeap(Texture* textureToBind, unsigned int handleStart, unsigned int slot = 0);
	void CopyCBufferToHeap(ConstantBuffer* bufferToBind, unsigned int handleStart, unsigned int slot = 0);

	void DrawAllImmediateContexts();
	void ClearAllImmediateContexts();
	void DrawImmediateCtx(ImmediateContext& ctx);
	ComPtr<ID3D12GraphicsCommandList2> GetBufferCommandList();
private:
	RendererConfig m_config = {};
	// This object must be first ALWAYS!!!!!
	LiveObjectReporter m_liveObjectReporter;

	ComPtr<ID3D12Device2> m_device;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain4> m_swapChain;
	std::vector<Texture*> m_backBuffers;
	Texture* m_defaultRenderTarget = nullptr;
	Texture* m_defaultDepthTarget = nullptr;
	Texture* m_defaultTexture = nullptr;
	ComPtr<ID3D12GraphicsCommandList2> m_commandList;
	/// <summary>
	/// Command list dedicated to immediate need for whatever buffer related purposes
	/// </summary>
	ComPtr<ID3D12GraphicsCommandList2> m_ResourcesCommandList;
	std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
	std::vector<ID3D12Resource*> m_frameUploadHeaps;
	//ID3D12DescriptorHeap* m_RTVdescriptorHeap;
	std::vector<DescriptorHeap*> m_defaultDescriptorHeaps;
	std::vector<DescriptorHeap*> m_defaultGPUDescriptorHeaps;
	ComPtr<ID3D12Fence1> m_fence;
	ComPtr<IDXGIFactory4> m_dxgiFactory;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	std::vector<ShaderByteCode*> m_shaderByteCodes;
	std::vector<Material*> m_loadedMaterials;
	std::vector<ImmediateContext> m_immediateCtxs;
	std::vector<Texture*> m_loadedTextures;
	Material* m_defaultMaterial = nullptr;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	std::vector<unsigned int> m_fenceValues;
	HANDLE m_fenceEvent;
	bool m_useWARP = false;
	bool m_uploadRequested = false;
	unsigned int m_currentBackBuffer = 0;
	unsigned int m_antiAliasingLevel = 0;
	unsigned int m_currentFrame = 0;
	unsigned int m_RTVdescriptorSize = 0;

	ConstantBuffer* m_cameraCBO = nullptr;
	ConstantBuffer* m_modelCBO = nullptr;
	Camera const* m_currentCamera = nullptr;

	ImmediateContext m_currentDrawCtx = {};
	unsigned int m_srvHandleStart = 0;
	unsigned int m_cbvHandleStart = 0;
};

template<typename T_Object>
void Renderer::SetDebugName(ComPtr<T_Object> object, char const* name)
{
	if(!object) return;
#if defined(ENGINE_DEBUG_RENDER)
	object->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
	//object->SetName(name);
#else
	UNUSED(object);
	UNUSED(name);
#endif
}

