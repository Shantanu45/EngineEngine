#pragma once

struct RenderSettings {
    bool frustum_culling = true;
    bool wireframe       = false;
    bool draw_grid       = true;
    bool draw_skybox     = true;
    bool show_timings    = false;
    bool draw_debug_aabbs = false;

    int last_draw_count  = 0;  // updated each frame by scene builder
};
