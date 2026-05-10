#include "rid_handle.h"
#include "rendering/rendering_device.h"

void RIDHandle::reset() {
    if (_rid.is_valid()) {
        Rendering::RenderingDevice::get_singleton()->free_rid(_rid);
        _rid = RID();
    }
}
