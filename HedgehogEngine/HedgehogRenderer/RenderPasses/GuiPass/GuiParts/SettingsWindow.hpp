#pragma once

namespace Context
{
	class Context;
}

namespace Renderer
{
	class SettingsWindow
	{
	public:
		static void Draw(Context::Context& context);
		static void ShowMaterialWindow();

	private:
		static bool s_MaterialWindowShow;

	};

}


