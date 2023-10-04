#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/DefaultShader.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <dxgidebug.h>
#include "Engine/Math/Vec4.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"
#include <d3dx12.h> // Notice the X. These are the helper structures not the DX12 header

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

void Renderer::CreateRenderTargetViews()
{
	// Handle size is vendor specific
	m_RTVdescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// Get a helper handle to the descriptor and create the RTVs 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVdescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	m_backBuffers.resize(m_config.m_backBuffersCount);
	for (unsigned int frameBufferInd = 0; frameBufferInd < m_config.m_backBuffersCount; frameBufferInd++) {
		ComPtr<ID3D12Resource2>& backBuffer = m_backBuffers[frameBufferInd];
		HRESULT fetchBuffer = m_swapChain->GetBuffer(frameBufferInd, IID_PPV_ARGS(&backBuffer));
		if (!SUCCEEDED(fetchBuffer)) {
			ERROR_AND_DIE("COULD NOT GET FRAME BUFFER");
		}

		m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
		rtvHandle.Offset(m_RTVdescriptorSize);

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
	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange[3] = {};
	descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, -1, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	descriptorRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	descriptorRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	// Base parameters, initial at the root table. Could be a descriptor, or a pointer to descriptor 
	// In this case, one descriptor table per slot in the first 3
	CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};

	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange[0], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange[1], D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[2].InitAsDescriptorTable(1, &descriptorRange[2], D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignature(3, rootParameters);
	rootSignature.Desc_1_2.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	//ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "COULD NOT SERIALIZE ROOT SIGNATURE");
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	HRESULT rootSignatureSerialization = D3D12SerializeVersionedRootSignature(&rootSignature, signature.GetAddressOf(), error.GetAddressOf());
	ThrowIfFailed(rootSignatureSerialization, "COULD NOT SERIALIZE ROOT SIGNATURE");
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)), "COULD NOT CREATE ROOT SIGNATURE");
	SetDebugName(m_rootSignature, "DEFAULTROOTSIGNATURE"); 

	m_defaultDescriptorHeaps.resize(1);
	CreateDescriptorHeap(m_defaultDescriptorHeaps[0], D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true);
	//CreateDescriptorHeap(m_defaultDescriptorHeaps[1], D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, true);

	auto descriptorsize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// Get a helper handle to the descriptor and create the RTVs 
	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptorHandle(m_defaultDescriptorHeaps[0]->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC bsDesc = {};
	bsDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bsDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	bsDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	m_device->CreateShaderResourceView(nullptr, &bsDesc, descriptorHandle);
	descriptorHandle.Offset(descriptorsize);
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
	unsigned int waitOnValue = m_fenceValues[m_currentBackBuffer];
	WaitForFenceValue(fence, m_fenceValues[m_currentBackBuffer], fenceEvent);

	m_fenceValues[m_currentBackBuffer] = newFenceValue;
}

ComPtr<ID3D12Resource2> Renderer::GetActiveColorTarget() const
{
	return m_backBuffers[m_currentBackBuffer];
}

ComPtr<ID3D12Resource2> Renderer::GetBackUpColorTarget() const
{
	int otherInd = (m_currentBackBuffer + 1) % 2;
	return m_backBuffers[otherInd];
}

void Renderer::Startup()
{
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
	CreateDescriptorHeap(m_RTVdescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_config.m_backBuffersCount + 1); // Backbufers + default
	CreateRenderTargetViews();

	m_commandAllocators.resize(m_config.m_backBuffersCount + 1);
	for (unsigned int frameIndex = 0; frameIndex < m_config.m_backBuffersCount; frameIndex++) {
		CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[frameIndex]);
	}
	CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_commandAllocators.size() - 1]);
	CreateCommandList(m_bufferCommandList, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_commandAllocators.size() - 1]);

	CreateDefaultRootSignature();
	m_defaultShader = CreateShader("Default", g_defaultShaderSource);

	CreateCommandList(m_commandList, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_currentBackBuffer]);

	ThrowIfFailed(m_commandList->Close(), "COULD NOT CLOSE DEFAULT COMMAND LIST");
	ThrowIfFailed(m_bufferCommandList->Close(), "COULT NOT CLOSE INTERNAL BUFFER COMMAND LIST");


	CreateFence();

	m_fenceValues[m_currentBackBuffer]++;
	CreateFenceEvent();

}


bool Renderer::CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, char const* name, char const* source, char const* entryPoint, char const* target, bool isAntialiasingOn)
{
	ID3DBlob* shaderBlob;
	ID3DBlob* shaderErrorBlob;

	UINT compilerFlags = 0;

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

	HRESULT shaderCompileResult = D3DCompile(source, strlen(source), name, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, compilerFlags, 0, &shaderBlob, &shaderErrorBlob);

	if (!SUCCEEDED(shaderCompileResult)) {

		std::string errorString = std::string((char*)shaderErrorBlob->GetBufferPointer());
		DX_SAFE_RELEASE(shaderErrorBlob);
		DX_SAFE_RELEASE(shaderBlob);

		DebuggerPrintf(Stringf("%s NOT COMPILING: %s", name, errorString.c_str()).c_str());
		ERROR_AND_DIE("FAILED TO COMPILE SHADER TO BYTECODE");

	}


	outByteCode.resize(shaderBlob->GetBufferSize());
	memcpy(outByteCode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());

	DX_SAFE_RELEASE(shaderBlob);

	return true;
}

Shader* Renderer::CreateShader(std::filesystem::path shaderName)
{
	std::string shaderSource;
	FileReadToString(shaderSource, shaderName.replace_extension("hlsl").string().c_str());
	return CreateShader(shaderName.replace_extension("").string().c_str(), shaderSource.c_str());
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

Shader* Renderer::CreateShader(char const* shaderName, char const* shaderSource)
{
	ShaderConfig config;
	config.m_name = shaderName;

	Shader* newShader = new Shader(config);
	std::string baseName = shaderName;
	std::string debugName;

	std::vector<uint8_t>& vertexShaderByteCode = newShader->m_VSByteCode;
	CompileShaderToByteCode(vertexShaderByteCode, shaderName, shaderSource, config.m_vertexEntryPoint.c_str(), "vs_5_0", false);

	std::string shaderNameAsString(shaderName);

	std::vector<uint8_t>& pixelShaderByteCode = newShader->m_PSByteCode;
	CompileShaderToByteCode(pixelShaderByteCode, shaderName, shaderSource, config.m_pixelEntryPoint.c_str(), "ps_5_0", false);

	std::vector<D3D12_SIGNATURE_PARAMETER_DESC> reflectInputDesc;
	CreateInputLayoutFromVS(vertexShaderByteCode, reflectInputDesc);
	//layoutElementsDesc =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	//};

	// Const Char* copying from D3D12_SIGNATURE_PARAMETER_DESC is quite problematic
	D3D12_INPUT_ELEMENT_DESC* inputLayoutDesc = new D3D12_INPUT_ELEMENT_DESC[reflectInputDesc.size()];
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


	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout.NumElements = (UINT)reflectInputDesc.size();
	psoDesc.InputLayout.pInputElementDescs = inputLayoutDesc;
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(newShader->m_VSByteCode.data(), newShader->m_VSByteCode.size());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(newShader->m_PSByteCode.data(), newShader->m_PSByteCode.size());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	HRESULT psoCreation = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&newShader->m_PSO));
	ThrowIfFailed(psoCreation, "COULD NOT CREATE PSO");
	std::string shaderDebugName = "PSO:";
	shaderDebugName += shaderName;
	SetDebugName(newShader->m_PSO, shaderDebugName.c_str());
	m_loadedShaders.push_back(newShader);

	for (int inputIndex = 0; inputIndex < reflectInputDesc.size(); inputIndex++) {
		D3D12_INPUT_ELEMENT_DESC& elementDesc = inputLayoutDesc[inputIndex];
		free((void*)elementDesc.SemanticName);
	}

	return newShader;
}

Shader* Renderer::GetShaderForName(char const* shaderName)
{
	std::string shaderNoExtension = std::filesystem::path(shaderName).replace_extension("").string();
	for (int shaderIndex = 0; shaderIndex < m_loadedShaders.size(); shaderIndex++) {
		Shader* shader = m_loadedShaders[shaderIndex];
		if (shader->GetName() == shaderNoExtension) {
			return shader;
		}
	}
	return nullptr;
}

void Renderer::SetDebugName(ID3D12Object* object, char const* name)
{
#if defined(ENGINE_DEBUG_RENDER)
	size_t strLength = strlen(name) + 1;
	wchar_t* debugName = new wchar_t[strLength];
	size_t numChars = 0;

	mbstowcs_s(&numChars, debugName, strLength, name, _TRUNCATE);
	size_t debugStringSize = numChars * sizeof(wchar_t);
	object->SetPrivateData(WKPDID_D3DDebugObjectNameW, debugStringSize, debugName);
	//object->SetName(name);
#else
	UNUSED(object);
	UNUSED(name);
#endif
}

void Renderer::DrawImmediateBuffers()
{
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->SetDescriptorHeaps(m_defaultDescriptorHeaps.size(), m_defaultDescriptorHeaps.data());
	m_commandList->SetGraphicsRootDescriptorTable(0, m_defaultDescriptorHeaps[0]->GetGPUDescriptorHandleForHeapStart());
	//m_commandList->SetGraphicsRootDescriptorTable(1, m_defaultDescriptorHeaps[1]->GetGPUDescriptorHandleForHeapStart());

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVdescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_VERTEX_BUFFER_VIEW D3DbufferView = {};
	for (VertexBuffer* vertexBuffer : m_immediateBuffers) {
		BufferView vBufferView = vertexBuffer->GetBufferView();
		// Initialize the vertex buffer view.
		D3DbufferView.BufferLocation = vBufferView.m_bufferLocation;
		D3DbufferView.StrideInBytes = (UINT)vBufferView.m_strideInBytes;
		D3DbufferView.SizeInBytes = (UINT)vBufferView.m_sizeInBytes;

		m_commandList->IASetVertexBuffers(0, 1, &D3DbufferView);
		UINT vertexCount = UINT(vBufferView.m_sizeInBytes / vBufferView.m_strideInBytes);
		m_commandList->DrawInstanced(vertexCount, 1, 0, 0);
	}

}

ComPtr<ID3D12GraphicsCommandList2> Renderer::GetBufferCommandList()
{
	return m_bufferCommandList;
}

Shader* Renderer::CreateOrGetShader(std::filesystem::path shaderName)
{
	std::string filename = shaderName.replace_extension(".hlsl").string();

	Shader* shaderSearched = GetShaderForName(filename.c_str());

	if (!shaderSearched) {
		return CreateShader(filename);
	}

	return shaderSearched;
}


void Renderer::DrawVertexArray(std::vector<Vertex_PCU> const& vertexes)
{
	DrawVertexArray((unsigned int)vertexes.size(), vertexes.data());
}

void Renderer::DrawVertexArray(unsigned int numVertexes, const Vertex_PCU* vertexes)
{
	const UINT vertexBufferSize = numVertexes * sizeof(Vertex_PCU);

	VertexBuffer* vBuffer = new VertexBuffer(this, vertexBufferSize, sizeof(Vertex_PCU), MemoryUsage::Dynamic, vertexes);
	m_immediateBuffers.push_back(vBuffer);
}

void Renderer::SetModelMatrix(Mat44 const& modelMat)
{

}

void Renderer::SetModelColor(Rgba8 const& modelColor)
{

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
	::WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

	m_fenceValues[m_currentBackBuffer] = newFenceValue;

}

void Renderer::CreateViewport()
{

	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_config.m_window->GetClientDimensions().x), static_cast<float>(m_config.m_window->GetClientDimensions().y), 0, 1);
	m_scissorRect = CD3DX12_RECT(0, 0, static_cast<long>(m_config.m_window->GetClientDimensions().x), static_cast<long>(m_config.m_window->GetClientDimensions().y));

}
void Renderer::BeginFrame()
{
	// Command list allocators can only be reset when the associated 
  // command lists have finished execution on the GPU; apps should use 
  // fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocators[m_currentBackBuffer]->Reset(), "FAILED TO RESET COMMAND ALLOCATOR");

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentBackBuffer].Get(), m_defaultShader->m_PSO), "COULD NOT RESET COMMAND LIST");

	// Indicate that the back buffer will be used as a render target.
	CD3DX12_RESOURCE_BARRIER resourceBarrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[m_currentBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &resourceBarrierRTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVdescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

#if defined(ENGINE_USE_IMGUI)
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void Renderer::EndFrame()
{
	DrawImmediateBuffers();


	//DebugRenderEndFrame();
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetActiveColorTarget().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	m_commandList->ResourceBarrier(1, &barrier);

	// Close command list
	ThrowIfFailed(m_commandList->Close(), "COULD NOT CLOSE COMMAND LIST");

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	ExecuteCommandLists(ppCommandLists, 1);

	m_swapChain->Present(1, 0);

	Flush(m_commandQueue, m_fence, m_fenceValues.data(), m_fenceEvent);
	// Flush Command Queue getting ready for next Frame
	WaitForGPU();

	for (VertexBuffer*& vBuffer : m_immediateBuffers) {
		delete vBuffer;
		vBuffer = nullptr;
	}

	m_immediateBuffers.resize(0);
	m_currentFrame++;

}

void Renderer::Shutdown()
{
	Flush(m_commandQueue, m_fence, m_fenceValues.data(), m_fenceEvent);
	for (Shader*& shader : m_loadedShaders) {
		delete shader;
		shader = nullptr;
	}

	for (auto commandAlloc : m_commandAllocators) {
		commandAlloc.Reset();
	}

	for (auto backBuffer : m_backBuffers) {
		backBuffer.Reset();
	}

	m_pipelineState.Reset();
	m_fence.Reset();
	DX_SAFE_RELEASE(m_RTVdescriptorHeap);
	for (ID3D12DescriptorHeap* descriptorHeap : m_defaultDescriptorHeaps) {
		DX_SAFE_RELEASE(descriptorHeap);
	}
	m_commandList.Reset();
	m_rootSignature.Reset();

	m_commandQueue.Reset();
	m_swapChain.Reset();
	m_device.Reset();
	//m_dxgiFactory.Get()->Release();
	m_dxgiFactory.Reset();




}

void Renderer::ClearScreen(Rgba8 const& color)
{
	float colorAsArray[4];
	color.GetAsFloats(colorAsArray);
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetActiveColorTarget().Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	m_commandList->ResourceBarrier(1, &barrier);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescHandle(m_RTVdescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		m_currentBackBuffer, m_RTVdescriptorSize);
	m_commandList->ClearRenderTargetView(rtvDescHandle, colorAsArray, 0, nullptr);


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
