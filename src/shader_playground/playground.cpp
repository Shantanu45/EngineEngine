#include "application/common.h"

struct ShaderPlayground : EE::Application
{
	bool pre_frame() override
	{
		return true;
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
	}
};

namespace EE
{
	Application* application_create(int, char**)
	{
		EE_APPLICATION_SETUP;

		try {
			auto* app = new ShaderPlayground();
			return app;
		}
		catch (const std::exception& e) {
			LOGE("application_create() threw exception: %s\n", e.what());
			return nullptr;
		}
	}
}