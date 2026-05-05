/*****************************************************************//**
 * \file   application_entry.cpp
 * \brief  
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/
#include "application_entry.h"
#include <vector>
#include <string>

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

	std::vector<std::string> narrow(argc);
	std::vector<char*> argv_ptrs(argc + 1, nullptr);
	for (int i = 0; i < argc; i++)
	{
		int len = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
		narrow[i].resize(len);
		WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, narrow[i].data(), len, nullptr, nullptr);
		argv_ptrs[i] = narrow[i].data();
	}
	LocalFree(wide_argv);

	int ret = EE::application_main(EE::application_create, argc, argv_ptrs.data());

	return ret;
}