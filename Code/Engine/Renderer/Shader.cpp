#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/GraphicsCommon.hpp"


Shader::Shader(const ShaderConfig& config) :
	m_config(config)
{

}

Shader::~Shader()
{
	DX_SAFE_RELEASE(m_PSO);
	m_PSO = nullptr;

}

const std::string& Shader::GetName() const
{
	return m_config.m_name;
}
