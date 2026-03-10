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

int WINAPI WinMain(
    HINSTANCE hInstance,      // Handle to the current instance of the application
    HINSTANCE hPrevInstance,  // Legacy parameter (always NULL in modern Windows)
    LPSTR     lpCmdLine,      // The command line arguments as a single string
    int       nCmdShow        // Flags that determine how the window should be shown
)
{
	int argc;
	wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::vector<char*> argv_buffer(argc + 1);
	char** argv = nullptr;

	int ret = EE::application_main(EE::application_create, argc, argv);

	return ret;
}