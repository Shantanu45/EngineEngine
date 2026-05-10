/*****************************************************************//**
 * \file   application_options.h
 * \brief
 *
 * \author Shantanu Kumar
 * \date   May 2026
 *********************************************************************/
#pragma once
#include <string>

namespace EE
{
	struct AppOptions
	{
		std::string assets_path;
		unsigned width  = 0;     // 0 = use application default
		unsigned height = 0;     // 0 = use application default
		bool fullscreen = false;
		bool vsync      = true;
		std::string render_mode = "pbr"; // "regular" or "pbr"
	};
}
