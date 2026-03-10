#pragma once

#include <Windows.h>
#include <cstdio>
#include <memory>
#include "util/logger.h"

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace EE
{
	enum class ApplicationQuery
	{
		DefaultManagerFlags
	};

	class Application;

	int application_main(Application* (*create_application)(int, char**), int argc, char** argv);

	extern Application* application_create(int argc, char* argv[]);

	struct ApplicationQueryDefaultManagerFlags
	{
		uint32_t manager_feature_flags;
	};

	extern bool query_application_interface(ApplicationQuery query, void* data, size_t size);

	// Call this or setup_default_filesystem to ensure application-main is linked in correctly without having to mess around
	// with -Wl,--whole-archive.
	void application_dummy();

	void application_setup_default_filesystem(const char* default_asset_directory);
}
									 
#define EE_APPLICATION_SETUP ::EE::application_dummy()

