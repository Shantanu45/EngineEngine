#pragma once

enum class ToneMapper : int {
    None = 0,
    Reinhard = 1,
    ACES = 2,
};

enum class MaterialDebugView : int {
    Lit = 0,
    Albedo = 1,
    Normal = 2,
    Roughness = 3,
    Metallic = 4,
    AmbientOcclusion = 5,
    Emissive = 6,
    ShadowFactor = 7,
    LightCount = 8,
    Depth = 9,
    DirectionalShadowMap = 10,
    LightSpaceCoords = 11,
    CascadeBoundaries = 12,
};

enum class DirectionalShadowMode : int {
    SingleMap = 0,
    Cascaded = 1,
};

struct RenderSettings {
    bool frustum_culling = true;
    bool wireframe       = false;
    bool draw_grid       = true;
    bool draw_skybox     = true;
    bool draw_world_axes = true;
    bool use_pbr_lighting = true;
    bool show_timings       = false;
    bool show_timing_graphs = false;
    bool draw_debug_aabbs = false;
    bool use_debug_culling_camera = false;
    bool render_from_debug_camera = false;
    bool debug_camera_detached = false;
    bool draw_render_frustum = false;
    bool draw_culling_frustum = false;
    bool draw_culling_results = false;
    float exposure = 1.0f;
    float directional_shadow_bias_scale = 0.002f;
    float directional_shadow_bias_min = 0.0005f;
    float point_shadow_bias_max = 0.005f;
    float point_shadow_bias_min = 0.0001f;
    ToneMapper tone_mapper = ToneMapper::ACES;
    MaterialDebugView material_debug_view = MaterialDebugView::Lit;
    DirectionalShadowMode directional_shadow_mode = DirectionalShadowMode::SingleMap;
};
