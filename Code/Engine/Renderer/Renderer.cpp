#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/DefaultShader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/Material.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/D3D12/Resource.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"
#include "Engine/Renderer/D3D12/D3D12TypeConversions.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Renderer/DebugRendererSystem.hpp"
#include <dxgidebug.h>
#include <d3dx12.h> // Notice the X. These are the helper structures not the DX12 header

#pragma message("ENGINE_DIR == " ENGINE_DIR)

bool is3DDefault = true;



DXGI_FORMAT GetFormatForComponent(D3D_REGISTER_COMPONENT_TYPE componentType, char const* semanticnName, BYTE mask) {
	// determine DXGI format
	if (mask == 1)
	{
		if (componentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32_UINT;
		else if (componentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32_SINT;
		else if (componentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32_FLOAT;
	}
	else if (mask <= 3)
	{
		if (componentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32_UINT;
		else if (componentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32_SINT;
		else if (componentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32_FLOAT;
	}
	else if (mask <= 7)
	{
		if (componentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32B32_UINT;
		else if (componentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32B32_SINT;
		else if (componentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32B32_FLOAT;
	}
	else if (mask <= 15)
	{
		if (AreStringsEqualCaseInsensitive(semanticnName, "COLOR")) {
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		else {
			if (componentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32B32A32_UINT;
			else if (componentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32B32A32_SINT;
			else if (componentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}


	}

	return DXGI_FORMAT_UNKNOWN;
}



Renderer::Renderer(RendererConfig const& config) :
	m_config(config)
{

}


Renderer::~Renderer()
{

}

void Renderer::EnableDebugLayer() const
{
#if defined(_DEBUG)
	ID3D12Debug3* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();

		debugController->Release();
	}
	else {
		ERROR_AND_DIE("COULD NOT ENABLE DX12 DEBUG LAYER");
	}

#endif
}

void Renderer::CreateDXGIFactory()
{
	UINT factoryFlags = 0;
	// Create DXGI Factory to create DXGI Objects

#if defined(_DEBUG)
	factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif


	HRESULT factoryCreation = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_dxgiFactory));

	SetDebugName(m_dxgiFactory, "DXGIFACTORY");

	if (!SUCCEEDED(factoryCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE DXGI FACTORY");
	}

}

ComPtr<IDXGIAdapter4> Renderer::GetAdapter()
{
	ComPtr<IDXGIAdapter4> adapter = nullptr;
	ComPtr<IDXGIAdapter1> adapter1;

	if (m_useWARP) {

		HRESULT enumWarpAdapter = m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));

		if (!SUCCEEDED(enumWarpAdapter)) {
			ERROR_AND_DIE("COULD NOT GET WARP ADAPTER");
		}
	}
	else {
		ComPtr<IDXGIFactory6> factory6;
		HRESULT transformedToFactory6 = m_dxgiFactory.As(&factory6);

		// Prefer dedicated GPU
		if (SUCCEEDED(transformedToFactory6)) {
			for (int adapterIndex = 0;
				factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
					IID_PPV_ARGS(adapter1.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND;
				adapterIndex++) {
				DXGI_ADAPTER_DESC1 desc;
				adapter1->GetDesc1(&desc);

				if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
					break;
				}

			}

		}
		else {
			for (int adapterIndex = 0;
				m_dxgiFactory->EnumAdapters1(adapterIndex, adapter1.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND;
				adapterIndex++) {
				DXGI_ADAPTER_DESC1 desc;
				adapter1->GetDesc1(&desc);

				if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
					break;
				}
			}
		}

		factory6.Reset();
	}

	HRESULT castToAdapter4 = adapter1.As(&adapter);
	if (!SUCCEEDED(castToAdapter4)) {
		ERROR_AND_DIE("COULD NOT CAST ADAPTER1 TO ADAPTER4");
	}

	adapter1.Reset();
	return adapter;
}



void Renderer::CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	HRESULT deviceCreationResult = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device));

	SetDebugName(m_device, "Device");
	if (!SUCCEEDED(deviceCreationResult)) {
		ERROR_AND_DIE("COULD NOT CREATE DIRECTX12 DEVICE");
	}

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(m_device.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		infoQueue.Reset();
	}
	else {
		ERROR_AND_DIE("COULD NOT SET MESSAGE SEVERITIES DX12 FOR DEBUG PURPORSES");
	}
#endif
}

void Renderer::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	HRESULT queueCreation = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue));
	SetDebugName(m_commandQueue, "COMMANDQUEUE");
	if (!SUCCEEDED(queueCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE COMMAND QUEUE");
	}
}

bool Renderer::HasTearingSupport()
{
	// Querying if there is support for Variable refresh rate 
	ComPtr<IDXGIFactory4> dxgiFactory;
	BOOL allowTearing = FALSE;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)))) {
		ComPtr<IDXGIFactory5> dxgiFactory5;
		if (SUCCEEDED(dxgiFactory.As(&dxgiFactory5))) {
			dxgiFactory.Reset();
			if (SUCCEEDED(dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
				dxgiFactory5.Reset();
				return allowTearing == TRUE;
			}
		}
	}
	else {
		ERROR_AND_DIE("COULD NOT CREATE DXGI FACTORY FOR TEARING SUPPORT");
	}

	return false;
}

void Renderer::CreateSwapChain()
{
	Window* window = Window::GetWindowContext();
	IntVec2 windowDimensions = window->GetClientDimensions();
	RECT clientRect = {};
	HWND windowHandle = (HWND)window->m_osWindowHandle;
	::GetClientRect(windowHandle, &clientRect);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = clientRect.right;
	swapChainDesc.Height = clientRect.bottom;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 }; // #ToDo Check for reenabling MSAA
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = (UINT)m_config.m_backBuffersCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	swapChainDesc.Flags = (HasTearingSupport()) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT swapChainCreationResult = m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(),
		windowHandle,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1);

	SetDebugName(swapChain1, "SwapChain1");
	if (!SUCCEEDED(swapChainCreationResult)) {
		ERROR_AND_DIE("COULD NOT CREATE SWAPCHAIN1");
	}

	if (!SUCCEEDED(swapChain1.As(&m_swapChain))) {
		ERROR_AND_DIE("COULD NOT CONVERT SWAPCHAIN1 TO SWAPCHAIN4");
	}

	swapChain1.Reset();

}

void Renderer::CreateDescriptorHeap(ID3D12DescriptorHeap*& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors, bool visibleFromGPU /*=false*/)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;
	if (visibleFromGPU) {
		desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	HRESULT descHeapCreation = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
	SetDebugName(descriptorHeap, "DescriptorHeap");

	if (!SUCCEEDED(descHeapCreation)) {
		ERROR_AND_DIE("FAILED TO CREATE DESCRIPTOR HEAP");
	}
}

void Renderer::CreateRenderTargetViewsForBackBuffers()
{
	// Handle size is vendor specific
	m_RTVdescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// Get a helper handle to the descriptor and create the RTVs 

	DescriptorHeap* rtvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::RTV);


	m_backBuffers.resize(m_config.m_backBuffersCount);
	for (unsigned int frameBufferInd = 0; frameBufferInd < m_config.m_backBuffersCount; frameBufferInd++) {
		ID3D12Resource2* bufferTex = nullptr;
		Texture*& backBuffer = m_backBuffers[frameBufferInd];
		HRESULT fetchBuffer = m_swapChain->GetBuffer(frameBufferInd, IID_PPV_ARGS(&bufferTex));
		if (!SUCCEEDED(fetchBuffer)) {
			ERROR_AND_DIE("COULD NOT GET FRAME BUFFER");
		}

		D3D12_RESOURCE_DESC bufferTexDesc = bufferTex->GetDesc();

		TextureCreateInfo backBufferTexInfo = {};
		backBufferTexInfo.m_bindFlags = ResourceBindFlagBit::RESOURCE_BIND_RENDER_TARGET_BIT;
		backBufferTexInfo.m_dimensions = IntVec2((int)bufferTexDesc.Width, (int)bufferTexDesc.Height);
		backBufferTexInfo.m_format = TextureFormat::R8G8B8A8_UNORM;
		backBufferTexInfo.m_name = "DefaultRenderTarget";
		backBufferTexInfo.m_owner = this;
		backBufferTexInfo.m_handle = new Resource();
		backBufferTexInfo.m_handle->m_resource = bufferTex;

		backBuffer = CreateTexture(backBufferTexInfo);
		DX_SAFE_RELEASE(bufferTex);
		//m_device->CreateRenderTargetView(bufferTex.Get(), nullptr, rtvHandle);
		//rtvHandle.Offset(m_RTVdescriptorSize);

	}



}

void Renderer::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>& commandAllocator)
{
	HRESULT commAllocatorCreation = m_device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));
	SetDebugName(commandAllocator, "CommandAllocator");
	if (!SUCCEEDED(commAllocatorCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE COMMAND ALLOCATOR");
	}
}

void Renderer::CreateCommandList(ComPtr<ID3D12GraphicsCommandList2>& commList, D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>const& commandAllocator)
{
	HRESULT commListCreation = m_device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commList));
	SetDebugName(commList, "COMMANDLIST");
	if (!SUCCEEDED(commListCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE COMMAND LIST");
	}
}

void Renderer::CreateFence()
{
	HRESULT fenceCreation = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	SetDebugName(m_fence, "FENCE");
	if (!SUCCEEDED(fenceCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE FENCE");
	}
}

void Renderer::CreateFenceEvent()
{
	m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Renderer::CreateDefaultRootSignature()
{
	//CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;

	/**
	 * Usual layout is 3 Constant Buffers
	 * Textures 0-8
	 * Sampler
	 * #TODO programatically define more complex root signatures. Perhaps just really on the HLSL definition?
	 */

	D3D12_DESCRIPTOR_RANGE_FLAGS cbvFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	D3D12_DESCRIPTOR_RANGE1 descriptorRanges[3] = {};
	descriptorRanges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, CBV_DESCRIPTORS_AMOUNT, 0,0, cbvFlags, 0 };
	descriptorRanges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, SRV_DESCRIPTORS_AMOUNT, 0,0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0 };
	descriptorRanges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0,0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0 };


	// Base parameters, initial at the root table. Could be a descriptor, or a pointer to descriptor 
	// In this case, one descriptor table per slot in the first 3

	CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[2].InitAsDescriptorTable(1, &descriptorRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignature(_countof(descriptorRanges), rootParameters);
	rootSignature.Desc_1_2.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignature.Desc_1_2.Flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

	//rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	//ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "COULD NOT SERIALIZE ROOT SIGNATURE");
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	HRESULT rootSignatureSerialization = D3D12SerializeVersionedRootSignature(&rootSignature, signature.GetAddressOf(), error.GetAddressOf());
	ThrowIfFailed(rootSignatureSerialization, "COULD NOT SERIALIZE ROOT SIGNATURE");
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)), "COULD NOT CREATE ROOT SIGNATURE");
	SetDebugName(m_rootSignature, "DEFAULTROOTSIGNATURE");


	//CreateDescriptorHeap(m_defaultDescriptorHeaps[1], D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, true);

	//auto descriptorsize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// Get a helper handle to the descriptor and create the RTVs 
	/*CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_defaultDescriptorHeaps[0]->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC bsDesc = {};
	bsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bsDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	bsDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	m_device->CreateShaderResourceView(nullptr, &bsDesc, descriptorHandle);
	descriptorHandle.Offset(descriptorsize);*/
}

void Renderer::CreateDefaultTextureTargets()
{
	IntVec2 texDimensions = Window::GetWindowContext()->GetClientDimensions();
	TextureCreateInfo defaultRTInfo = {};
	defaultRTInfo.m_bindFlags = ResourceBindFlagBit::RESOURCE_BIND_RENDER_TARGET_BIT | ResourceBindFlagBit::RESOURCE_BIND_SHADER_RESOURCE_BIT;
	defaultRTInfo.m_dimensions = texDimensions;
	defaultRTInfo.m_format = TextureFormat::R8G8B8A8_UNORM;
	defaultRTInfo.m_name = "DefaultRenderTarget";
	defaultRTInfo.m_owner = this;

	m_defaultRenderTarget = CreateTexture(defaultRTInfo);

	TextureCreateInfo defaultDSTInfo = {};
	defaultDSTInfo.m_bindFlags = ResourceBindFlagBit::RESOURCE_BIND_DEPTH_STENCIL_BIT | ResourceBindFlagBit::RESOURCE_BIND_SHADER_RESOURCE_BIT;
	defaultDSTInfo.m_dimensions = texDimensions;
	defaultDSTInfo.m_format = TextureFormat::R24G8_TYPELESS;
	defaultDSTInfo.m_clearFormat = TextureFormat::D24_UNORM_S8_UINT;
	defaultDSTInfo.m_name = "DefaultDepthTarget";
	defaultDSTInfo.m_owner = this;
	defaultDSTInfo.m_clearColour = Rgba8(255, 255, 255, 255);

	m_defaultDepthTarget = CreateTexture(defaultDSTInfo);
}

unsigned int Renderer::SignalFence(ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int& fenceValue)
{
	HRESULT fenceSignal = commandQueue->Signal(fence.Get(), fenceValue);

	if (!SUCCEEDED(fenceSignal)) {
		ERROR_AND_DIE("FENCE SIGNALING FAILED");
	}

	return fenceValue + 1;

}

void Renderer::WaitForFenceValue(ComPtr<ID3D12Fence1>& fence, unsigned int fenceValue, HANDLE fenceEvent)
{
	UINT64 completedValue = fence->GetCompletedValue();
	if (completedValue < fenceValue) {
		HRESULT eventOnCompletion = fence->SetEventOnCompletion(fenceValue, fenceEvent);
		if (!SUCCEEDED(eventOnCompletion)) {
			ERROR_AND_DIE("FAILED TO SET EVENT ON COMPLETION FOR FENCE");
		}
		::WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}
}

void Renderer::Flush(ComPtr<ID3D12CommandQueue>& commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int* fenceValues, HANDLE fenceEvent)
{
	unsigned int currentValue = fenceValues[m_currentBackBuffer];
	unsigned int newFenceValue = SignalFence(commandQueue, fence, currentValue);

	m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
	//unsigned int waitOnValue = m_fenceValues[m_currentBackBuffer];
	WaitForFenceValue(fence, m_fenceValues[m_currentBackBuffer], fenceEvent);

	m_fenceValues[m_currentBackBuffer] = newFenceValue;
}

Texture* Renderer::GetActiveColorTarget() const
{
	return m_backBuffers[m_currentBackBuffer];
}

Texture* Renderer::GetBackUpColorTarget() const
{
	int otherInd = (m_currentBackBuffer + 1) % 2;
	return m_backBuffers[otherInd];
}

void Renderer::Startup()
{
#if defined(GAME_2D)
	is3DDefault = false;
#endif
	m_fenceValues.resize(m_config.m_backBuffersCount);
	// Enable Debug Layer before initializing any DX12 object
	EnableDebugLayer();
	CreateViewport();
	CreateDXGIFactory();
	ComPtr<IDXGIAdapter4> adapter = GetAdapter();
	CreateDevice(adapter);
	adapter.Reset();
	CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	CreateSwapChain();

	m_defaultGPUDescriptorHeaps.resize(2);

	// These are limited by the root signature
	m_defaultGPUDescriptorHeaps[(size_t)DescriptorHeapType::SRV_UAV_CBV] = new DescriptorHeap(this, DescriptorHeapType::SRV_UAV_CBV, 2048, true);
	m_defaultGPUDescriptorHeaps[(size_t)DescriptorHeapType::SAMPLER] = new DescriptorHeap(this, DescriptorHeapType::SAMPLER, 64, true);

	m_defaultDescriptorHeaps.resize((size_t)DescriptorHeapType::NUM_DESCRIPTOR_HEAPS);

	/// <summary>
	/// Recommendation here is to have a large pool of descriptors and use them ring buffer style
	/// </summary>
	m_defaultDescriptorHeaps[(size_t)DescriptorHeapType::SRV_UAV_CBV] = new DescriptorHeap(this, DescriptorHeapType::SRV_UAV_CBV, 4096);
	m_defaultDescriptorHeaps[(size_t)DescriptorHeapType::SAMPLER] = new DescriptorHeap(this, DescriptorHeapType::SAMPLER, 64);
	m_defaultDescriptorHeaps[(size_t)DescriptorHeapType::RTV] = new DescriptorHeap(this, DescriptorHeapType::RTV, 1024);
	m_defaultDescriptorHeaps[(size_t)DescriptorHeapType::DSV] = new DescriptorHeap(this, DescriptorHeapType::DSV, 8);


	CreateRenderTargetViewsForBackBuffers();
	CreateDefaultTextureTargets();


	m_commandAllocators.resize(m_config.m_backBuffersCount + 1);
	for (unsigned int frameIndex = 0; frameIndex < m_config.m_backBuffersCount; frameIndex++) {
		CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[frameIndex]);
	}
	CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_commandAllocators.size() - 1]);
	CreateCommandList(m_ResourcesCommandList, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_commandAllocators.size() - 1]);

	CreateDefaultRootSignature();

	std::string default2DMatPath = ENGINE_MAT_DIR;
	default2DMatPath += "Default2DMaterial.xml";
	m_default2DMaterial = CreateMaterial(default2DMatPath);

	std::string default3DMatPath = ENGINE_MAT_DIR;
	default3DMatPath += "Default3DMaterial.xml";
	m_default3DMaterial = CreateMaterial(default3DMatPath);

	CreateCommandList(m_commandList, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_currentBackBuffer]);

	ThrowIfFailed(m_commandList->Close(), "COULD NOT CLOSE DEFAULT COMMAND LIST");
	ThrowIfFailed(m_ResourcesCommandList->Close(), "COULT NOT CLOSE INTERNAL BUFFER COMMAND LIST");


	CreateFence();

	m_fenceValues[m_currentBackBuffer]++;
	CreateFenceEvent();
	SetSamplerMode(SamplerMode::POINTCLAMP);

	m_ResourcesCommandList->Reset(m_commandAllocators[m_commandAllocators.size() - 1].Get(), nullptr);
	m_commandList->Reset(m_commandAllocators[m_currentBackBuffer].Get(), nullptr);

	m_defaultTexture = new Texture();
	Image whiteTexelImg(IntVec2(1, 1), Rgba8::WHITE);
	whiteTexelImg.m_imageFilePath = "DefaultTexture";
	m_defaultTexture = CreateTextureFromImage(whiteTexelImg);

	// Assuming worst case scenario, which would be all constant buffers are full and are only engine ones
	m_cameraCBOArray.resize(CBV_DESCRIPTORS_AMOUNT / 2);
	m_modelCBOArray.resize(CBV_DESCRIPTORS_AMOUNT / 2);

	BufferDesc cameraBufferDesc = {};
	cameraBufferDesc.data = nullptr;
	cameraBufferDesc.descriptorHeap = nullptr;
	cameraBufferDesc.memoryUsage = MemoryUsage::Dynamic;
	cameraBufferDesc.owner = this;
	cameraBufferDesc.size = sizeof(CameraConstants);
	cameraBufferDesc.stride = sizeof(CameraConstants);

	BufferDesc modelBufferDesc = cameraBufferDesc;
	modelBufferDesc.size = sizeof(ModelConstants);
	modelBufferDesc.stride = sizeof(ModelConstants);

	BufferDesc vertexBuffDesc = {};
	vertexBuffDesc.data = nullptr;
	vertexBuffDesc.descriptorHeap = nullptr;
	vertexBuffDesc.memoryUsage = MemoryUsage::Dynamic;
	vertexBuffDesc.owner = this;
	vertexBuffDesc.size = sizeof(Vertex_PCU);
	vertexBuffDesc.stride = sizeof(Vertex_PCU);

	m_immediateVBO = new VertexBuffer(vertexBuffDesc);

	// Allocating the memory so they're ready to use
	for (int bufferInd = 0; bufferInd < m_cameraCBOArray.size(); bufferInd++) {
		ConstantBuffer*& cameraBuffer = m_cameraCBOArray[bufferInd];
		ConstantBuffer*& modelBuffer = m_modelCBOArray[bufferInd];

		cameraBuffer = new ConstantBuffer(cameraBufferDesc);
		modelBuffer = new ConstantBuffer(modelBufferDesc);
	}

	DebugRenderConfig debugSystemConfig;
	debugSystemConfig.m_renderer = this;
	debugSystemConfig.m_startHidden = false;
	debugSystemConfig.m_fontName = "Data/Images/SquirrelFixedFont";

	DebugRenderSystemStartup(debugSystemConfig);

}


bool Renderer::CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, char const* source, ShaderLoadInfo const& loadInfo)
{
	ID3DBlob* shaderBlob;
	ID3DBlob* shaderErrorBlob;

	UINT compilerFlags = 0;

	bool isAntialiasingOn = loadInfo.m_antialiasing;

	char const* sourceName = loadInfo.m_shaderSrc.c_str();
	char const* entryPoint = loadInfo.m_shaderEntryPoint.c_str();
	char const* target = Material::GetTargetForShader(loadInfo.m_shaderType);

#if defined(ENGINE_DEBUG_RENDER)
	compilerFlags |= D3DCOMPILE_DEBUG;
	compilerFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	compilerFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	std::string aaLevelAsStr = std::to_string(m_antiAliasingLevel);
	char const* aaCStr = aaLevelAsStr.c_str();
	D3D_SHADER_MACRO macros[] = {
		"ENGINE_ANTIALIASING", (isAntialiasingOn) ? "1" : "0",
		"ANTIALIASING_LEVEL", aaCStr,
		NULL, NULL
	};

	HRESULT shaderCompileResult = D3DCompile(source, strlen(source), sourceName, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, compilerFlags, 0, &shaderBlob, &shaderErrorBlob);

	if (!SUCCEEDED(shaderCompileResult)) {

		std::string errorString = std::string((char*)shaderErrorBlob->GetBufferPointer());
		DX_SAFE_RELEASE(shaderErrorBlob);
		DX_SAFE_RELEASE(shaderBlob);

		DebuggerPrintf(Stringf("%s NOT COMPILING: %s", sourceName, errorString.c_str()).c_str());
		ERROR_AND_DIE("FAILED TO COMPILE SHADER TO BYTECODE");

	}


	outByteCode.resize(shaderBlob->GetBufferSize());
	memcpy(outByteCode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

	DX_SAFE_RELEASE(shaderBlob);

	return true;
}

Material* Renderer::CreateOrGetMaterial(std::filesystem::path materialPathNoExt)
{
	std::string materialXMLPath = materialPathNoExt.replace_extension(".xml").string();
	Material* material = GetMaterialForName(materialXMLPath.c_str());
	if (material) {
		return material;
	}

	return CreateMaterial(materialXMLPath);
}

Material* Renderer::CreateMaterial(std::string const& materialXMLFile)
{
	XMLDoc materialDoc;
	XMLError loadStatus = materialDoc.LoadFile(materialXMLFile.c_str());
	GUARANTEE_OR_DIE(loadStatus == tinyxml2::XML_SUCCESS, Stringf("COULD NOT LOAD MATERIAL XML FILE %s", materialXMLFile.c_str()));

	XMLElement const* firstElem = materialDoc.FirstChildElement("Material");

	std::string matName = ParseXmlAttribute(*firstElem, "name", "Unnamed Material");

	// Material properties
	XMLElement const* matProperty = firstElem->FirstChildElement();

	Material* newMat = new Material();
	newMat->LoadFromXML(matProperty);
	newMat->m_config.m_name = matName;
	newMat->m_config.m_src = materialXMLFile;
	CreatePSOForMaterial(newMat);

	m_loadedMaterials.push_back(newMat);

	return newMat;
}

ShaderByteCode* Renderer::CompileOrGetShaderBytes(ShaderLoadInfo const& shaderLoadInfo)
{
	ShaderByteCode* retByteCode = GetByteCodeForShaderSrc(shaderLoadInfo);

	if (retByteCode) return retByteCode;

	retByteCode = new ShaderByteCode();
	retByteCode->m_src = shaderLoadInfo.m_shaderSrc;
	retByteCode->m_shaderType = shaderLoadInfo.m_shaderType;

	char const* target = Material::GetTargetForShader(shaderLoadInfo.m_shaderType);
	std::string shaderSource;
	FileReadToString(shaderSource, shaderLoadInfo.m_shaderSrc);
	CompileShaderToByteCode(retByteCode->m_byteCode, shaderSource.c_str(), shaderLoadInfo);

	m_shaderByteCodes.push_back(retByteCode);
	return retByteCode;
}


ShaderByteCode* Renderer::GetByteCodeForShaderSrc(ShaderLoadInfo const& shaderLoadInfo)
{
	for (ShaderByteCode*& byteCode : m_shaderByteCodes) {
		if (AreStringsEqualCaseInsensitive(shaderLoadInfo.m_shaderSrc, byteCode->m_src)) {
			if (shaderLoadInfo.m_shaderType == byteCode->m_shaderType) {
				return byteCode;
			}
		}
	}

	return nullptr;
}

void Renderer::CreatePSOForMaterial(Material* material)
{
	MaterialConfig& matConfig = material->m_config;
	std::string baseName = material->GetName();
	std::string debugName;
	std::string shaderSource;


	for (ShaderLoadInfo& loadInfo : matConfig.m_shaders) {
		if (loadInfo.m_shaderSrc.empty()) continue;

		ShaderByteCode* shaderByteCode = CompileOrGetShaderBytes(loadInfo);
		material->m_byteCodes[loadInfo.m_shaderType] = shaderByteCode;
	}
	ShaderByteCode* vsByteCode = material->m_byteCodes[ShaderType::Vertex];
	std::vector<uint8_t>& vertexShaderByteCode = vsByteCode->m_byteCode;

	std::vector<D3D12_SIGNATURE_PARAMETER_DESC> reflectInputDesc;
	CreateInputLayoutFromVS(vertexShaderByteCode, reflectInputDesc);
	//layoutElementsDesc =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	//};

	// Const Char* copying from D3D12_SIGNATURE_PARAMETER_DESC is quite problematic
	std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayoutDesc = material->m_inputLayout;
	inputLayoutDesc.resize(reflectInputDesc.size());
	std::vector<char const*> nameStrings;
	nameStrings.resize(reflectInputDesc.size());

	for (int inputIndex = 0; inputIndex < reflectInputDesc.size(); inputIndex++) {
		D3D12_INPUT_ELEMENT_DESC& elementDesc = inputLayoutDesc[inputIndex];
		D3D12_SIGNATURE_PARAMETER_DESC& paramDesc = reflectInputDesc[inputIndex];
		char const*& currentString = nameStrings[inputIndex];

		/*
		* This gross allocation needs to be done, as the
		* SemanticName form paramDesc becomes invalid the moment it goes out of scope
		*/
		size_t nameLength = strlen(paramDesc.SemanticName);
		void* newNameAddr = malloc(nameLength + 1);
		strncpy_s((char*)newNameAddr, nameLength + 1, (char const*)paramDesc.SemanticName, _TRUNCATE);
		currentString = (char const*)newNameAddr;

		elementDesc.Format = GetFormatForComponent(paramDesc.ComponentType, paramDesc.SemanticName, paramDesc.Mask);
		elementDesc.SemanticName = currentString;
		elementDesc.SemanticIndex = paramDesc.SemanticIndex;
		elementDesc.InputSlot = 0;
		elementDesc.AlignedByteOffset = (inputIndex == 0) ? 0 : D3D12_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;
	}

	//inputLayoutDesc[0].SemanticName = "POSITION";
	//inputLayoutDesc[1].SemanticName = "COLOR";


	ShaderByteCode* psByteCode = material->m_byteCodes[ShaderType::Pixel];
	ShaderByteCode* gsByteCode = material->m_byteCodes[ShaderType::Geometry];
	ShaderByteCode* hsByteCode = material->m_byteCodes[ShaderType::Hull];
	ShaderByteCode* dsByteCode = material->m_byteCodes[ShaderType::Domain];

	D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(
		LocalToD3D12(matConfig.m_fillMode),			// Fill mode
		LocalToD3D12(matConfig.m_cullMode),			// Cull mode
		LocalToD3D12(matConfig.m_windingOrder),		// Winding order
		D3D12_DEFAULT_DEPTH_BIAS,					// Depth bias
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,				// Bias clamp
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,		// Slope scaled bias
		TRUE,										// Depth Clip enable
		FALSE,										// Multi sample (MSAA)
		FALSE,										// Anti aliased line enable
		0,											// Force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF	// Conservative Rasterization
	);

	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	SetBlendMode(matConfig.m_blendMode, blendDesc);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout.NumElements = (UINT)reflectInputDesc.size();
	psoDesc.InputLayout.pInputElementDescs = inputLayoutDesc.data();
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsByteCode->m_byteCode.data(), vsByteCode->m_byteCode.size());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(psByteCode->m_byteCode.data(), psByteCode->m_byteCode.size());
	if (gsByteCode) {
		psoDesc.GS = CD3DX12_SHADER_BYTECODE(gsByteCode->m_byteCode.data(), gsByteCode->m_byteCode.size());
	}

	if (hsByteCode) {
		psoDesc.HS = CD3DX12_SHADER_BYTECODE(hsByteCode->m_byteCode.data(), hsByteCode->m_byteCode.size());
	}

	if (dsByteCode) {
		psoDesc.DS = CD3DX12_SHADER_BYTECODE(dsByteCode->m_byteCode.data(), dsByteCode->m_byteCode.size());
	}

	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState.DepthEnable = matConfig.m_depthEnable;
	psoDesc.DepthStencilState.DepthFunc = LocalToD3D12(matConfig.m_depthFunc);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = // a stencil operation structure, does not really matter since stencil testing is turned off
	{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	psoDesc.DepthStencilState.FrontFace = defaultStencilOp; // both front and back facing polygons get the same treatment
	psoDesc.DepthStencilState.BackFace = defaultStencilOp;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE(matConfig.m_topology);
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;

	HRESULT psoCreation = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&material->m_PSO));
	ThrowIfFailed(psoCreation, "COULD NOT CREATE PSO");
	std::string shaderDebugName = "PSO:";
	shaderDebugName += baseName;
	SetDebugName(material->m_PSO, shaderDebugName.c_str());
}


bool Renderer::CreateInputLayoutFromVS(std::vector<uint8_t>& shaderByteCode, std::vector<D3D12_SIGNATURE_PARAMETER_DESC>& elementsDescs)
{
	// Reflect shader info
	ID3D12ShaderReflection* pShaderReflection = NULL;
	if (FAILED(D3DReflect((void*)shaderByteCode.data(), shaderByteCode.size(), IID_ID3D11ShaderReflection, (void**)&pShaderReflection)))
	{
		return false;
	}

	// Get shader info
	D3D12_SHADER_DESC shaderDesc;
	pShaderReflection->GetDesc(&shaderDesc);


	// Read input layout description from shader info
	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
		pShaderReflection->GetInputParameterDesc(i, &paramDesc);
		//save element desc
		elementsDescs.push_back(paramDesc);
	}
	DX_SAFE_RELEASE(pShaderReflection);
	return true;
}


Material* Renderer::GetMaterialForName(char const* materialName)
{
	std::string shaderNoExtension = std::filesystem::path(materialName).replace_extension("").string();
	for (int shaderIndex = 0; shaderIndex < m_loadedMaterials.size(); shaderIndex++) {
		Material* shader = m_loadedMaterials[shaderIndex];
		if (shader->GetName() == shaderNoExtension) {
			return shader;
		}
	}
	return nullptr;
}

Material* Renderer::GetMaterialForPath(char const* materialPath)
{
	std::string shaderNoExtension = std::filesystem::path(materialPath).replace_extension("").string();
	for (int shaderIndex = 0; shaderIndex < m_loadedMaterials.size(); shaderIndex++) {
		Material* shader = m_loadedMaterials[shaderIndex];
		if (shader->GetPath() == shaderNoExtension) {
			return shader;
		}
	}
	return nullptr;
}

void Renderer::SetDebugName(ID3D12Object* object, char const* name)
{
#if defined(ENGINE_DEBUG_RENDER)
	object->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#else
	UNUSED(object);
	UNUSED(name);
#endif
}

void Renderer::DrawImmediateCtx(ImmediateContext& ctx)
{
	Texture* currentRt = ctx.m_renderTargets[0];
	Resource* currentRtResc = currentRt->GetResource();
	currentRtResc->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, m_commandList.Get());

	Texture* depthTarget = ctx.m_depthTarget;
	Resource* depthTargetRsc = depthTarget->GetResource();
	depthTargetRsc->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE, m_commandList.Get());

	SetMaterialPSO(ctx.m_material);
	for (auto& [slot, texture] : ctx.m_boundTextures) {
		CopyTextureToHeap(texture, ctx.m_srvHandleStart, slot);
	}

	CopyCBufferToHeap(*ctx.m_cameraCBO, ctx.m_cbvHandleStart, 0);
	CopyCBufferToHeap(*ctx.m_modelCBO, ctx.m_cbvHandleStart, 1);

	for (auto& [slot, cbuffer] : ctx.m_boundCBuffers) {
		CopyCBufferToHeap(cbuffer, ctx.m_cbvHandleStart, slot);
	}

	DescriptorHeap* rtvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::RTV);
	DescriptorHeap* dsvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::DSV);
	DescriptorHeap* srvUAVCBVHeap = GetGPUDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);
	DescriptorHeap* samplerHeap = GetGPUDescriptorHeap(DescriptorHeapType::SAMPLER);
	ID3D12DescriptorHeap* allDescriptorHeaps[] = {
		srvUAVCBVHeap->GetHeap(),
		samplerHeap->GetHeap()
	};

	UINT numHeaps = sizeof(allDescriptorHeaps) / sizeof(ID3D12DescriptorHeap*);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->SetDescriptorHeaps(numHeaps, allDescriptorHeaps);


	m_commandList->SetGraphicsRootDescriptorTable(0, srvUAVCBVHeap->GetGPUHandleAtOffset(ctx.m_cbvHandleStart));
	m_commandList->SetGraphicsRootDescriptorTable(1, srvUAVCBVHeap->GetGPUHandleAtOffset(ctx.m_srvHandleStart));
	m_commandList->SetGraphicsRootDescriptorTable(2, samplerHeap->GetGPUHandleForHeapStart());

	//for (int heapIndex = 0; heapIndex < allDescriptorHeaps.size(); heapIndex++) {
	//	ID3D12DescriptorHeap* currentDescriptorHeap = allDescriptorHeaps[heapIndex];
	//	m_commandList->SetGraphicsRootDescriptorTable(heapIndex, currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	//}

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Later, each texture has its handle
	//ResourceView* rtv =  m_defaultRenderTarget->GetOrCreateView(RESOURCE_BIND_RENDER_TARGET_BIT);
	D3D12_CPU_DESCRIPTOR_HANDLE currentRTVHandle = currentRt->GetOrCreateView(RESOURCE_BIND_RENDER_TARGET_BIT)->GetHandle();
	ResourceView* dsvView = ctx.m_depthTarget->GetOrCreateView(RESOURCE_BIND_DEPTH_STENCIL_BIT);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvView->GetHandle();

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->OMSetRenderTargets(1, &currentRTVHandle, FALSE, &dsvHandle);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VERTEX_BUFFER_VIEW D3DbufferView = {};
	BufferView vBufferView = m_immediateVBO->GetBufferView();
	// Initialize the vertex buffer view.
	D3DbufferView.BufferLocation = vBufferView.m_bufferLocation;
	D3DbufferView.StrideInBytes = (UINT)vBufferView.m_strideInBytes;
	D3DbufferView.SizeInBytes = (UINT)vBufferView.m_sizeInBytes;

	m_commandList->IASetVertexBuffers(0, 1, &D3DbufferView);
	m_commandList->DrawInstanced(ctx.m_vertexCount, 1, ctx.m_vertexStart, 0);

}

ComPtr<ID3D12GraphicsCommandList2> Renderer::GetBufferCommandList()
{
	return m_ResourcesCommandList;
}

Texture* Renderer::CreateOrGetTextureFromFile(char const* imageFilePath)
{
	Texture* existingTexture = GetTextureForFileName(imageFilePath);
	if (existingTexture)
	{
		return existingTexture;
	}

	// Never seen this texture before!  Let's load it.
	Texture* newTexture = CreateTextureFromFile(imageFilePath);
	return newTexture;
}

void Renderer::DrawVertexArray(std::vector<Vertex_PCU> const& vertexes)
{
	DrawVertexArray((unsigned int)vertexes.size(), vertexes.data());
}

void Renderer::DrawVertexArray(unsigned int numVertexes, const Vertex_PCU* vertexes)
{
	m_currentDrawCtx.m_srvHandleStart = m_srvHandleStart;
	m_currentDrawCtx.m_cbvHandleStart = m_cbvHandleStart;

	auto findHighestValTex = [](const std::pair<unsigned int, Texture const*>& a, const std::pair<unsigned int, Texture const*>& b)->bool { return a.first < b.first; };
	auto findHighestValCbuffer = [](const std::pair<unsigned int, ConstantBuffer*>& a, const std::pair<unsigned int, ConstantBuffer*>& b)->bool { return a.first < b.first; };

	auto texMaxIt = std::max_element(m_currentDrawCtx.m_boundTextures.begin(), m_currentDrawCtx.m_boundTextures.end(), findHighestValTex);
	auto cBufferMaxIt = std::max_element(m_currentDrawCtx.m_boundCBuffers.begin(), m_currentDrawCtx.m_boundCBuffers.end(), findHighestValCbuffer);

	unsigned int texMax = (texMaxIt != m_currentDrawCtx.m_boundTextures.end()) ? texMaxIt->first : 0;
	unsigned int cBufferMax = (cBufferMaxIt != m_currentDrawCtx.m_boundCBuffers.end()) ? cBufferMaxIt->first : 0;

	m_srvHandleStart += texMax + 1;
	m_cbvHandleStart += cBufferMax + 1;

	m_cbvHandleStart += 2;

	const UINT vertexBufferSize = numVertexes * sizeof(Vertex_PCU);

	/*BufferDesc bufferDesc = {};
	bufferDesc.data = vertexes;
	bufferDesc.descriptorHeap = nullptr;
	bufferDesc.memoryUsage = MemoryUsage::Dynamic;
	bufferDesc.owner = this;
	bufferDesc.size = vertexBufferSize;
	bufferDesc.stride = sizeof(Vertex_PCU);

	VertexBuffer* vBuffer = new VertexBuffer(bufferDesc);
	m_currentDrawCtx.m_immediateBuffer = vBuffer;*/

	m_currentDrawCtx.m_vertexCount = numVertexes;
	m_currentDrawCtx.m_vertexStart = m_immediateVertexes.size();
	std::copy(vertexes, vertexes + numVertexes, std::back_inserter(m_immediateVertexes));

	if (!m_hasUsedModelSlot) {
		ConstantBuffer*& currentModelCBO = *m_currentDrawCtx.m_modelCBO;
		currentModelCBO->CopyCPUToGPU(&m_currentDrawCtx.m_modelConstants, sizeof(ModelConstants));
	}
	m_immediateCtxs.push_back(m_currentDrawCtx);
	m_hasUsedModelSlot = true;
}

void Renderer::SetModelMatrix(Mat44 const& modelMat)
{
	if (m_hasUsedModelSlot) {
		m_currentDrawCtx.m_modelCBO = &GetNextModelBuffer();
	}
	m_currentDrawCtx.m_modelConstants.ModelMatrix = modelMat;
	m_hasUsedModelSlot = false;
}

void Renderer::SetModelColor(Rgba8 const& modelColor)
{
	if (m_hasUsedModelSlot) {
		m_currentDrawCtx.m_modelCBO = &GetNextModelBuffer();
	}
	modelColor.GetAsFloats(m_currentDrawCtx.m_modelConstants.ModelColor);
	m_hasUsedModelSlot = false;
}

void Renderer::ExecuteCommandLists(ID3D12CommandList** commandLists, unsigned int count)
{
	m_commandQueue->ExecuteCommandLists(count, commandLists);
}

void Renderer::WaitForGPU()
{
	unsigned int currentValue = m_fenceValues[m_currentBackBuffer];
	int newFenceValue = SignalFence(m_commandQueue, m_fence, currentValue);

	HRESULT fenceCompletion = m_fence->SetEventOnCompletion(currentValue, m_fenceEvent);
	ThrowIfFailed(fenceCompletion, "ERROR ON SETTING EVENT ON COMPLETION FOR FENCE");
	::WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

	m_fenceValues[m_currentBackBuffer] = newFenceValue;

}

DescriptorHeap* Renderer::GetDescriptorHeap(DescriptorHeapType descriptorHeapType) const
{
	return m_defaultDescriptorHeaps[(size_t)descriptorHeapType];
}

DescriptorHeap* Renderer::GetGPUDescriptorHeap(DescriptorHeapType descriptorHeapType) const
{
	if ((size_t)descriptorHeapType > m_defaultGPUDescriptorHeaps.size()) return nullptr;
	return m_defaultGPUDescriptorHeaps[(size_t)descriptorHeapType];
}

void Renderer::CreateViewport()
{

	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_config.m_window->GetClientDimensions().x), static_cast<float>(m_config.m_window->GetClientDimensions().y), 0, 1);
	m_scissorRect = CD3DX12_RECT(0, 0, static_cast<long>(m_config.m_window->GetClientDimensions().x), static_cast<long>(m_config.m_window->GetClientDimensions().y));

}

Texture* Renderer::CreateTexture(TextureCreateInfo& creationInfo)
{
	Resource*& handle = creationInfo.m_handle;

	if (handle) {
		handle->m_resource->AddRef();
		//handle->m_resource->
	}
	else {
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Width = (UINT64)creationInfo.m_dimensions.x;
		textureDesc.Height = (UINT64)creationInfo.m_dimensions.y;
		textureDesc.MipLevels = 1;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.Format = LocalToD3D12(creationInfo.m_format);
		textureDesc.Flags = LocalToD3D12(creationInfo.m_bindFlags);
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		CD3DX12_HEAP_PROPERTIES heapType(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;
		if (creationInfo.m_initialData) {
			initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		handle = new Resource();
		handle->m_currentState = initialResourceState;
		D3D12_CLEAR_VALUE clearValueRT = {};
		D3D12_CLEAR_VALUE clearValueDST = {};
		D3D12_CLEAR_VALUE* clearValue = NULL;

		// If it can be bound as RT then it needs the clear colour, otherwise it's null
		if (creationInfo.m_bindFlags & RESOURCE_BIND_RENDER_TARGET_BIT) {
			creationInfo.m_clearColour.GetAsFloats(clearValueRT.Color);
			clearValueRT.Format = LocalToD3D12(creationInfo.m_clearFormat);
			clearValue = &clearValueRT;
		}

		if (creationInfo.m_bindFlags & RESOURCE_BIND_DEPTH_STENCIL_BIT) {
			creationInfo.m_clearColour.GetAsFloats(clearValueDST.Color);
			clearValueDST.Format = LocalToD3D12(TextureFormat::D24_UNORM_S8_UINT);
			clearValue = &clearValueDST;
		}

		HRESULT textureCreateHR = m_device->CreateCommittedResource(
			&heapType,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			initialResourceState,
			clearValue,
			IID_PPV_ARGS(&handle->m_resource)
		);

		if (creationInfo.m_initialData) {
			ID3D12Resource* textureUploadHeap;
			UINT64  const uploadBufferSize = GetRequiredIntermediateSize(handle->m_resource, 0, 1);
			CD3DX12_RESOURCE_DESC uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);

			HRESULT createUploadHeap = m_device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&uploadHeapDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap));

			ThrowIfFailed(createUploadHeap, "FAILED TO CREATE TEXTURE UPLOAD HEAP");
			SetDebugName(textureUploadHeap, "UplHeap");

			D3D12_SUBRESOURCE_DATA imageData = {};
			imageData.pData = creationInfo.m_initialData;
			imageData.RowPitch = creationInfo.m_stride * creationInfo.m_dimensions.x;
			imageData.SlicePitch = creationInfo.m_stride * creationInfo.m_dimensions.y * creationInfo.m_dimensions.x;
			UpdateSubresources(m_commandList.Get(), handle->m_resource, textureUploadHeap, 0, 0, 1, &imageData);
			handle->TransitionTo(D3D12_RESOURCE_STATE_COMMON, m_commandList.Get());

			m_frameUploadHeaps.push_back(textureUploadHeap);
			m_uploadRequested = true;
		}

		std::string const errorMsg = Stringf("COULD NOT CREATE TEXTURE WITH NAME %s", creationInfo.m_name.c_str());
		ThrowIfFailed(textureCreateHR, errorMsg.c_str());
	}
	//WaitForGPU();
	Texture* newTexture = new Texture(creationInfo);
	newTexture->m_handle = handle;
	SetDebugName(newTexture->m_handle->m_resource, creationInfo.m_name.c_str());

	m_loadedTextures.push_back(newTexture);

	return newTexture;
}

void Renderer::DestroyTexture(Texture* textureToDestroy)
{
	if (textureToDestroy) {
		Resource* texResource = textureToDestroy->m_handle;
		delete textureToDestroy;
		if (texResource) {
			delete texResource;
		}
	}
}

Texture* Renderer::GetTextureForFileName(char const* imageFilePath)
{

	Texture* textureToGet = nullptr;

	for (Texture*& loadedTexture : m_loadedTextures) {
		if (loadedTexture->GetImageFilePath() == imageFilePath) {
			return loadedTexture;
		}
	}
	return textureToGet;
}

Texture* Renderer::CreateTextureFromFile(char const* imageFilePath)
{
	Image loadedImage(imageFilePath);
	Texture* newTexture = CreateTextureFromImage(loadedImage);

	return newTexture;
}

Texture* Renderer::CreateTextureFromImage(Image const& image)
{
	TextureCreateInfo ci{};
	ci.m_owner = this;
	ci.m_name = image.GetImageFilePath();
	ci.m_dimensions = image.GetDimensions();
	ci.m_initialData = image.GetRawData();
	ci.m_stride = sizeof(Rgba8);


	Texture* newTexture = CreateTexture(ci);
	SetDebugName(newTexture->GetResource()->m_resource, newTexture->m_name.c_str());

	//m_loadedTextures.push_back(newTexture);

	return newTexture;
}

ResourceView* Renderer::CreateShaderResourceView(ResourceViewInfo const& viewInfo) const
{
	DescriptorHeap* srvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvDescriptorHeap->GetNextCPUHandle();
	m_device->CreateShaderResourceView(viewInfo.m_source->m_resource, viewInfo.m_srvDesc, cpuHandle);

	ResourceView* newResourceView = new ResourceView(viewInfo);
	newResourceView->m_descriptorHandle = cpuHandle;

	return newResourceView;
}

ResourceView* Renderer::CreateRenderTargetView(ResourceViewInfo const& viewInfo) const
{
	D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc = viewInfo.m_rtvDesc;

	DescriptorHeap* rtvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = rtvDescriptorHeap->GetNextCPUHandle();

	m_device->CreateRenderTargetView(viewInfo.m_source->m_resource, rtvDesc, cpuHandle);

	ResourceView* newResourceView = new ResourceView(viewInfo);
	newResourceView->m_descriptorHandle = cpuHandle;

	return newResourceView;
}

ResourceView* Renderer::CreateDepthStencilView(ResourceViewInfo const& viewInfo) const
{
	D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc = viewInfo.m_dsvDesc;

	DescriptorHeap* dsvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::DSV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = dsvDescriptorHeap->GetNextCPUHandle();

	m_device->CreateDepthStencilView(viewInfo.m_source->m_resource, dsvDesc, cpuHandle);

	ResourceView* newResourceView = new ResourceView(viewInfo);
	newResourceView->m_descriptorHandle = cpuHandle;

	return newResourceView;
}

ResourceView* Renderer::CreateConstantBufferView(ResourceViewInfo const& viewInfo, DescriptorHeap* descriptorHeap) const
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc = viewInfo.m_cbvDesc;

	DescriptorHeap* cbvDescriptorHeap = (descriptorHeap) ? descriptorHeap : GetDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvDescriptorHeap->GetNextCPUHandle();

	m_device->CreateConstantBufferView(cbvDesc, cpuHandle);

	ResourceView* newResourceView = new ResourceView(viewInfo);
	newResourceView->m_descriptorHandle = cpuHandle;

	return newResourceView;
}

void Renderer::SetSamplerMode(SamplerMode samplerMode)
{
	D3D12_SAMPLER_DESC samplerDesc = {};
	switch (samplerMode)
	{
	case SamplerMode::POINTCLAMP:
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

		break;
	case SamplerMode::POINTWRAP:
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

		break;
	case SamplerMode::BILINEARCLAMP:
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

		break;
	case SamplerMode::BILINEARWRAP:
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		break;

	case SamplerMode::SHADOWMAPS:
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor[0] = 0.0f;
		samplerDesc.BorderColor[1] = 0.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 0.0f;

		break;
	default:
		break;
	}
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	DescriptorHeap* samplerHeap = GetDescriptorHeap(DescriptorHeapType::SAMPLER);

	m_device->CreateSampler(&samplerDesc, samplerHeap->GetHandleAtOffset(0));
	DescriptorHeap* gpuSamplerHeap = GetGPUDescriptorHeap(DescriptorHeapType::SAMPLER);
	m_device->CopyDescriptorsSimple(1, gpuSamplerHeap->GetCPUHandleForHeapStart(), samplerHeap->GetHandleAtOffset(0), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	//if (!SUCCEEDED(samplerStateCreationResult)) {
	//	ERROR_AND_DIE("ERROR CREATING SAMPLER STATE");
	//}
}

void Renderer::SetBlendMode(BlendMode blendMode, D3D12_BLEND_DESC& blendDesc)
{

	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (blendMode) {
	case BlendMode::ALPHA:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case BlendMode::ADDITIVE:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	case BlendMode::OPAQUE:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		break;
	default:
		ERROR_AND_DIE(Stringf("Unknown / unsupported blend mode #%i", blendMode));
		break;
	}
}

BitmapFont* Renderer::CreateBitmapFont(std::filesystem::path bitmapPath)
{
	std::string filename = bitmapPath.string();
	std::string filePathString = bitmapPath.replace_extension(".png").string();
	Texture* bitmapTexture = CreateOrGetTextureFromFile(filePathString.c_str());
	BitmapFont* newBitmapFont = new BitmapFont(filename.c_str(), *bitmapTexture);

	m_loadedFonts.push_back(newBitmapFont);
	return newBitmapFont;
}

void Renderer::ResetGPUDescriptorHeaps()
{
	//#TODO
}

ResourceView* Renderer::CreateResourceView(ResourceViewInfo const& resourceViewInfo, DescriptorHeap* descriptorHeap) const
{
	switch (resourceViewInfo.m_viewType)
	{
	default:
		ERROR_AND_DIE("UNRECOGNIZED VIEW TYPE");
		break;
	case RESOURCE_BIND_SHADER_RESOURCE_BIT:
		return CreateShaderResourceView(resourceViewInfo);
	case RESOURCE_BIND_RENDER_TARGET_BIT:
		return CreateRenderTargetView(resourceViewInfo);
	case RESOURCE_BIND_DEPTH_STENCIL_BIT:
		return CreateDepthStencilView(resourceViewInfo);
	case RESOURCE_BIND_CONSTANT_BUFFER_VIEW_BIT:
		return CreateConstantBufferView(resourceViewInfo, descriptorHeap);
	case RESOURCE_BIND_UNORDERED_ACCESS_VIEW_BIT:
		break;
	}

	return nullptr;
}

BitmapFont* Renderer::CreateOrGetBitmapFont(std::filesystem::path bitmapPath)
{
	for (int loadedFontIndex = 0; loadedFontIndex < m_loadedFonts.size(); loadedFontIndex++) {
		BitmapFont*& bitmapFont = m_loadedFonts[loadedFontIndex];
		if (bitmapFont->m_fontFilePathNameWithNoExtension == bitmapPath.string()) {
			return bitmapFont;
		}
	}

	return CreateBitmapFont(bitmapPath);

}

void Renderer::BindConstantBuffer(ConstantBuffer* cBuffer, unsigned int slot /*= 0*/)
{
	m_currentDrawCtx.m_boundCBuffers[slot] = cBuffer;
}

void Renderer::BindTexture(Texture const* texture, unsigned int slot /*= 0*/)
{
	m_currentDrawCtx.m_boundTextures[slot] = texture;
}

void Renderer::BindMaterial(Material* mat)
{
	if (!mat) {
		if (is3DDefault) {
			mat = m_default3DMaterial;
		}
		else {
			mat = m_default2DMaterial;
		}
	}
	m_currentDrawCtx.m_material = mat;
}

void Renderer::CopyTextureToHeap(Texture const* textureToBind, unsigned int handleStart, unsigned int slot)
{
	Texture* usedTex = const_cast<Texture*>(textureToBind);
	if (!textureToBind) {
		usedTex = m_defaultTexture;
	}

	ResourceView* rsv = usedTex->GetOrCreateView(RESOURCE_BIND_SHADER_RESOURCE_BIT);
	DescriptorHeap* srvHeap = GetGPUDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetHandleAtOffset(handleStart + slot);
	D3D12_CPU_DESCRIPTOR_HANDLE textureHandle = rsv->GetHandle();

	usedTex->GetResource()->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_commandList.Get());

	if (srvHandle.ptr != textureHandle.ptr) {
		m_device->CopyDescriptorsSimple(1, srvHandle, rsv->GetHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void Renderer::CopyCBufferToHeap(ConstantBuffer* bufferToBind, unsigned int handleStart, unsigned int slot /*= 0*/)
{
	if (!bufferToBind) return;

	ResourceView* rsv = bufferToBind->GetOrCreateView();
	DescriptorHeap* srvHeap = GetGPUDescriptorHeap(DescriptorHeapType::SRV_UAV_CBV);
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetHandleAtOffset(handleStart + slot);
	D3D12_CPU_DESCRIPTOR_HANDLE cBufferHandle = rsv->GetHandle();


	bufferToBind->m_buffer->TransitionTo(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_commandList.Get());
	m_device->CopyDescriptorsSimple(1, srvHandle, rsv->GetHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

ConstantBuffer*& Renderer::GetNextCameraBuffer()
{
	m_currentCameraCBufferSlot++;
	if (m_currentCameraCBufferSlot > m_cameraCBOArray.size()) ERROR_AND_DIE("RAN OUT OF CONSTANT BUFFER SLOTS");
	ConstantBuffer*& bufferToReturn = m_cameraCBOArray[m_currentCameraCBufferSlot];
	return bufferToReturn;
}

ConstantBuffer*& Renderer::GetNextModelBuffer()
{
	m_currentModelCBufferSlot++;
	if (m_currentModelCBufferSlot > m_modelCBOArray.size()) ERROR_AND_DIE("RAN OUT OF CONSTANT BUFFER SLOTS");
	ConstantBuffer*& bufferToReturn = m_modelCBOArray[m_currentModelCBufferSlot];
	return bufferToReturn;
}

ConstantBuffer*& Renderer::GetCurrentCameraBuffer()
{
	return m_cameraCBOArray[m_currentCameraCBufferSlot];
}

ConstantBuffer*& Renderer::GetCurrentModelBuffer()
{
	return m_modelCBOArray[m_currentModelCBufferSlot];
}

void Renderer::DrawAllImmediateContexts()
{
	size_t vertexesSize = sizeof(Vertex_PCU) * m_immediateVertexes.size();
	m_immediateVBO->GuaranteeBufferSize(vertexesSize);
	m_immediateVBO->CopyCPUToGPU(m_immediateVertexes.data(), vertexesSize);
	for (ImmediateContext& ctx : m_immediateCtxs) {
		DrawImmediateCtx(ctx);
	}
}

void Renderer::ClearAllImmediateContexts()
{
	for (ImmediateContext& ctx : m_immediateCtxs) {
		/*if (ctx.m_immediateBuffer) {
			delete ctx.m_immediateBuffer;
			ctx.m_immediateBuffer = nullptr;
		}*/
	}

	m_immediateCtxs.clear();
	m_immediateCtxs.resize(0);
}

void Renderer::SetMaterialPSO(Material* mat)
{
	m_commandList->SetPipelineState(mat->m_PSO);
}

void Renderer::BeginFrame()
{
	DebugRenderBeginFrame();

	m_currentModelCBufferSlot = 0;
	m_currentCameraCBufferSlot = 0;

	m_currentDrawCtx = ImmediateContext();
	m_srvHandleStart = SRV_HANDLE_START;
	m_cbvHandleStart = CBV_HANDLE_START;

	if (m_uploadRequested) {
		//WaitForGPU();
		m_commandList->Close();
		ID3D12CommandList* commLists[1] = { m_commandList.Get() };
		ExecuteCommandLists(commLists, 1);
		WaitForGPU();
		for (ID3D12Resource*& uploadHeap : m_frameUploadHeaps) {
			DX_SAFE_RELEASE(uploadHeap);
			uploadHeap = nullptr;
		}
		m_frameUploadHeaps.resize(0);
		m_uploadRequested = false;
	}

	Texture* currentRt = GetActiveColorTarget();
	Resource* activeRTResource = currentRt->GetResource();
	DescriptorHeap* rtvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::RTV);

	// Command list allocators can only be reset when the associated 
  // command lists have finished execution on the GPU; apps should use 
  // fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocators[m_currentBackBuffer]->Reset(), "FAILED TO RESET COMMAND ALLOCATOR");

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentBackBuffer].Get(), m_default2DMaterial->m_PSO), "COULD NOT RESET COMMAND LIST");

	BindTexture(nullptr);
	activeRTResource->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, m_commandList.Get());
	// Indicate that the back buffer will be used as a render target.
	//CD3DX12_RESOURCE_BARRIER resourceBarrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_currentBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	//m_commandList->ResourceBarrier(1, &resourceBarrierRTV);
	//m_defaultRenderTarget->GetResource()->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, m_commandList.Get());

	//D3D12_CPU_DESCRIPTOR_HANDLE currentRTVHandle = m_defaultRenderTarget->GetOrCreateView(ResourceBindFlagBit::RESOURCE_BIND_RENDER_TARGET_BIT)->GetHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE currentRTVHandle = currentRt->GetOrCreateView(ResourceBindFlagBit::RESOURCE_BIND_RENDER_TARGET_BIT)->GetHandle();

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->OMSetRenderTargets(1, &currentRTVHandle, FALSE, nullptr);

	ResetGPUDescriptorHeaps();
	ClearScreen(m_defaultRenderTarget->GetClearColour());
	ClearDepth();
	// Record commands.
	//float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	//m_defaultRenderTarget->GetClearColour().GetAsFloats(clearColor);
	//m_commandList->ClearRenderTargetView(currentRTVHandle, clearColor, 0, nullptr);
	m_immediateVertexes.clear();


#if defined(ENGINE_USE_IMGUI)
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void Renderer::EndFrame()
{
	DebugRenderEndFrame();


	DrawAllImmediateContexts();

	Resource* defaultRTResource = m_defaultRenderTarget->GetResource();
	defaultRTResource->TransitionTo(D3D12_RESOURCE_STATE_COPY_DEST, m_commandList.Get());

	// This is setting up for IMGUI
	Resource* currentRtResource = GetActiveColorTarget()->GetResource();
	currentRtResource->TransitionTo(D3D12_RESOURCE_STATE_COPY_SOURCE, m_commandList.Get());


	m_commandList->CopyResource(m_defaultRenderTarget->m_handle->m_resource, currentRtResource->m_resource);

	currentRtResource->TransitionTo(D3D12_RESOURCE_STATE_PRESENT, m_commandList.Get());
	//DebugRenderEndFrame();
	/*CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetActiveColorTarget().Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT);

	m_commandList->ResourceBarrier(1, &barrier);*/

	// Close command list
	//ThrowIfFailed(m_ResourcesCommandList->Close(), "COULD NOT CLOSE RESOURCES COMMAND LIST");
	ThrowIfFailed(m_commandList->Close(), "COULD NOT CLOSE COMMAND LIST");

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	ExecuteCommandLists(ppCommandLists, 1);

#if defined(ENGINE_DISABLE_VSYNC)
	m_swapChain->Present(0, 0);
#else
	m_swapChain->Present(1, 0);
#endif

	Flush(m_commandQueue, m_fence, m_fenceValues.data(), m_fenceEvent);
	// Flush Command Queue getting ready for next Frame
	WaitForGPU();

	ClearAllImmediateContexts();

	m_currentFrame++;

}

void Renderer::Shutdown()
{
	Flush(m_commandQueue, m_fence, m_fenceValues.data(), m_fenceEvent);

	for (auto& uploadHeap : m_frameUploadHeaps) {
		DX_SAFE_RELEASE(uploadHeap);
	}

	for (Texture*& texture : m_loadedTextures) {
		DestroyTexture(texture);
		texture = nullptr;
	}

	for (ShaderByteCode*& shaderByteCode : m_shaderByteCodes) {
		delete shaderByteCode;
		shaderByteCode = nullptr;
	}

	for (Material*& shader : m_loadedMaterials) {
		delete shader;
		shader = nullptr;
	}

	for (auto commandAlloc : m_commandAllocators) {

		commandAlloc.Reset();
	}

	ClearAllImmediateContexts();

	//for (auto backBuffer : m_backBuffers) {
	//	backBuffer.Reset();
	//}

	m_pipelineState.Reset();
	m_fence.Reset();
	for (DescriptorHeap*& descriptorHeap : m_defaultDescriptorHeaps) {
		delete descriptorHeap;
		descriptorHeap = nullptr;
	}
	for (DescriptorHeap*& descriptorHeap : m_defaultGPUDescriptorHeaps) {
		delete descriptorHeap;
		descriptorHeap = nullptr;
	}


	for (int constantBufferInd = 0; constantBufferInd < m_cameraCBOArray.size(); constantBufferInd++) {
		ConstantBuffer*& cameraBuffer = m_cameraCBOArray[constantBufferInd];
		ConstantBuffer*& modelBuffer = m_modelCBOArray[constantBufferInd];

		if (cameraBuffer) {
			delete cameraBuffer;
			cameraBuffer = nullptr;
		}

		if (modelBuffer) {
			delete modelBuffer;
			modelBuffer = nullptr;
		}

	}

	delete m_immediateVBO;
	m_immediateVBO = nullptr;

	m_commandList.Reset();
	m_ResourcesCommandList.Reset();
	m_rootSignature.Reset();

	m_commandQueue.Reset();
	m_swapChain.Reset();
	m_device.Reset();
	//m_dxgiFactory.Get()->Release();
	m_dxgiFactory.Reset();

	DebugRenderSystemShutdown();
}

void Renderer::BeginCamera(Camera const& camera)
{
	m_currentCamera = &camera;
	if (camera.GetCameraMode() == CameraMode::Orthographic) {
		BindMaterial(m_default2DMaterial);
	}
	else {
		BindMaterial(m_default3DMaterial);
	}

	BindTexture(m_defaultTexture);
	SetSamplerMode(SamplerMode::POINTCLAMP);
	m_currentDrawCtx.m_renderTargets[0] = GetActiveColorTarget();
	m_currentDrawCtx.m_depthTarget = m_defaultDepthTarget;

	CameraConstants cameraConstants = {};
	cameraConstants.ProjectionMatrix = camera.GetProjectionMatrix();
	cameraConstants.ViewMatrix = camera.GetViewMatrix();
	cameraConstants.InvertedMatrix = cameraConstants.ProjectionMatrix.GetInverted();

	ConstantBuffer*& nextCameraBuffer = GetCurrentCameraBuffer();
	nextCameraBuffer->CopyCPUToGPU(&cameraConstants, sizeof(CameraConstants));
	m_currentCameraCBufferSlot++;

	ConstantBuffer*& nextModelBuffer = GetCurrentModelBuffer();
	m_currentModelCBufferSlot++;

	m_currentDrawCtx.m_cameraCBO = &nextCameraBuffer;
	m_currentDrawCtx.m_modelConstants = ModelConstants();
	m_currentDrawCtx.m_modelCBO = &nextModelBuffer;
	m_hasUsedModelSlot = false;
	//BindConstantBuffer(m_cameraCBO, 2);
	//BindConstantBuffer(m_modelCBO, 3);
}

void Renderer::EndCamera(Camera const& camera)
{
	if (&camera != m_currentCamera) { 
		ERROR_RECOVERABLE("USING A DIFFERENT CAMERA TO END CAMERA PASS");
	}

	if (m_hasUsedModelSlot) {
		ConstantBuffer*& currentModelCBO = *m_currentDrawCtx.m_modelCBO;
		currentModelCBO->CopyCPUToGPU(&m_currentDrawCtx.m_modelConstants, sizeof(ModelConstants));
		m_currentModelCBufferSlot++;
	}

	m_currentCamera = nullptr;
	m_currentDrawCtx = ImmediateContext();
}

void Renderer::ClearScreen(Rgba8 const& color)
{
	DescriptorHeap* rtvDescriptorHeap = GetDescriptorHeap(DescriptorHeapType::RTV);
	Texture* currentBackBuffer = GetActiveColorTarget();

	float colorAsArray[4];
	color.GetAsFloats(colorAsArray);
	//currentBackBuffer->GetClearColour().GetAsFloats(colorAsArray);
	Resource* rtResource = currentBackBuffer->GetResource();
	rtResource->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, m_commandList.Get());
	//CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	//	GetActiveColorTarget().Get(),
	//	D3D12_RESOURCE_STATE_PRESENT,
	//	D3D12_RESOURCE_STATE_RENDER_TARGET
	//);

	//m_commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE currentRTVHandle = currentBackBuffer->GetOrCreateView(RESOURCE_BIND_RENDER_TARGET_BIT)->GetHandle();;

	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
	//	m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->ClearRenderTargetView(currentRTVHandle, colorAsArray, 0, nullptr);


}

void Renderer::ClearDepth(float clearDepth /*= 0.0f*/)
{
	ResourceView* dsvView = m_defaultDepthTarget->GetOrCreateView(RESOURCE_BIND_DEPTH_STENCIL_BIT);
	Resource* dsvRsc = m_defaultDepthTarget->GetResource();
	dsvRsc->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE, m_commandList.Get());

	D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;

	m_commandList->ClearDepthStencilView(dsvView->GetHandle(), clearFlags, clearDepth, 0, 0, NULL);
}

LiveObjectReporter::~LiveObjectReporter()
{
#if defined(_DEBUG) 
	ID3D12Debug3* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

		IDXGIDebug1* debug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug));
		debugController->Release();
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		debug->Release();
	}
	else {
		ERROR_AND_DIE("COULD NOT ENABLE DX12 LIVE REPORTING");
	}

#endif
}
