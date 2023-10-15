#pragma once
#include "Engine/Math/IntVec2.hpp"
#include <vector>
#include <string>
struct ID3D12Resource2;

class TextureView;
class Renderer;

enum class TextureFormat : int {
		R8G8B8A8_UNORM,
		R32G32B32A32_FLOAT,
		R32G32_FLOAT,
		D24_UNORM_S8_UINT,
		R24G8_TYPELESS,
		R32_FLOAT
};

enum TextureBindFlagBit : unsigned int {
	TEXTURE_BIND_NONE = 0,
	TEXTURE_BIND_SHADER_RESOURCE_BIT = (1 << 0),
	TEXTURE_BIND_RENDER_TARGET_BIT = (1 << 1),
	TEXTURE_BIND_DEPTH_STENCIL_BIT = (1 << 2),
	TEXTURE_BIND_UNORDERED_ACCESS_VIEW_BIT = (1 << 3)
};

typedef unsigned int TextureBindFlag;
struct TextureCreateInfo {
	std::string m_name = "Unnamed Texture";
	Renderer* m_owner = nullptr;
	IntVec2 m_dimensions = IntVec2::ZERO;
	TextureBindFlag m_bindFlags = TEXTURE_BIND_NONE;
	TextureFormat m_format = TextureFormat::R8G8B8A8_UNORM;
	bool m_isMultiSample = false;
	ID3D12Resource2* m_handle = nullptr;
};


class Texture {
friend class Renderer;
public:
	/// <summary>
	/// Checks if view has been created, or creates it otherwise
	/// </summary>
	void CheckOrCreateView(TextureBindFlagBit viewType);
	IntVec2 GetDimensions() const;
private:
	/// Views are created in DX12 but they're not a separate structure
	void CreateShaderResourceView();
	void CreateRenderTargetView();
	void CreateDepthStencilView();
	Texture(TextureCreateInfo const& createInfo);

private:
	Renderer* m_owner = nullptr;
	TextureCreateInfo m_creationInfo;
	ID3D12Resource2* m_handle = nullptr;
	unsigned int m_createdViews = 0;
};