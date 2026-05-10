#include "forward/forward_ui_setup.h"

#include "forward/frame_stats_panel.h"
#include "forward/menu_bar.h"
#include "forward/renderer_timings_panel.h"

void register_forward_menu_panels(UILayer& ui_layer)
{
	ui_layer.add(std::make_unique<MenuBarPanel>());
}

void register_frame_stats_panels(UILayer& ui_layer)
{
	ui_layer.add(std::make_unique<FrameStatsPanel>());
}

void register_renderer_timing_panels(UILayer& ui_layer)
{
	ui_layer.add(std::make_unique<RendererTimingsPanel>());
}

void register_default_forward_panels(UILayer& ui_layer)
{
	register_forward_menu_panels(ui_layer);
	register_frame_stats_panels(ui_layer);
	register_renderer_timing_panels(ui_layer);
}
