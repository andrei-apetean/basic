#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "framework.h"
#include "ui_frag.h"
#include "ui_vert.h"

#define check(err, msg) do{if (err){puts((msg)); return err;}} while (0);
#define checkv(err, msg, ...) do{if (err){    \
    printf((msg), __VA_ARGS__); return err;}} \
    while (0);

#define MAX_SWAPCHAIN_IMAGES 8
#define FRAMES_IN_FLIGHT 3

/* forward declarations - bad idea?*/

typedef VkFlags VkWin32SurfaceCreateFlagsKHR;
typedef struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType                 sType;
    const void*                     pNext;
    VkWin32SurfaceCreateFlagsKHR    flags;
    HINSTANCE                       hinstance;
    HWND                            hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (VKAPI_PTR *PFN_vkCreateWin32SurfaceKHR)(VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkSurfaceKHR* pSurface);

/* vulkan instance layers needed by this framework */
enum vulkan_instance_layers {
    VULKAN_INSTANCE_LAYER_VALIDATION = 0,
    VULKAN_INSTANCE_LAYER_COUNT,
};

/* vulkan instance extensions needed by this framework */
enum vulkan_instance_ext {
    VULKAN_INSTANCE_EXT_SURFACE = 0,
    VULKAN_INSTANCE_EXT_SURFACE_WIN32,
    VULKAN_INSTANCE_EXT_SURFACE_WL,
    VULKAN_INSTANCE_EXT_SURFACE_X11,
    VULKAN_INSTANCE_EXT_DEBUG_REPORT,
    VULKAN_INSTANCE_EXT_DEBUG_UTILS,
    VULKAN_INSTANCE_EXT_COUNT,
};

/* vulkan device extensions needed by this framework */
enum vulkan_device_ext {
    VULKAN_DEVICE_EXT_SWAPCHAIN = 0,
    VULKAN_DEVICE_EXT_COUNT,
};

struct vulkan_buffer {
    VkBuffer       handle;
    VkDeviceMemory memory;
    void*          mapped;
    VkDeviceSize   size;
};

struct vulkan_image {
    VkImage            handle;
    VkImageView        view;
    VkDeviceMemory     device_memory;

    VkExtent2D         extent;
    VkFormat           fmt;
    VkImageLayout      layout;
    VkImageAspectFlags aspect_mask;
};

struct vulkan_frame_data {
    VkCommandPool   pool;
    VkCommandBuffer buffer;
    VkSemaphore     img_ready_semaphore;
    VkFence         render_fence;
};

struct vulkan_pipeline {
    VkPipeline       pipeline;
    VkPipelineLayout layout;
};

struct vulkan_state {
    VkInstance               instance;
    VkAllocationCallbacks*   alloc;
    VkSurfaceKHR             surface;
    VkDevice                 device;
    VkPhysicalDevice         physical_device;
    VkQueue                  gfx_queue;
    uint32_t                 gfx_family;
    uint32_t                 debug;
    VkSwapchainKHR           swapchain;
    VkSurfaceFormatKHR       swapchain_surface_fmt;
    VkExtent2D               swapchain_extent;
    uint32_t                 swapchain_image_count;
    uint32_t                 current_frame;
    uint32_t                 image_index;
    VkCommandPool            upload_pool;
    struct vulkan_frame_data frame_data[FRAMES_IN_FLIGHT];
    struct vulkan_image      swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkSemaphore              render_finished[MAX_SWAPCHAIN_IMAGES];
    struct vulkan_pipeline   pipelines[BUILTIN_RENDER_PIPELINE_COUNT];
    VkDebugUtilsMessengerEXT messenger;

    uint32_t                 window_width;
    uint32_t                 window_height;
    struct vulkan_buffer     ui_instance_buffer;
};

#define MAX_UI_ELEMENTS 256
struct ui_instance {
    float x, y, w, h;
    float r, g, b, a;
};

static struct vulkan_state g_vk;

static VkBool32
debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT types,
                     const VkDebugUtilsMessengerCallbackDataEXT* data,
                     void* user_data);

static uint32_t vulkan_create_swapchain(uint32_t width, uint32_t height);
static void     vulkan_destroy_swapchain(void);

static void vulkan_transition_image(VkCommandBuffer buffer,
        struct vulkan_image* img, VkImageLayout new);

static uint32_t vulkan_create_pipeline(const struct render_pipeline_config* cfg,
        struct vulkan_pipeline* pipeline);

static int32_t vulkan_find_memory_type(uint32_t filter, VkMemoryPropertyFlags flags);
static int32_t vulkan_find_gfx_queue(VkPhysicalDevice d, int32_t* idx, VkSurfaceKHR s);
static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice d, VkSurfaceKHR s);
static VkPresentModeKHR   vulkan_find_present_mod(VkPhysicalDevice d, VkSurfaceKHR s);


static uint32_t vulkan_buffer_alloc(struct vulkan_buffer *buf, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags flags);
uint32_t vulkan_buffer_create(struct vulkan_buffer *buf, const void *data,
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags);
void vulkan_buffer_destroy(struct vulkan_buffer *buf);
static void vulkan_buffer_copy(VkBuffer src, VkBuffer dst, VkDeviceSize size);

uint32_t render_device_init_vulkan(const struct os_window* window)
{
    VkResult err = VK_SUCCESS;

    g_vk.debug = 1;
    { /* instance creation */
        uint32_t extension_count = 0;
        const char* extensions[VULKAN_INSTANCE_EXT_COUNT] = { 0 };
        extensions[extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
        switch (window->backend) {
            case WINDOW_BACKEND_WIN32:
                extensions[extension_count++] = "VK_KHR_win32_surface";
                break;
            case WINDOW_BACKEND_WAYLAND:
                extensions[extension_count++] = "VK_KHR_wayland_surface";
                break;
            case WINDOW_BACKEND_XCB:
                extensions[extension_count++] = "VK_KHR_xcb_surface";
                break;
            case WINDOW_BACKEND_COUNT:
                return VK_ERROR_UNKNOWN;
        }

        if (g_vk.debug) {
            extensions[extension_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        const char* layers[VULKAN_INSTANCE_LAYER_COUNT] = {
            "VK_LAYER_KHRONOS_validation",
        };

        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "runa",
            .pEngineName = "runa",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = VK_MAKE_VERSION(1, 3, 0),
        };

        /* todo: validate extensions/layers */
        VkInstanceCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .ppEnabledExtensionNames = extensions,
            .enabledExtensionCount = extension_count,
            .ppEnabledLayerNames = layers,
            .enabledLayerCount = g_vk.debug ? count_of(layers) : 0,
        };

        err = vkCreateInstance( &ci, g_vk.alloc, &g_vk.instance);
    }
    check(err,"[inf][vk] failed to create instance\n");

    printf("[inf][vk] created instance\n"); 

    if (g_vk.debug) {
        const VkDebugUtilsMessengerCreateInfoEXT info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = debug_utils_callback,
        };
        PFN_vkCreateDebugUtilsMessengerEXT create_fn;
        create_fn = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk.instance, "vkCreateDebugUtilsMessengerEXT");

        if (create_fn) {
            err = create_fn(g_vk.instance, &info, 0, &g_vk.messenger);
            check(err,"[inf][vk] failed to create debug messenger\n");
            printf("[inf][vk] created debug messenger\n"); 
        } else {
            printf("[inf][vk] failed to create debug messenger\n");
        }
    }
    { /* surface creation*/
        /* tbd: separate into swapchain?*/
        switch (window->backend) {
            case WINDOW_BACKEND_WIN32:
                PFN_vkCreateWin32SurfaceKHR create_surface = 
                    (PFN_vkCreateWin32SurfaceKHR)
                    vkGetInstanceProcAddr(g_vk.instance, "vkCreateWin32SurfaceKHR");
                if (!create_surface) {
                    return VK_ERROR_UNKNOWN;
                }
                VkWin32SurfaceCreateInfoKHR info = {
                    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                    .hwnd = window->win32.hwnd,
                    .hinstance = window->win32.hinstance,
                };
                err = create_surface(g_vk.instance, &info, g_vk.alloc, &g_vk.surface);
                check(err, "[err][vk] failed to create window surface\n");
                    break;
            case WINDOW_BACKEND_WAYLAND:
            case WINDOW_BACKEND_XCB:
            case WINDOW_BACKEND_COUNT:
                return VK_ERROR_UNKNOWN;
                break;
        }
        printf("[inf][vk] vulkan surface created\n");
    }

    { /* physical device selection */
        uint32_t dev_count = 0;
        err = vkEnumeratePhysicalDevices(g_vk.instance, &dev_count, NULL);
        if (err) return err;

        VkPhysicalDevice devices[dev_count];
        err = vkEnumeratePhysicalDevices(g_vk.instance, &dev_count, devices);
        if (err) return err;

        VkPhysicalDevice discrete_device = 0;
        VkPhysicalDevice integrated_device = 0;

        int32_t gfx_idx = 0;

        for (uint32_t i = 0; i < dev_count; i++) {
            VkPhysicalDeviceProperties props = {0};
            vkGetPhysicalDeviceProperties(devices[i], &props);
            printf("[inf][vk] evaluating device '%s'\n", props.deviceName);

            int suitable = vulkan_find_gfx_queue(devices[i], &gfx_idx, g_vk.surface);
            VkPhysicalDeviceType dev_type = props.deviceType;
            if ((dev_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) && suitable)
                discrete_device = devices[i];

            if ((dev_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) && suitable)
                integrated_device = devices[i];

            const char* types[] = { "other", "integrated_gpu", "discrete_gpu",
                "virtual_gpu", "cpu" };
            printf("\tapi_version: %d\n"
                    "\tdevice_id: %d\n"
                    "\tvendor_id: %d\n"
                    "\tdevice_type: %s\n"
                    "\tsupports_present: %s [queue_family %d]\n",
                    props.apiVersion,
                    props.deviceID,
                    props.vendorID,
                    types[props.deviceType],
                    suitable ? "true" : "false",
                    gfx_idx);
        }

        g_vk.physical_device = discrete_device ? discrete_device
            : integrated_device ? integrated_device
            : VK_NULL_HANDLE;

        err = g_vk.physical_device == VK_NULL_HANDLE;
        check(err, "[inf][vk] failed to find a suitable device\n");

        VkPhysicalDeviceProperties props = {0};
        vkGetPhysicalDeviceProperties(g_vk.physical_device, &props);
        printf("[inf][vk] selected device: '%s'\n", props.deviceName);

        int32_t queue_idx = 0;
        vulkan_find_gfx_queue(g_vk.physical_device, &queue_idx, g_vk.surface);
        g_vk.gfx_family = queue_idx;
    }

    { /* logical device creation */
        float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_cis[] = {
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = g_vk.gfx_family,
                .queueCount = 1,
                .pQueuePriorities = queue_priority,
            },
        };

        VkPhysicalDeviceFeatures2 features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features.samplerAnisotropy = VK_TRUE,
        };

        VkPhysicalDeviceVulkan12Features features_12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features,
            .bufferDeviceAddress = VK_TRUE,
            .descriptorIndexing = VK_TRUE,
        };

        VkPhysicalDeviceVulkan13Features features_13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .dynamicRendering = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .pNext = &features_12,
        };

        uint32_t dev_extension_count = 0;
        const char* dev_extensions[VULKAN_DEVICE_EXT_COUNT] = { 0 };
        dev_extensions[dev_extension_count++] =  VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo device_ci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = count_of(queue_cis),
            .pQueueCreateInfos = queue_cis,
            .enabledExtensionCount = dev_extension_count,
            .ppEnabledExtensionNames = dev_extensions,
            .pNext = &features_13,
        };

        err = vkCreateDevice(g_vk.physical_device, &device_ci, g_vk.alloc,
                             &g_vk.device);
        check(err, "[inf][vk] failed to create logical device\n");

        vkGetDeviceQueue(g_vk.device, g_vk.gfx_family, 0, &g_vk.gfx_queue);
        printf("[inf][vk] queues obtained\n");
    }

    {
        g_vk.window_width = window->width;
        g_vk.window_height = window->height;
        vulkan_create_swapchain(window->width, window->height);
    }

    { /* command objects */
        VkCommandPoolCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = g_vk.gfx_family,
        };

        err = vkCreateCommandPool(g_vk.device, &ci, g_vk.alloc, &g_vk.upload_pool);
        check(err, "[err][vk] failed to create upload pool\n");

        VkFenceCreateInfo fence_ci = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VkSemaphoreCreateInfo semaphore_ci = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            err = vkCreateCommandPool(g_vk.device, &ci, g_vk.alloc,
                    &g_vk.frame_data[i].pool);
            checkv(err, "[err][vk] failed to create command pool (%d)\n", i);

            VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = g_vk.frame_data[i].pool,
                .commandBufferCount = 1,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            };
            err = vkAllocateCommandBuffers(g_vk.device, &alloc_info,
                    &g_vk.frame_data[i].buffer);
            checkv(err, "[err][vk] failed to allocate command buffer (%d)\n", i);

            err = vkCreateFence(g_vk.device, &fence_ci, g_vk.alloc,
                    &g_vk.frame_data[i].render_fence);
            checkv(err, "[vk] failed to create fence (%d)\n", i);

            err = vkCreateSemaphore(g_vk.device, &semaphore_ci, g_vk.alloc,
                    &g_vk.frame_data[i].img_ready_semaphore);
            checkv(err, "[vk] failed to create swapchain semaphore(%d)\n", i);
        }

        for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
            err = vkCreateSemaphore(g_vk.device, &semaphore_ci, g_vk.alloc,
                    &g_vk.render_finished[i]);
            checkv(err, "[vk] failed to create render semaphore(%d)\n", i);
        }

        printf("[inf][vk] command objects created (%d)\n", g_vk.swapchain_image_count);
    }

    { /* pipelines */
        struct vertex_binding ui_bindings[] = {
            {
                .binding = 0,
                .stride = sizeof(struct ui_instance),
                .input_rate = VERTEX_INPUT_RATE_INSTANCE,
            },
        };
        struct vertex_attribute ui_attributes[] = {
            // binding 0 — per instance rect and color
            {
                .binding = 0,
                .location = 0,
                .format = VERTEX_FORMAT_FLOAT4,
                .offset = offsetof(struct ui_instance, x),
            },
            {
                .binding = 0,
                .location = 1,
                .format = VERTEX_FORMAT_FLOAT4,
                .offset = offsetof(struct ui_instance, r),
            },
        };

        const struct render_pipeline_config configs[BUILTIN_RENDER_PIPELINE_COUNT] = {
                [BUILTIN_RENDER_PIPELINE_UI] = {
                    .shader_fragment_size = sizeof(ui_frag_spv),
                    .shader_fragment = (uint32_t*)ui_frag_spv,
                    .shader_vertex_size = sizeof(ui_vert_spv),
                    .shader_vertex =(uint32_t*)ui_vert_spv,
                    .bindings = ui_bindings,
                    .binding_count = count_of(ui_bindings),
                    .attributes = ui_attributes,
                    .attribute_count = count_of(ui_attributes),
                    .cull_mode = CULL_MODE_NONE,
                    .blend_mode = BLEND_MODE_ALPHA,
                    .depth_test = 0,
                    .depth_write = 0,
                    .topology = TOPOLOGY_TRIANGLE_STRIP,
                    .push_constant_size = sizeof(float) * 2,
                },
            };

        for (uint32_t i = 0; i < BUILTIN_RENDER_PIPELINE_COUNT; i++) {
            err = vulkan_create_pipeline(&configs[i], &g_vk.pipelines[i]);
            checkv(err, "[err][vk] failed to create render pipeline(%d)\n", i);
            printf("[inf][vk] pipeline created!\n");
        }
    }

    {
        err = vulkan_buffer_create(&g_vk.ui_instance_buffer, NULL,
                sizeof(struct ui_instance) * MAX_UI_ELEMENTS,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(err, "[err][vk] failed to create render pipeline(%d)\n");
    }

    printf("[inf][vk] vulkan initialization successful\n");

    return err;
}

void render_device_draw_ui(struct ui_instance *instances, uint32_t count)
{
    if (count == 0) return;

    struct vulkan_frame_data *frame = &g_vk.frame_data[g_vk.current_frame];
    VkCommandBuffer cmd = frame->buffer;

    // upload instances
    memcpy(g_vk.ui_instance_buffer.mapped, instances, 
            sizeof(struct ui_instance) * count);

    // bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            g_vk.pipelines[BUILTIN_RENDER_PIPELINE_UI].pipeline);

    // set viewport and scissor
    VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)g_vk.swapchain_extent.width,
        .height   = (float)g_vk.swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = g_vk.swapchain_extent,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // push screen dimensions
    float push[] = {
        (float)g_vk.window_width,
        (float)g_vk.window_height,
    };
    vkCmdPushConstants(cmd, g_vk.pipelines[BUILTIN_RENDER_PIPELINE_UI].layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(push), push);

    // bind instance buffer
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_vk.ui_instance_buffer.handle, &offset);

    // draw — 4 vertices per instance, no index buffer
    vkCmdDraw(cmd, 4, count, 0, 0);
}

void render_device_terminate_vulkan(void)
{
    vkDeviceWaitIdle(g_vk.device);

    vulkan_buffer_destroy(&g_vk.ui_instance_buffer);

    for (uint32_t i = 0; i < BUILTIN_RENDER_PIPELINE_COUNT; i++) {
        vkDestroyPipelineLayout(g_vk.device, g_vk.pipelines[i].layout, g_vk.alloc);
        vkDestroyPipeline(g_vk.device, g_vk.pipelines[i].pipeline, g_vk.alloc);
    };

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        struct vulkan_frame_data f = g_vk.frame_data[i];
        vkDestroyFence(g_vk.device, f.render_fence, g_vk.alloc);
        vkDestroySemaphore(g_vk.device, f.img_ready_semaphore, g_vk.alloc);

        vkFreeCommandBuffers(g_vk.device, f.pool, 1, &f.buffer);
        vkDestroyCommandPool(g_vk.device, f.pool, g_vk.alloc);
    }

    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        vkDestroySemaphore(g_vk.device, g_vk.render_finished[i], g_vk.alloc);
    }

    vkDestroyCommandPool(g_vk.device, g_vk.upload_pool, g_vk.alloc);

    vulkan_destroy_swapchain();

    if (g_vk.device) vkDestroyDevice(g_vk.device, g_vk.alloc);

    if (g_vk.surface) vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, g_vk.alloc);
    if (g_vk.debug && g_vk.messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_fn;
        destroy_fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr( g_vk.instance, "vkDestroyDebugUtilsMessengerEXT");

        if (destroy_fn) {
            destroy_fn(g_vk.instance, g_vk.messenger, g_vk.alloc);
        }
    }

    if (g_vk.instance) vkDestroyInstance(g_vk.instance, g_vk.alloc);

    printf("[inf][vk] vulkan shutdown successful\n");
}

void render_device_on_resize(uint32_t width, uint32_t height)
{
    g_vk.window_width= width;
    g_vk.window_height = height;

    vkDeviceWaitIdle(g_vk.device);
    vulkan_destroy_swapchain();
    vulkan_create_swapchain(width, height);
}

void render_device_begin_drawing_vulkan(void)
{
    struct vulkan_frame_data *frame = &g_vk.frame_data[g_vk.current_frame];
    VkCommandBuffer buffer = frame->buffer;

    vkWaitForFences(g_vk.device, 1, &frame->render_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_vk.device, 1, &frame->render_fence);

    VkResult result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
            frame->img_ready_semaphore, VK_NULL_HANDLE, &g_vk.image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        render_device_on_resize(g_vk.window_width, g_vk.window_height);
        return;
    }

    struct vulkan_image *img = &g_vk.swapchain_images[g_vk.image_index];

    vkResetCommandBuffer(buffer, 0);
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(buffer, &begin);
    vulkan_transition_image(buffer, img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = img->view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { .float32 = {0.094f, 0.994f, 0.094f, 1.0f} } },
    };
    VkRenderingInfo rendering = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { .offset = {0,0}, .extent = g_vk.swapchain_extent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachment,
    };
    vkCmdBeginRendering(buffer, &rendering);

    /* temporary */
    float screen_w = (float)g_vk.window_width;
    float screen_h = (float)g_vk.window_height;
    struct ui_instance titlebar = {
        .x = 0.0f, .y = 0.0f,
        .w = screen_w, .h = 30.0f,
        .r = 1.0f, .g = 0.122f, .b = 0.122f, .a = 1.0f  // 0x1f1f1f
    };
    struct ui_instance elements[] = {
        { 
            .x = 0.0f,
            .y = titlebar.h,
            .w = 300.0f,
            .h = screen_h - titlebar.h,
            .r = 0.2f,
            .g = 0.5f,
            .b = 1.0f,
            .a = 1.0f,
        },
        {
            .x = 300.0f,
            .y = titlebar.h,
            .w = screen_w - 300.0f,
            .h = screen_h - titlebar.h,
            .r = 1.0f,
            .g = 0.3f,
            .b = 0.3f,
            .a = 0.8f,
        },
        titlebar,
    };
    render_device_draw_ui(elements, count_of(elements));
}

void render_device_end_drawing_vulkan(void)
{
    struct vulkan_frame_data *frame = &g_vk.frame_data[g_vk.current_frame];
    VkCommandBuffer buffer = frame->buffer;
    struct vulkan_image *img = &g_vk.swapchain_images[g_vk.image_index];

    vkCmdEndRendering(buffer);
    vulkan_transition_image(buffer, img, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(buffer);

    VkSemaphoreSubmitInfo wait_info = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame->img_ready_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSemaphoreSubmitInfo signal_info = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = g_vk.render_finished[g_vk.image_index],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    };
    VkCommandBufferSubmitInfo cmd_info = {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = buffer,
    };
    VkSubmitInfo2 submit = {
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &wait_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signal_info,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmd_info,
    };
    vkQueueSubmit2(g_vk.gfx_queue, 1, &submit, frame->render_fence);

    VkPresentInfoKHR present = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &g_vk.render_finished[g_vk.image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &g_vk.swapchain,
        .pImageIndices      = &g_vk.image_index,
    };
    vkQueuePresentKHR(g_vk.gfx_queue, &present);

    g_vk.current_frame = (g_vk.current_frame + 1) % FRAMES_IN_FLIGHT;

}

static int32_t vulkan_find_gfx_queue(VkPhysicalDevice d, int32_t* idx, VkSurfaceKHR s)
{
    *idx = -1;

    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qfc, NULL);

    VkQueueFamilyProperties qprops[qfc];
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qfc, qprops);

    for (uint32_t j = 0; j < qfc; j++) {
        if (qprops[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *idx = j;
        }
    }

    if (*idx < 0) return 0;

    VkBool32 present = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(d, *idx, s, &present);
    return present;
}

static VkPresentModeKHR vulkan_find_present_mod(VkPhysicalDevice d, VkSurfaceKHR s)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, s, &count, NULL);

    VkPresentModeKHR supported[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, s, &count, supported);

    VkPresentModeKHR requested = VK_PRESENT_MODE_MAILBOX_KHR;
    for (size_t i = 0; i < count; i++) {
        if (requested == supported[i]) {
            return supported[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice d, VkSurfaceKHR s)
{
    VkResult err = VK_SUCCESS;
    const VkSurfaceFormatKHR err_fmt = {
        .format = VK_FORMAT_UNDEFINED,
        .colorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR,
    };

    const VkFormat candidates[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
    };

    const VkColorSpaceKHR cspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    uint32_t fmtcount = 0;
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(d, s, &fmtcount, NULL);
    if (err) return err_fmt;

    VkSurfaceFormatKHR fmts[fmtcount];
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(d, s, &fmtcount, fmts);
    if (err) return err_fmt;

    for (uint32_t i = 0; i < fmtcount; i++) {
        for (uint32_t j = 0; j < count_of(candidates); j++) {
            if (candidates[j] == fmts[i].format && fmts[i].colorSpace == cspace)
                return fmts[i];
        }
    }
    return err_fmt;
}

static uint32_t vulkan_create_swapchain(uint32_t width, uint32_t height)
{
    VkResult err = VK_SUCCESS;
    VkSurfaceFormatKHR* surface_fmt = &g_vk.swapchain_surface_fmt;

    *surface_fmt = vulkan_find_surface_fmt(g_vk.physical_device, g_vk.surface);

    err = surface_fmt->format == VK_FORMAT_UNDEFINED;
    check(err, "[err][vk] failed to find surface format");

    err = surface_fmt->colorSpace == VK_COLOR_SPACE_MAX_ENUM_KHR;
    check(err, "[err][vk] failed to find surface format");

    VkPresentModeKHR pmode = vulkan_find_present_mod(
            g_vk.physical_device, g_vk.surface);

    VkSurfaceCapabilitiesKHR caps = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device,
            g_vk.surface, &caps);

    g_vk.swapchain_extent = caps.currentExtent;
    if (g_vk.swapchain_extent.width == UINT32_MAX) {
        g_vk.swapchain_extent.width  = clamp(width, caps.minImageExtent.width,
                caps.maxImageExtent.width);
        g_vk.swapchain_extent.height = clamp(height, caps.minImageExtent.height,
                caps.maxImageExtent.height);
    }
    printf("[inf][vk] swapchain extent: %d : %d\n", 
            g_vk.swapchain_extent.width, g_vk.swapchain_extent.height);

    g_vk.swapchain_image_count = 4;
    if (g_vk.swapchain_image_count < caps.minImageCount)
        g_vk.swapchain_image_count = caps.minImageCount;

    VkSurfaceTransformFlagBitsKHR pretransform =
        (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : caps.currentTransform;

    int img_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    img_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = g_vk.surface,
        .imageFormat = g_vk.swapchain_surface_fmt.format,
        .imageColorSpace = g_vk.swapchain_surface_fmt.colorSpace,
        .imageArrayLayers = 1,
        .imageUsage = img_usage,
        .imageExtent = g_vk.swapchain_extent,
        .preTransform = pretransform,
        .minImageCount = g_vk.swapchain_image_count,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = pmode,
        .clipped = VK_TRUE,
    };

    err = vkCreateSwapchainKHR(g_vk.device, &ci, g_vk.alloc, &g_vk.swapchain);
    check(err, "[err][vk] failed to retrieve swapchain images\n");

    err = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain,
            &g_vk.swapchain_image_count, NULL);

    check(err, "[err][vk] failed to retrieve swapchain images\n");
    VkImage sc_imgs[MAX_SWAPCHAIN_IMAGES] = { 0 };

    err = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain, 
            &g_vk.swapchain_image_count, sc_imgs);
    check(err, "[err][vk] failed to retrieve swapchain images\n");

    /* copy handles over */
    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        g_vk.swapchain_images[i].handle = sc_imgs[i];
        g_vk.swapchain_images[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
        g_vk.swapchain_images[i].aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    printf("[inf][vk] swapchain images obtained (%d)\n", g_vk.swapchain_image_count);

    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = g_vk.swapchain_images[i].handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = g_vk.swapchain_surface_fmt.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .baseArrayLayer = 0,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        err = vkCreateImageView(g_vk.device, &ci, g_vk.alloc,
                &g_vk.swapchain_images[i].view);
        checkv(err, "[err][vk] failed to create swapchain image view (%d)\n", i);
    }
    return err;
}

static void vulkan_destroy_swapchain(void)
{
    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        if (g_vk.swapchain_images[i].view) {
            vkDestroyImageView(g_vk.device, g_vk.swapchain_images[i].view, g_vk.alloc);
        }
    }
    vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, g_vk.alloc);
}

static void vulkan_transition_image(VkCommandBuffer buffer,
        struct vulkan_image* img, VkImageLayout new)
{
    if (img->layout == new) return;
    VkImageSubresourceRange sub_image = {
        .aspectMask = img->aspect_mask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT
            | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = img->layout,
        .newLayout = new,
        .subresourceRange = sub_image,
        .image = img->handle,
    };

    VkDependencyInfo  dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(buffer, &dep_info);
    img->layout = new;
}

static VkFormat vertex_format_to_vk(uint32_t fmt)
{
    switch (fmt) {
        case VERTEX_FORMAT_FLOAT:  return VK_FORMAT_R32_SFLOAT;
        case VERTEX_FORMAT_FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
        case VERTEX_FORMAT_FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VERTEX_FORMAT_FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return VK_FORMAT_UNDEFINED;
}

static VkCullModeFlags cull_mode_to_vk(uint32_t mode)
{
    switch (mode) {
        case CULL_MODE_NONE:  return VK_CULL_MODE_NONE;
        case CULL_MODE_FRONT: return VK_CULL_MODE_FRONT_BIT;
        case CULL_MODE_BACK:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

static VkPrimitiveTopology topology_to_vk(uint32_t t)
{
    switch (t) {
        case TOPOLOGY_TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case TOPOLOGY_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

static uint32_t vulkan_create_pipeline(const struct render_pipeline_config* cfg,
        struct vulkan_pipeline* pipe)
{
    VkResult err = VK_SUCCESS;

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = cfg->push_constant_size,
    };

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = cfg->push_constant_size > 0 ? 1 : 0,
        .pPushConstantRanges = cfg->push_constant_size > 0 ? &pc_range : NULL,
    };

    err = vkCreatePipelineLayout(g_vk.device, &layout_ci, g_vk.alloc, &pipe->layout);
    check(err, "[err][vk] failed to create pipeline layout\n");

    VkShaderModuleCreateInfo vert_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = cfg->shader_vertex_size,
            .pCode = cfg->shader_vertex,
        };
    VkShaderModuleCreateInfo frag_ci = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = cfg->shader_fragment_size,
            .pCode = cfg->shader_fragment,
    };
    VkShaderModule vert_module, frag_module;

    err = vkCreateShaderModule(g_vk.device, &vert_ci, g_vk.alloc, &vert_module);
    check(err, "[err][vk] failed to create vertex shader module\n");

    err = vkCreateShaderModule(g_vk.device, &frag_ci, g_vk.alloc, &frag_module);
    check(err, "[err][vk] failed to create fragment shader module\n");

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName  = "main",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName  = "main",
        },
    };

    VkVertexInputBindingDescription bindings[MAX_VERTEX_BINDINGS]  = { 0 };
    for (uint32_t i = 0; i < cfg->binding_count; i++) {
        bindings[i] =(VkVertexInputBindingDescription) {
            .binding = cfg->bindings[i].binding,
            .stride = cfg->bindings[i].stride,
            .inputRate = cfg->bindings[i].input_rate == VERTEX_INPUT_RATE_VERTEX ?
                VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE,
        };
    };

    VkVertexInputAttributeDescription attributes[MAX_VERTEX_ATTRIBUTES] = { 0 };
    for (uint32_t i = 0; i < cfg->attribute_count; i++) {
        attributes[i] = (VkVertexInputAttributeDescription) {
            .binding = cfg->attributes[i].binding,
            .location = cfg->attributes[i].location,
            .offset = cfg->attributes[i].offset,
            .format = vertex_format_to_vk(cfg->attributes[i].format),
        };
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = cfg->binding_count,
        .pVertexBindingDescriptions      = bindings,
        .vertexAttributeDescriptionCount = cfg->attribute_count,
        .pVertexAttributeDescriptions    = attributes,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topology_to_vk(cfg->topology),
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynamic_states,
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = cull_mode_to_vk(cfg->cull_mode),
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    if (cfg->blend_mode == BLEND_MODE_ALPHA) {
        blend_attachment.blendEnable         = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    } else if (cfg->blend_mode == BLEND_MODE_ADDITIVE) {
        blend_attachment.blendEnable         = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend_attachment,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = cfg->depth_test,
        .depthWriteEnable = cfg->depth_write,
        .depthCompareOp   = VK_COMPARE_OP_LESS,
    };

    VkPipelineRenderingCreateInfo rendering_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &g_vk.swapchain_surface_fmt.format,
    };

    VkGraphicsPipelineCreateInfo pipe_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_ci,
        .stageCount = count_of(stages),
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState   = &multisample,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic_state,
        .layout = pipe->layout,
    };

    err = vkCreateGraphicsPipelines(g_vk.device, NULL, 1, &pipe_ci,
            g_vk.alloc, &pipe->pipeline);

    vkDestroyShaderModule(g_vk.device, vert_module, g_vk.alloc);
    vkDestroyShaderModule(g_vk.device, frag_module, g_vk.alloc);

    check(err, "[err][vk] failed to create pipeline layout\n");
    return err;
}

/*                 buffers */

static int32_t vulkan_find_memory_type(uint32_t filter, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(g_vk.physical_device, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((filter & (1 << i)) &&
            (props.memoryTypes[i].propertyFlags & flags) == flags)
            return (int32_t)i;
    }
    return -1;
}

static uint32_t vulkan_buffer_alloc(struct vulkan_buffer *buf, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
{
    VkResult err;
    VkBufferCreateInfo ci = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    err = vkCreateBuffer(g_vk.device, &ci, g_vk.alloc, &buf->handle);
    check(err, "[err][vk] failed to create buffer\n");

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(g_vk.device, buf->handle, &mem_req);

    int32_t mem_type = vulkan_find_memory_type(mem_req.memoryTypeBits, flags);
    if (mem_type == -1) {
        printf("[err][vk] failed to find suitable memory type\n");
        return 1;
    }
    VkMemoryAllocateInfo alloc_ci = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = (uint32_t)mem_type,
    };
    err = vkAllocateMemory(g_vk.device, &alloc_ci, g_vk.alloc, &buf->memory);
    check(err, "[err][vk] failed to allocate buffer memory\n");

    err = vkBindBufferMemory(g_vk.device, buf->handle, buf->memory, 0);
    check(err, "[err][vk] failed to bind buffer memory\n");

    buf->size   = size;
    buf->mapped = NULL;
    return 0;
}

static void vulkan_buffer_copy(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo alloc_ci = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_vk.upload_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(g_vk.device, &alloc_ci, &cmd);

    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin);
    VkBufferCopy region = { .size = size };
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };
    vkQueueSubmit(g_vk.gfx_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.gfx_queue);
    vkFreeCommandBuffers(g_vk.device, g_vk.upload_pool, 1, &cmd);
}

uint32_t vulkan_buffer_create(struct vulkan_buffer *buf, const void *data,
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
{
    uint32_t err;

    /* host-visible — write directly, no staging needed */
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        err = vulkan_buffer_alloc(buf, size, usage, flags);
        if (err) return err;

        vkMapMemory(g_vk.device, buf->memory, 0, size, 0, &buf->mapped);
        if (data)
            memcpy(buf->mapped, data, size);

        printf("[inf][vk] host buffer created (%zu bytes)\n", (size_t)size);
        return 0;
    }

    /* device-local — stage through a temporary host-visible buffer */
    struct vulkan_buffer staging = {0};
    err = vulkan_buffer_alloc(&staging, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (err) return err;

    if (data) {
        void *mapped;
        vkMapMemory(g_vk.device, staging.memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(g_vk.device, staging.memory);
    }

    err = vulkan_buffer_alloc(buf, size,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (err) return err;

    vulkan_buffer_copy(staging.handle, buf->handle, size);
    vulkan_buffer_destroy(&staging);

    printf("[inf][vk] device buffer created (%zu bytes)\n", (size_t)size);
    return 0;
}

void vulkan_buffer_destroy(struct vulkan_buffer *buf)
{
    if (buf->mapped)
        vkUnmapMemory(g_vk.device, buf->memory);
    vkDestroyBuffer(g_vk.device, buf->handle, g_vk.alloc);
    vkFreeMemory(g_vk.device, buf->memory, g_vk.alloc);
    *buf = (struct vulkan_buffer){0};
}

static VkBool32
debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT types,
                     const VkDebugUtilsMessengerCallbackDataEXT* data,
                     void* user_data)
{
    (void)types;
    (void)data;
    (void)user_data;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        printf("ERROR: %s\n\n", data->pMessage);
    }

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printf("WARNING: %s\n\n", data->pMessage);
    }

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        printf("INFO: %s\n\n", data->pMessage);
    }
    return VK_FALSE;
}
