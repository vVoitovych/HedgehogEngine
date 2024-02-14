#pragma once

#include <string>

namespace Scene
{
	class Scene;

	class SceneSerializer
	{
	public:
		static void SerializeScene(Scene& scene, std::string scenePath);

		static void DeserializeScene(Scene& scene, std::string scenePath);

	};
}




