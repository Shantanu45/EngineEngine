#pragma once

#include "render_doc\renderdoc_app.h"
#include "libassert\assert.hpp"
#if defined(_WIN32)
#include <Windows.h>
#endif

namespace Util
{

    inline RENDERDOC_API_1_7_0* rdoc = nullptr;
    inline bool pending_capture = false;

    inline void init_render_doc(const std::string& rdoc_dir, const std::string& capture_dir) {
#if defined(_WIN32)
        // Try already-injected first
        HMODULE lib = GetModuleHandleA("renderdoc.dll");

        // If not injected, load it manually
        if (!lib) {
            std::string rdoc_dll = rdoc_dir.empty() ? "C:/Program Files/RenderDoc/renderdoc.dll" : rdoc_dir + "renderdoc.dll";
            lib = LoadLibraryA(rdoc_dll.c_str());
        }

        if (lib) {
            pRENDERDOC_GetAPI getAPI =
                (pRENDERDOC_GetAPI)GetProcAddress(lib, "RENDERDOC_GetAPI");
            if (getAPI) {
                int ret = getAPI(eRENDERDOC_API_Version_1_7_0, (void**)&rdoc);
                DEBUG_ASSERT(ret == 1);
                if (!capture_dir.empty())
                {
                    rdoc->SetCaptureFilePathTemplate(capture_dir.c_str());
                }
            }
        }
#else
#error add implementation
#endif
    }

    inline void capture_next_frame() {
        if (rdoc) {
            rdoc->TriggerCapture(); // captures the next frame
            pending_capture = true;
        }
        else
        {
            DEBUG_ASSERT(false, "renderdoc not initialized");
        }
    }

    inline void open_last_captured()
    {
        if (rdoc)
        {
            rdoc->LaunchReplayUI(1, nullptr);
        }
        else
        {
            DEBUG_ASSERT(false, "renderdoc not initialized");
        }
    }

    inline uint32_t get_rdoc_num_captures()
    {
        return rdoc->GetNumCaptures();
    }

    inline void shutdown_render_doc() {
        rdoc = nullptr;  // stop using the API first

        HMODULE lib = GetModuleHandleA("renderdoc.dll");
        if (lib) {
            FreeLibrary(lib);  // triggers DLL_PROCESS_DETACH -> RenderDoc shuts down
        }
    }
}
