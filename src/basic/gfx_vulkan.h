#ifndef GFX_VULKAN_H
#define GFX_VULKAN_H
#include <basic/basic.h>

uint32_t render_device_init_vulkan(const struct window *window);
void     render_device_terminate_vulkan(void);
void     render_device_on_resize(uint32_t width, uint32_t height);

#endif /* GFX_VULKAN_H */
