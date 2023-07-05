#pragma once
#include <string>
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11GeometryShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11ComputeShader;
struct ID3D11InputLayout;

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
	Shader(const ShaderConfig& config);
	Shader(const Shader& copy) = delete;
	~Shader();

	const std::string& GetName() const;

	ShaderConfig		m_config;
	ID3D12Resource* m_vertexShader = nullptr;
	ID3D12Resource* m_pixelShader = nullptr;
};
