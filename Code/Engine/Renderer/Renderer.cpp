#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/FileUtils.hpp"


inline void ThrowIfFailed(HRESULT hr, char const* errorMsg) {
	if (FAILED(hr)) {
		ERROR_AND_DIE(errorMsg);
	}
}

Renderer::Renderer(RendererConfig const& config) :
	m_config(config)
{

}


void Renderer::EnableDebugLayer() const
{
#if defined(_DEBUG)
	ID3D12Debug3* debugController = nullptr;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
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
	}

	HRESULT castToAdapter4 = adapter1.As(&adapter);
	if (!SUCCEEDED(castToAdapter4)) {
		ERROR_AND_DIE("COULD NOT CAST ADAPTER1 TO ADAPTER4");
	}

	return adapter;
}



void Renderer::CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	HRESULT deviceCreationResult = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device));

	if (!SUCCEEDED(deviceCreationResult)) {
		ERROR_AND_DIE("COULD NOT CREATE DIRECTX12 DEVICE");
	}

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(m_device.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
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
			if (SUCCEEDED(dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
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

	if (!SUCCEEDED(swapChainCreationResult)) {
		ERROR_AND_DIE("COULD NOT CREATE SWAPCHAIN1");
	}

	if (!SUCCEEDED(swapChain1.As(&m_swapChain))) {
		ERROR_AND_DIE("COULD NOT CONVERT SWAPCHAIN1 TO SWAPCHAIN4");
	}

}

void Renderer::CreateDescriptorHeap(ComPtr<ID3D12DescriptorHeap>& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	HRESULT descHeapCreation = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));

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

	for (unsigned int frameBufferInd = 0; frameBufferInd < m_config.m_backBuffersCount; frameBufferInd++) {
		ComPtr<ID3D12Resource2>& backBuffer = m_backBuffers[frameBufferInd];
		HRESULT fetchBuffer = m_swapChain->GetBuffer(frameBufferInd, IID_PPV_ARGS(&backBuffer));
		if (!SUCCEEDED(fetchBuffer)) {
			ERROR_AND_DIE("COULD NOT GET FRAME BUFFER");
		}

		backBuffer.Reset();
		m_device->CreateRenderTargetView(m_backBuffers[frameBufferInd].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(m_RTVdescriptorSize);

	}


}

void Renderer::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>& commandAllocator)
{
	HRESULT commAllocatorCreation = m_device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));
	if (!SUCCEEDED(commAllocatorCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE COMMAND ALLOCATOR");
	}
}

void Renderer::CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator>const& commandAllocator)
{
	HRESULT commListCreation = m_device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
	if (!SUCCEEDED(commListCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE COMMAND LIST");
	}
}

void Renderer::CreateFence()
{
	HRESULT fenceCreation = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));

	if (!SUCCEEDED(fenceCreation)) {
		ERROR_AND_DIE("COULD NOT CREATE FENCE");
	}
}

void Renderer::CreateFenceEvent()
{
	m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Renderer::CreateRootSignature()
{

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "COULD NOT SERIALIZE ROOT SIGNATURE");
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)), "COULD NOT CREATE ROOT SIGNATURE");

}

unsigned int Renderer::SignalFence(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int& fenceValue)
{
	HRESULT fenceSignal = commandQueue->Signal(fence.Get(), fenceValue);


	if (!SUCCEEDED(fenceSignal)) {
		ERROR_AND_DIE("FENCE SIGNALING FAILED");
	}

	return fenceValue + 1;

}

void Renderer::WaitForFenceValue(ComPtr<ID3D12Fence1> fence, unsigned int fenceValue, HANDLE fenceEvent)
{
	if (fence->GetCompletedValue() < fenceValue) {
		HRESULT eventOnCompletion = fence->SetEventOnCompletion(fenceValue, fenceEvent);
		if (!SUCCEEDED(eventOnCompletion)) {
			ERROR_AND_DIE("FAILED TO SET EVENT ON COMPLETION FOR FENCE");
		}
		::WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}
}

void Renderer::Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence1> fence, unsigned int* fenceValues, HANDLE fenceEvent)
{
	unsigned int currentValue = fenceValues[m_currentBackBuffer];
	unsigned int newFenceValue = SignalFence(commandQueue, fence, currentValue);

	m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
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
	// Enable Debug Layer before initializing any DX12 object
	EnableDebugLayer();
	CreateDXGIFactory();
	ComPtr<IDXGIAdapter4> adapter = GetAdapter();
	CreateDevice(adapter);
	CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	CreateSwapChain();
	CreateDescriptorHeap(m_RTVdescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_config.m_backBuffersCount);
	CreateRenderTargetViews();

	m_commandAllocators.resize(m_config.m_backBuffersCount);
	for (int frameIndex = 0; frameIndex < m_config.m_backBuffersCount; frameIndex++) {
		CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[frameIndex]);
	}
	CreateFenceEvent();

	CreateRootSignature();

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

bool Renderer::CreateInputLayoutFromVS(std::vector<uint8_t>& shaderByteCode, D3D12_INPUT_LAYOUT_DESC& pInputLayout, std::vector<D3D12_INPUT_ELEMENT_DESC>& elementsDescs)
{
	// Reflect shader info
	ID3D12ShaderReflection* pVertexShaderReflection = NULL;
	if (FAILED(D3DReflect((void*)shaderByteCode.data(), shaderByteCode.size(), IID_ID3D11ShaderReflection, (void**)&pVertexShaderReflection)))
	{
		return false;
	}

	// Get shader info
	D3D12_SHADER_DESC shaderDesc;
	pVertexShaderReflection->GetDesc(&shaderDesc);


	// Read input layout description from shader info
	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
		pVertexShaderReflection->GetInputParameterDesc(i, &paramDesc);

		// fill out input element desc
		D3D12_INPUT_ELEMENT_DESC elementDesc;
		elementDesc.SemanticName = paramDesc.SemanticName;
		elementDesc.SemanticIndex = paramDesc.SemanticIndex;
		elementDesc.InputSlot = 0;
		elementDesc.AlignedByteOffset = (i == 0) ? 0 : D3D12_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;

		// determine DXGI format
		if (paramDesc.Mask == 1)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
		}
		else if (paramDesc.Mask <= 3)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (paramDesc.Mask <= 7)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (paramDesc.Mask <= 15)
		{
			if (AreStringsEqualCaseInsensitive(elementDesc.SemanticName, "COLOR")) {
				elementDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			else {
				if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
				else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			}


		}

		//save element desc
		elementsDescs.push_back(elementDesc);
	}

	pInputLayout.NumElements = elementsDescs.size();
	pInputLayout.pInputElementDescs = elementsDescs.data();

	return true;
}

Shader* Renderer::CreateShader(char const* shaderName, char const* shaderSource)
{
	ShaderConfig config;
	config.m_name = shaderName;

	Shader* newShader = new Shader(config);
	std::string baseName = shaderName;
	std::string debugName;

	std::vector<uint8_t> vertexShaderByteCode;
	CompileShaderToByteCode(vertexShaderByteCode, shaderName, shaderSource, config.m_vertexEntryPoint.c_str(), "vs_5_0", m_antiAliasingLevel);
	CD3DX12_SHADER_BYTECODE(vertexShaderByteCode.data(), vertexShaderByteCode.size());
	

	debugName = baseName + "|VShader";
	SetDebugName(newShader->m_vertexShader, debugName.c_str());

	std::string shaderNameAsString(shaderName);

	CreateInputLayoutFromVS(vertexShaderByteCode, &newShader->m_inputLayout);


	std::vector<uint8_t> pixelShaderByteCode;
	CompileShaderToByteCode(pixelShaderByteCode, shaderName, shaderSource, config.m_pixelEntryPoint.c_str(), "ps_5_0");
	m_device->CreatePixelShader(pixelShaderByteCode.data(), pixelShaderByteCode.size(), NULL, &newShader->m_pixelShader);

	debugName = baseName + "|PShader";
	SetDebugName(newShader->m_pixelShader, debugName.c_str());

	/*
	* Pixel Shader is compiled twice to support toggling antialiasing on runtime
	*/
	pixelShaderByteCode.clear();
	CompileShaderToByteCode(pixelShaderByteCode, shaderName, shaderSource, config.m_pixelEntryPoint.c_str(), "ps_5_0", true);
	m_device->CreatePixelShader(pixelShaderByteCode.data(), pixelShaderByteCode.size(), NULL, &newShader->m_MSAAPixelShader);

	debugName = baseName + "|MSAA-PShader";
	SetDebugName(newShader->m_MSAAPixelShader, debugName.c_str());

	std::vector<uint8_t> geometryShaderByteCode;

	bool compiledGeometryShader = CompileShaderToByteCode(geometryShaderByteCode, shaderName, shaderSource, config.m_geometryEntryPoint.c_str(), "gs_5_0");

	if (compiledGeometryShader) {
		if (createWithStreamOutput) {

			D3D11_SO_DECLARATION_ENTRY* DXSOEntries = new D3D11_SO_DECLARATION_ENTRY[numDeclarationEntries];
			for (int entryInd = 0; entryInd < numDeclarationEntries; entryInd++) {
				D3D11_SO_DECLARATION_ENTRY newEntry = {};
				SODeclarationEntry const& currentEntry = soEntries[entryInd];

				newEntry.Stream = 0;
				newEntry.SemanticName = currentEntry.SemanticName.c_str();
				newEntry.SemanticIndex = currentEntry.SemanticIndex;
				newEntry.StartComponent = currentEntry.StartComponent;
				newEntry.ComponentCount = currentEntry.ComponentCount;
				newEntry.OutputSlot = currentEntry.OutputSlot;
				DXSOEntries[entryInd] = newEntry;
			}
			if (DXSOEntries) {
				m_device->CreateGeometryShaderWithStreamOutput(geometryShaderByteCode.data(), geometryShaderByteCode.size(), DXSOEntries, numDeclarationEntries, NULL, 0, 0, NULL, &newShader->m_geometryShader);
			}
			delete[] DXSOEntries;
		}
		else {
			m_device->CreateGeometryShader(geometryShaderByteCode.data(), geometryShaderByteCode.size(), NULL, &newShader->m_geometryShader);
		}
		debugName = baseName;
		debugName += "|GShader";

		SetDebugName(newShader->m_geometryShader, debugName.c_str());
	}


	std::vector<uint8_t> hullShaderByteCode;

	bool compiledHullShader = CompileShaderToByteCode(hullShaderByteCode, shaderName, shaderSource, config.m_hullShaderEntryPoint.c_str(), "hs_5_0");

	if (compiledHullShader) {
		m_device->CreateHullShader(hullShaderByteCode.data(), hullShaderByteCode.size(), NULL, &newShader->m_hullShader);
		debugName = baseName;
		debugName += "|HShader";

		SetDebugName(newShader->m_hullShader, debugName.c_str());
	}

	std::vector<uint8_t> domainShaderByteCode;

	bool compiledDomainShader = CompileShaderToByteCode(domainShaderByteCode, shaderName, shaderSource, config.m_domainShaderEntryPoint.c_str(), "ds_5_0");

	if (compiledDomainShader) {
		m_device->CreateDomainShader(domainShaderByteCode.data(), domainShaderByteCode.size(), NULL, &newShader->m_domainShader);
		debugName = baseName;
		debugName += "|DShader";

		SetDebugName(newShader->m_domainShader, debugName.c_str());
	}

	std::vector<uint8_t> computeShaderByteCode;

	bool compiledComputeShader = CompileShaderToByteCode(computeShaderByteCode, shaderName, shaderSource, config.m_computeShaderEntryPoint.c_str(), "cs_5_0");

	if (compiledComputeShader) {
		m_device->CreateComputeShader(computeShaderByteCode.data(), computeShaderByteCode.size(), NULL, &newShader->m_computeShader);
		debugName = baseName;
		debugName += "|CShader";

		SetDebugName(newShader->m_computeShader, debugName.c_str());
	}

	m_loadedShaders.push_back(newShader);

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
	object->SetName(name);
#else
	UNUSED(object);
	UNUSED(name);
#endif
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

void Renderer::BeginFrame()
{
	//DebugRenderBeginFrame();


	ComPtr<ID3D12CommandAllocator> commandAllocator = m_commandAllocators[m_currentBackBuffer];
	ComPtr<ID3D12Resource1> backBuffer = GetActiveColorTarget();

	commandAllocator->Reset();
	m_commandList->Reset(commandAllocator.Get(), nullptr);


#if defined(ENGINE_USE_IMGUI)
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void Renderer::EndFrame()
{
	//DebugRenderEndFrame();
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		GetActiveColorTarget().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	m_commandList->ResourceBarrier(1, &barrier);

	// Close command list
	HRESULT commandListClose = m_commandList->Close();
	if (!SUCCEEDED(commandListClose)) {
		ERROR_AND_DIE("COULD NOT CLOSE COMMAND LIST");
	}
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };

	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	m_swapChain->Present(1, 0);

	// Flush Command Queue getting ready for next Frame
	Flush(m_commandQueue, m_fence, m_fenceValues, m_fenceEvent);


}

void Renderer::Shutdown()
{

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
