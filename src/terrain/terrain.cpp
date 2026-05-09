#include "application/common.h"

#include "filesystem/filesystem.h"
#include "input/input.h"
#include "terrain/terrain_runtime.h"

namespace {

struct TerrainApplication : EE::Application {
	bool pre_frame() override
	{
		auto input_system = Services::get().get<EE::InputSystemInterface>();
		auto fs_ptr = Services::get().get<FileSystem::FilesystemInterface>();
		auto& filesystem = static_cast<FileSystem::Filesystem&>(*fs_ptr);
		return runtime.initialize(get_wsi(), filesystem, input_system);
	}

	void render_frame(double frame_time, double elapsed_time) override
	{
		runtime.render_frame(frame_time, elapsed_time);
	}

	void teardown_application() override
	{
		runtime.shutdown();
	}

	std::string get_name() override
	{
		return "terrain";
	}

private:
	Terrain::TerrainRuntime runtime;
};

} // namespace

namespace EE {

Application* application_create(int, char**)
{
	EE_APPLICATION_SETUP;

	try {
		return new TerrainApplication();
	}
	catch (const std::exception& e) {
		LOGE("application_create() threw exception: %s\n", e.what());
		return nullptr;
	}
}

} // namespace EE
