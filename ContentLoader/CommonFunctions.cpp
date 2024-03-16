#include "CommonFunctions.hpp"

#include <Windows.h>
#include <stdexcept>

namespace ContentLoader
{
	std::string GetCurrentDirectory()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::string::size_type pos = std::string(buffer).find_last_of("\\/");

		return std::string(buffer).substr(0, pos);
	}

	std::string GetAssetsDirectory()
	{
		std::string result = GetCurrentDirectory();
		result += "\\..\\..\\Assets\\";
		return result;
	}

}


