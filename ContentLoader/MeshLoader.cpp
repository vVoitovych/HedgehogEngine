#include "MeshLoader.hpp"
#include "CommonFunctions.hpp"

#include "ObjLoader.hpp"
#include "GltfMeshLoader.hpp"

#include <filesystem>
#include <stdexcept>

namespace ContentLoader
{

	LoadedMesh LoadMesh(const std::string& fileName)
	{
		std::string path = GetAssetsDirectory() + fileName;
		std::string extension = std::filesystem::path(path).extension().string();
		if (extension == ".obj")
		{
			return LoadObj(path);
		}
		else if (extension == ".gltf")
		{
			return LoadGltfMesh(path);
		}
		else
		{
			throw std::runtime_error("unsuported file format");
		}

	}


}



