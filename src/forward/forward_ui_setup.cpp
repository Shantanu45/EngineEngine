#include "forward/forward_ui_setup.h"

#include "forward/debug_stats_panel.h"
#include "forward/menu_bar.h"

void register_default_forward_panels(UILayer& ui_layer)
{
	ui_layer.add(std::make_unique<MenuBarPanel>());
	ui_layer.add(std::make_unique<DebugStatsPanel>());
}
