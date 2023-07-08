#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include "Engine/Renderer/GraphicsCommon.hpp"

struct ID3D12PipelineState;
struct CD3DX12_RASTERIZER_DESC;
struct CD3DX12_BLEND_DESC;

struct ShaderConfig
{
	std::string m_name = "Unnamed";
	std::string m_vertexEntryPoint = "VertexMain";
	std::string m_pixelEntryPoint = "PixelMain";
	std::string m_geometryEntryPoint = "GeometryMain";
	std::string m_hullShaderEntryPoint = "HullMain";
	std::string m_domainShaderEntryPoint = "DomainMain";
	std::string m_computeShaderEntryPoint = "ComputeMain";
};

class Shader
{
	friend class Renderer;

public:
	Shader(const Shader& copy) = delete;
private:
	Shader(const ShaderConfig& config);
	~Shader();

	const std::string& GetName() const;

private:
	ShaderConfig		m_config;

	std::vector<uint8_t> m_VSByteCode;
	std::vector<uint8_t> m_PSByteCode;
	std::vector<uint8_t> m_cachedPSO;
	
	ID3D12PipelineState* m_PSO = nullptr;
	BlendMode m_blendMode = BlendMode::OPAQUE;
	DepthTest m_depthFunc = DepthTest::LESSEQUAL;
	CullMode m_cullMode = CullMode::BACK;
	WindingOrder m_windingOrder = WindingOrder::COUNTERCLOCKWISE;
	TopologyMode m_topology = TopologyMode::TRIANGLELIST;

	bool m_depthEnable = false;
	bool m_stencilEnable = false;


};
