#pragma once

#include <memory>
#include <vector>

#include "rendering/camera.h"
#include "entt/entt.hpp"

struct UIContext {
    Camera*         camera     = nullptr;
    entt::registry* world      = nullptr;
    bool            show_stats = true;
};

struct IUIPanel {
    virtual void draw(UIContext& ctx) = 0;
    virtual ~IUIPanel()               = default;
};

class UILayer {
public:
    void add(std::unique_ptr<IUIPanel> panel) { panels.push_back(std::move(panel)); }

    void draw_frame(UIContext& ctx) {
        for (auto& p : panels)
            p->draw(ctx);
    }

private:
    std::vector<std::unique_ptr<IUIPanel>> panels;
};
