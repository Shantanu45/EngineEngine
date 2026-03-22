/*****************************************************************//**
 * \file   application_entry.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "application_entry.h"

namespace EE
{
	// Make sure this is linked in.
	void application_dummy()
	{
	}

	// Alternatively, make sure this is linked in.
	// Implementation is here to trick a linker to always let main() in static library work.
	void application_setup_default_filesystem(const char* default_asset_directory)
	{
	}
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argc;
	wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::vector<char*> argv_buffer(argc + 1);
	char** argv = nullptr;

	int ret = EE::application_main(EE::application_create, argc, argv);

	return ret;
}