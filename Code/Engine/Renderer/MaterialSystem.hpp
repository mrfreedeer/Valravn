#pragma once
#include <vector>
#include <string>
#include <filesystem>

class Material;
class Renderer;

struct MaterialSystemConfig {
	Renderer* m_renderer = nullptr;
};

class MaterialSystem {
public:
	MaterialSystem(MaterialSystemConfig const& config);
	void Startup();
	void Shutdown();
	void BeginFrame();
	void EndFrame();

	Material* CreateOrGetMaterial(std::filesystem::path& materialPathNoExt);
	Material* GetMaterialForName(std::string const& materialNameNoExt);
	Material* GetMaterialForPath(std::filesystem::path const& materialPath);
	Material* CreateMaterial(std::string const& materialXMLFile);
private:
	void LoadEngineMaterials();
private:
	MaterialSystemConfig m_config = {};
	std::vector<Material*> m_loadedMaterials;
};