#include "MaterialSystem.hpp"
#include "Engine/Renderer/Material.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include <string>

MaterialSystem::MaterialSystem(MaterialSystemConfig const& config) :
	m_config(config)
{

}

void MaterialSystem::Startup()
{
	LoadEngineMaterials();
}

void MaterialSystem::Shutdown()
{
	for (Material*& mat : m_loadedMaterials) {
		delete mat;
		mat = nullptr;
	}
}

void MaterialSystem::BeginFrame()
{

}

void MaterialSystem::EndFrame()
{

}

Material* MaterialSystem::CreateOrGetMaterial(std::filesystem::path& materialPathNoExt)
{
	std::string materialXMLPath = materialPathNoExt.replace_extension(".xml").string();
	std::string materialName = materialPathNoExt.filename().replace_extension("").string();
	Material* material = g_theMaterialSystem->GetMaterialForPath(materialXMLPath.c_str());
	if (material) {
		return material;
	}

	return CreateMaterial(materialXMLPath);
}



Material* MaterialSystem::GetMaterialForName(std::string const& materialNameNoExt)
{
	for (int matIndex = 0; matIndex < m_loadedMaterials.size(); matIndex++) {
		Material* material = m_loadedMaterials[matIndex];
		if (material->GetName() == materialNameNoExt) {
			return material;
		}
	}
	return nullptr;
}


Material* MaterialSystem::GetMaterialForPath(std::filesystem::path const& materialPath)
{
	for (int matIndex = 0; matIndex < m_loadedMaterials.size(); matIndex++) {
		Material* material = m_loadedMaterials[matIndex];
		if (material->GetPath() == materialPath.string()) {
			return material;
		}
	}
	return nullptr;
}


Material* MaterialSystem::CreateMaterial(std::string const& materialXMLFile)
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
	m_config.m_renderer->CreatePSOForMaterial(newMat);

	m_loadedMaterials.push_back(newMat);

	return newMat;
}

void MaterialSystem::LoadEngineMaterials()
{
	std::string materialsPath = ENGINE_MAT_DIR;
	for (auto const& matPath : std::filesystem::directory_iterator(materialsPath)) {
		if (matPath.is_directory()) continue;

		CreateMaterial(matPath.path().string());
	}
}
