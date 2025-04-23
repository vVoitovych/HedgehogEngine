#pragma once

namespace Context
{
	class Context;
}

namespace Renderer
{
	class MaterialEditor
	{
	public:
		static void Draw(Context::Context& context);
		static void ShowMaterialWindow();

	private:
		static bool s_MaterialWindowShow;
	};

}


