#pragma once

struct RenderSettings {
    bool frustum_culling = true;
    bool wireframe       = false;
    bool draw_grid       = true;
    bool draw_skybox     = true;
    bool use_pbr_lighting = true;
    bool show_timings       = false;
    bool show_timing_graphs = false;
    bool draw_debug_aabbs = false;
};
