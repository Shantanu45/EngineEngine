#include "deferred/deferred_ui_setup.h"

#include "deferred/frame_stats_panel.h"
#include "deferred/menu_bar.h"
#include "deferred/renderer_timings_panel.h"

void register_deferred_menu_panels(UILayer& ui_layer)
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

void register_default_deferred_panels(UILayer& ui_layer)
{
	register_deferred_menu_panels(ui_layer);
	register_frame_stats_panels(ui_layer);
	register_renderer_timing_panels(ui_layer);
}
