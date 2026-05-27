#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <basic/basic.h>

#define check(err)  do { if (err) {printf("VK_ERR %d\n", err); return err; }}while (0)
#define checkv(err) do { if (err) { return err; } } while (0)

#define MAX_SWAPCHAIN_IMAGES 8
#define FRAMES_IN_FLIGHT     3
#define MAX_BUFFERS          256

/* Win32 surface — forward declared to keep windows.h out of this file */
typedef VkFlags VkWin32SurfaceCreateFlagsKHR;
typedef struct VkWin32SurfaceCreateInfoKHR {
    VkStructureType              sType;
    const void*                  pNext;
    VkWin32SurfaceCreateFlagsKHR flags;
    HINSTANCE                    hinstance;
    HWND                         hwnd;
} VkWin32SurfaceCreateInfoKHR;
typedef VkResult (VKAPI_PTR *PFN_vkCreateWin32SurfaceKHR)(
    VkInstance, const VkWin32SurfaceCreateInfoKHR*,
    const VkAllocationCallbacks*, VkSurfaceKHR*);

enum vulkan_instance_ext {
    VULKAN_INSTANCE_EXT_SURFACE = 0,
    VULKAN_INSTANCE_EXT_SURFACE_WIN32,
    VULKAN_INSTANCE_EXT_SURFACE_WL,
    VULKAN_INSTANCE_EXT_SURFACE_X11,
    VULKAN_INSTANCE_EXT_DEBUG_REPORT,
    VULKAN_INSTANCE_EXT_DEBUG_UTILS,
    VULKAN_INSTANCE_EXT_COUNT,
};

/* ------------------------------------------------------------------ */
/* types                                                               */
/* ------------------------------------------------------------------ */

struct vulkan_buffer {
    VkBuffer       handle;
    VkDeviceMemory memory;
    void*          mapped;
    VkDeviceSize   size;
};

struct vulkan_image {
    VkImage            handle;
    VkImageView        view;
    VkDeviceMemory     memory;
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
    float            clear_color[4];
    uint32_t         clear;
    uint32_t         depth_test;
    uint32_t         depth_write;
};

/* unified texture slot — plain textures leave depth zeroed,
 * render targets populate both image and depth */
struct render_texture_slot {
    struct vulkan_image image;
    struct vulkan_image depth;          /* zeroed for plain textures   */
    uint32_t            is_render_target;
    uint32_t            active;
};

struct render_buffer_slot {
    struct vulkan_buffer buffer;
    enum buffer_usage    usage;
    uint32_t             active;
};

/* render thread owns all fields */
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
    uint32_t                 swapchain_dirty;
    uint32_t                 current_frame;
    uint32_t                 image_index;

    VkCommandPool            upload_pool;
    struct vulkan_frame_data frame_data[FRAMES_IN_FLIGHT];
    struct vulkan_image      swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkSemaphore              render_finished[MAX_SWAPCHAIN_IMAGES];

    struct vulkan_pipeline   pipelines[RENDER_PIPELINES_COUNT];

    VkDescriptorPool         descriptor_pool;
    VkDescriptorSetLayout    descriptor_layout;
    VkDescriptorSet          descriptor_set;
    VkSampler                samplers[SAMPLER_COUNT];

    VkDebugUtilsMessengerEXT messenger;
    uint32_t                 current_pipeline;
    uint32_t                 window_width;
    uint32_t                 window_height;
};

static struct vulkan_state       g_vk;                       /* render thread */
static struct render_texture_slot g_textures[MAX_TEXTURES];  /* render thread */
static struct render_buffer_slot  g_buffers[MAX_BUFFERS];    /* render thread */

/* ------------------------------------------------------------------ */
/* forward declarations                                                */
/* ------------------------------------------------------------------ */

struct render_pipeline_config;

static VkBool32           debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                              VkDebugUtilsMessageTypeFlagsEXT,
                              const VkDebugUtilsMessengerCallbackDataEXT*, void*);
static uint32_t           vulkan_create_swapchain(uint32_t w, uint32_t h);
static void               vulkan_destroy_swapchain(void);
static void               vulkan_transition_image(VkCommandBuffer,
                              struct vulkan_image*, VkImageLayout);
static uint32_t           vulkan_create_pipeline(const struct render_pipeline_config*,
                              struct vulkan_pipeline*);
static int32_t            vulkan_find_memory_type(uint32_t filter, VkMemoryPropertyFlags);
static int32_t            vulkan_find_gfx_queue(VkPhysicalDevice, int32_t*, VkSurfaceKHR);
static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice, VkSurfaceKHR);
static VkPresentModeKHR   vulkan_find_present_mod(VkPhysicalDevice, VkSurfaceKHR);
static uint32_t           vulkan_image_alloc(struct vulkan_image*, uint32_t w, uint32_t h,
                              VkFormat, VkImageUsageFlags, VkImageAspectFlags);
static void               vulkan_image_free(struct vulkan_image*);
static uint32_t           vulkan_buffer_alloc(struct vulkan_buffer*, VkDeviceSize,
                              VkBufferUsageFlags, VkMemoryPropertyFlags);
static void               vulkan_buffer_free(struct vulkan_buffer*);
static void               vulkan_buffer_copy(VkBuffer src, VkBuffer dst, VkDeviceSize);
static uint32_t           vulkan_image_upload(struct vulkan_image*, const uint8_t*,
                              uint32_t w, uint32_t h);
static void               vulkan_submit_immediate(VkCommandBuffer);
static VkFormat           render_target_format_to_vk(enum render_target_format);
static VkFormat           resolve_pipeline_format(enum render_target_format);
static VkBufferUsageFlags buffer_usage_to_vk(enum buffer_usage);
static uint32_t           texture_write_descriptor(uint32_t slot, VkImageView view,
                              VkImageLayout layout, VkSampler sampler);

/* ------------------------------------------------------------------ */
/* init / terminate                                                    */
/* ------------------------------------------------------------------ */

uint32_t render_device_init_vulkan(const struct window *window)
{
    VkResult err = VK_SUCCESS;
    g_vk.debug   = 1;

    { /* instance */
        uint32_t    ext_count = 0;
        const char *exts[VULKAN_INSTANCE_EXT_COUNT] = {0};
        exts[ext_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
        switch (window->backend) {
            case WINDOW_BACKEND_WIN32:    exts[ext_count++] = "VK_KHR_win32_surface";   break;
            case WINDOW_BACKEND_WAYLAND:  exts[ext_count++] = "VK_KHR_wayland_surface"; break;
            case WINDOW_BACKEND_XCB:      exts[ext_count++] = "VK_KHR_xcb_surface";     break;
            default: return VK_ERROR_UNKNOWN;
        }
        if (g_vk.debug) {
            exts[ext_count++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            exts[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
        VkApplicationInfo app_info = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = "basic-v",
            .pEngineName        = "basic-v",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .engineVersion      = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion         = VK_MAKE_VERSION(1, 3, 0),
        };
        VkInstanceCreateInfo ci = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &app_info,
            .ppEnabledExtensionNames = exts,
            .enabledExtensionCount   = ext_count,
            .ppEnabledLayerNames     = layers,
            .enabledLayerCount       = g_vk.debug ? count_of(layers) : 0,
        };
        err = vkCreateInstance(&ci, g_vk.alloc, &g_vk.instance);
        check(err);
    }

    if (g_vk.debug) {
        PFN_vkCreateDebugUtilsMessengerEXT fn =
            (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk.instance, "vkCreateDebugUtilsMessengerEXT");
        if (fn) fn(g_vk.instance, &(VkDebugUtilsMessengerCreateInfoEXT){
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = debug_utils_callback,
        }, g_vk.alloc, &g_vk.messenger);
    }

    { /* surface */
        switch (window->backend) {
            case WINDOW_BACKEND_WIN32: {
                PFN_vkCreateWin32SurfaceKHR fn =
                    (PFN_vkCreateWin32SurfaceKHR)
                    vkGetInstanceProcAddr(g_vk.instance, "vkCreateWin32SurfaceKHR");
                if (!fn) return VK_ERROR_UNKNOWN;
                err = fn(g_vk.instance, &(VkWin32SurfaceCreateInfoKHR){
                    .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                    .hwnd      = window->win32.hwnd,
                    .hinstance = window->win32.hinstance,
                }, g_vk.alloc, &g_vk.surface);
                check(err);
                break;
            }
            default: return VK_ERROR_UNKNOWN;
        }
    }

    { /* physical device */
        uint32_t dev_count = 0;
        err = vkEnumeratePhysicalDevices(g_vk.instance, &dev_count, NULL);
        check(err);
        VkPhysicalDevice devices[dev_count];
        err = vkEnumeratePhysicalDevices(g_vk.instance, &dev_count, devices);
        check(err);

        VkPhysicalDevice discrete = VK_NULL_HANDLE, integrated = VK_NULL_HANDLE;
        int32_t gfx_idx = 0;
        for (uint32_t i = 0; i < dev_count; i++) {
            VkPhysicalDeviceProperties props = {0};
            vkGetPhysicalDeviceProperties(devices[i], &props);
            if (!vulkan_find_gfx_queue(devices[i], &gfx_idx, g_vk.surface)) continue;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   discrete   = devices[i];
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) integrated = devices[i];
        }
        g_vk.physical_device = discrete ? discrete : integrated ? integrated : VK_NULL_HANDLE;
        if (!g_vk.physical_device) return VK_ERROR_UNKNOWN;
        vulkan_find_gfx_queue(g_vk.physical_device, &gfx_idx, g_vk.surface);
        g_vk.gfx_family = (uint32_t)gfx_idx;
    }

    { /* logical device */
        float queue_priority = 1.0f;
        VkPhysicalDeviceFeatures2 features = {
            .sType                      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features.samplerAnisotropy = VK_TRUE,
        };
        VkPhysicalDeviceVulkan12Features features_12 = {
            .sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext                                        = &features,
            .bufferDeviceAddress                          = VK_TRUE,
            .descriptorIndexing                           = VK_TRUE,
            .runtimeDescriptorArray                       = VK_TRUE,
            .descriptorBindingPartiallyBound              = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing    = VK_TRUE,
        };
        VkPhysicalDeviceVulkan13Features features_13 = {
            .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext            = &features_12,
            .dynamicRendering = VK_TRUE,
            .synchronization2 = VK_TRUE,
        };
        const char *dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        err = vkCreateDevice(g_vk.physical_device, &(VkDeviceCreateInfo){
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &(VkDeviceQueueCreateInfo){
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = g_vk.gfx_family,
                .queueCount       = 1,
                .pQueuePriorities = &queue_priority,
            },
            .enabledExtensionCount   = count_of(dev_exts),
            .ppEnabledExtensionNames = dev_exts,
            .pNext                   = &features_13,
        }, g_vk.alloc, &g_vk.device);
        check(err);
        vkGetDeviceQueue(g_vk.device, g_vk.gfx_family, 0, &g_vk.gfx_queue);
    }

    {
        g_vk.window_width  = window->width;
        g_vk.window_height = window->height;
        err = vulkan_create_swapchain(window->width, window->height);
        check(err);
    }

    { /* command objects */
        VkCommandPoolCreateInfo pool_ci = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = g_vk.gfx_family,
        };
        err = vkCreateCommandPool(g_vk.device, &pool_ci, g_vk.alloc, &g_vk.upload_pool);
        check(err);

        VkFenceCreateInfo     fence_ci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,     .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        VkSemaphoreCreateInfo sem_ci   = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            err = vkCreateCommandPool(g_vk.device, &pool_ci, g_vk.alloc, &g_vk.frame_data[i].pool);
            checkv(err);
            err = vkAllocateCommandBuffers(g_vk.device, &(VkCommandBufferAllocateInfo){
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = g_vk.frame_data[i].pool,
                .commandBufferCount = 1,
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            }, &g_vk.frame_data[i].buffer);
            checkv(err);
            err = vkCreateFence(g_vk.device, &fence_ci, g_vk.alloc, &g_vk.frame_data[i].render_fence);
            checkv(err);
            err = vkCreateSemaphore(g_vk.device, &sem_ci, g_vk.alloc, &g_vk.frame_data[i].img_ready_semaphore);
            checkv(err);
        }
        for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
            err = vkCreateSemaphore(g_vk.device, &sem_ci, g_vk.alloc, &g_vk.render_finished[i]);
            checkv(err);
        }
    }

    { /* descriptors */
        err = vkCreateSampler(g_vk.device, &(VkSamplerCreateInfo){
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_NEAREST,
            .minFilter    = VK_FILTER_NEAREST,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .maxLod       = VK_LOD_CLAMP_NONE,
        }, g_vk.alloc, &g_vk.samplers[SAMPLER_NEAREST]);
        check(err);

        err = vkCreateSampler(g_vk.device, &(VkSamplerCreateInfo){
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = VK_FILTER_LINEAR,
            .minFilter    = VK_FILTER_LINEAR,
            .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .maxLod       = VK_LOD_CLAMP_NONE,
        }, g_vk.alloc, &g_vk.samplers[SAMPLER_LINEAR]);
        check(err);

        VkDescriptorPoolSize pool_size = {
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_TEXTURES,
        };
        err = vkCreateDescriptorPool(g_vk.device, &(VkDescriptorPoolCreateInfo){
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets       = 1,
            .poolSizeCount = 1,
            .pPoolSizes    = &pool_size,
        }, g_vk.alloc, &g_vk.descriptor_pool);
        check(err);

        VkDescriptorBindingFlags binding_flags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        VkDescriptorSetLayoutBinding binding = {
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_TEXTURES,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        err = vkCreateDescriptorSetLayout(g_vk.device, &(VkDescriptorSetLayoutCreateInfo){
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = &(VkDescriptorSetLayoutBindingFlagsCreateInfo){
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount  = 1,
                .pBindingFlags = &binding_flags,
            },
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = 1,
            .pBindings    = &binding,
        }, g_vk.alloc, &g_vk.descriptor_layout);
        check(err);

        err = vkAllocateDescriptorSets(g_vk.device, &(VkDescriptorSetAllocateInfo){
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = g_vk.descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &g_vk.descriptor_layout,
        }, &g_vk.descriptor_set);
        check(err);
    }

    return VK_SUCCESS;
}

void render_device_terminate_vulkan(void)
{
    vkDeviceWaitIdle(g_vk.device);

    for (uint32_t i = 0; i < MAX_TEXTURES; i++)
        if (g_textures[i].active) render_texture_destroy(i);

    for (uint32_t i = 0; i < MAX_BUFFERS; i++)
        if (g_buffers[i].active) render_buffer_destroy(i);

    vkDestroyDescriptorPool(g_vk.device, g_vk.descriptor_pool, g_vk.alloc);
    vkDestroyDescriptorSetLayout(g_vk.device, g_vk.descriptor_layout, g_vk.alloc);

    for (uint32_t i = 0; i < SAMPLER_COUNT; i++)
        vkDestroySampler(g_vk.device, g_vk.samplers[i], g_vk.alloc);

    for (uint32_t i = 0; i < RENDER_PIPELINES_COUNT; i++) {
        vkDestroyPipelineLayout(g_vk.device, g_vk.pipelines[i].layout, g_vk.alloc);
        vkDestroyPipeline(g_vk.device, g_vk.pipelines[i].pipeline, g_vk.alloc);
    }

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        struct vulkan_frame_data *f = &g_vk.frame_data[i];
        vkDestroyFence(g_vk.device, f->render_fence, g_vk.alloc);
        vkDestroySemaphore(g_vk.device, f->img_ready_semaphore, g_vk.alloc);
        vkFreeCommandBuffers(g_vk.device, f->pool, 1, &f->buffer);
        vkDestroyCommandPool(g_vk.device, f->pool, g_vk.alloc);
    }

    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++)
        vkDestroySemaphore(g_vk.device, g_vk.render_finished[i], g_vk.alloc);

    vkDestroyCommandPool(g_vk.device, g_vk.upload_pool, g_vk.alloc);
    vulkan_destroy_swapchain();

    if (g_vk.device)   vkDestroyDevice(g_vk.device, g_vk.alloc);
    if (g_vk.surface)  vkDestroySurfaceKHR(g_vk.instance, g_vk.surface, g_vk.alloc);

    if (g_vk.debug && g_vk.messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT fn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(g_vk.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(g_vk.instance, g_vk.messenger, g_vk.alloc);
    }

    if (g_vk.instance) vkDestroyInstance(g_vk.instance, g_vk.alloc);
}

/* ------------------------------------------------------------------ */
/* frame                                                               */
/* ------------------------------------------------------------------ */

void render_device_on_resize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(g_vk.device);
    vulkan_destroy_swapchain();
    vulkan_create_swapchain(width, height);
}

uint32_t render_begin_frame(void)
{
    if (g_vk.swapchain_dirty) {
        g_vk.swapchain_dirty = 0;
        render_device_on_resize(g_vk.window_width, g_vk.window_height);
    }

    struct vulkan_frame_data *frame = &g_vk.frame_data[g_vk.current_frame];
    vkWaitForFences(g_vk.device, 1, &frame->render_fence, VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(g_vk.device, g_vk.swapchain, UINT64_MAX,
            frame->img_ready_semaphore, VK_NULL_HANDLE, &g_vk.image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        render_device_on_resize(g_vk.window_width, g_vk.window_height);
        return 1;
    }
    if (result == VK_SUBOPTIMAL_KHR)
        g_vk.swapchain_dirty = 1;

    vkResetFences(g_vk.device, 1, &frame->render_fence);
    vkResetCommandBuffer(frame->buffer, 0);
    vkBeginCommandBuffer(frame->buffer, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    });
    return 0;
}

void render_end_frame(void)
{
    struct vulkan_frame_data *frame = &g_vk.frame_data[g_vk.current_frame];
    VkCommandBuffer cmd = frame->buffer;
    struct vulkan_image *dst = &g_vk.swapchain_images[g_vk.image_index];

    vulkan_transition_image(cmd, dst, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(cmd);

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
        .commandBuffer = cmd,
    };
    vkQueueSubmit2(g_vk.gfx_queue, 1, &(VkSubmitInfo2){
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &wait_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signal_info,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmd_info,
    }, frame->render_fence);

    VkResult result = vkQueuePresentKHR(g_vk.gfx_queue, &(VkPresentInfoKHR){
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &g_vk.render_finished[g_vk.image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &g_vk.swapchain,
        .pImageIndices      = &g_vk.image_index,
    });

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        g_vk.swapchain_dirty = 1;

    g_vk.current_frame = (g_vk.current_frame + 1) % FRAMES_IN_FLIGHT;
}

/* ------------------------------------------------------------------ */
/* pipeline                                                            */
/* ------------------------------------------------------------------ */

void render_begin_pipeline(enum render_pipelines id, uint32_t target_id)
{
    VkCommandBuffer        cmd  = g_vk.frame_data[g_vk.current_frame].buffer;
    struct vulkan_pipeline *pipe = &g_vk.pipelines[id];

    struct vulkan_image *target;
    struct vulkan_image *depth = NULL;

    if (target_id == RENDER_TARGET_SWAPCHAIN) {
        target = &g_vk.swapchain_images[g_vk.image_index];
    } else {
        assert(target_id < MAX_TEXTURES && g_textures[target_id].is_render_target);
        target = &g_textures[target_id].image;
        depth  = g_textures[target_id].depth.handle ? &g_textures[target_id].depth : NULL;
    }

    uint32_t use_depth = depth && (pipe->depth_test || pipe->depth_write);

    vulkan_transition_image(cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (use_depth)
        vulkan_transition_image(cmd, depth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color_attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = target->view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = pipe->clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = { .float32 = {
            pipe->clear_color[0], pipe->clear_color[1],
            pipe->clear_color[2], pipe->clear_color[3],
        }}},
    };

    VkRenderingAttachmentInfo depth_attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = use_depth ? depth->view : VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .depthStencil = { .depth = 1.0f } },
    };
    vkCmdBeginRendering(cmd, &(VkRenderingInfo){
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { .offset = {0,0}, .extent = target->extent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
        .pDepthAttachment     = use_depth ? &depth_attachment : NULL,
    });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipe->layout, 0, 1, &g_vk.descriptor_set, 0, NULL);
    vkCmdSetViewport(cmd, 0, 1, &(VkViewport){
        .width = (float)target->extent.width, .height = (float)target->extent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f,
    });
    vkCmdSetScissor(cmd, 0, 1, &(VkRect2D){ .extent = target->extent });

    g_vk.current_pipeline = id;
}

void render_end_pipeline(void)
{
    vkCmdEndRendering(g_vk.frame_data[g_vk.current_frame].buffer);
}

void render_draw(uint32_t buffer_id, const void *push, uint32_t push_size,
                 uint32_t vertex_count, uint32_t instance_count)
{
    if (buffer_id >= MAX_BUFFERS || !g_buffers[buffer_id].active) return;

    VkCommandBuffer        cmd  = g_vk.frame_data[g_vk.current_frame].buffer;
    struct vulkan_pipeline *pipe = &g_vk.pipelines[g_vk.current_pipeline];
    VkDeviceSize offset = 0;

    if (push && push_size > 0)
        vkCmdPushConstants(cmd, pipe->layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, push_size, push);

    vkCmdBindVertexBuffers(cmd, 0, 1, &g_buffers[buffer_id].buffer.handle, &offset);
    vkCmdDraw(cmd, vertex_count, instance_count, 0, 0);
}

void render_pipeline_register(enum render_pipelines id,
        const struct render_pipeline_config *cfg)
{
    if (id >= RENDER_PIPELINES_COUNT) return;
    vulkan_create_pipeline(cfg, &g_vk.pipelines[id]);
}

/* ------------------------------------------------------------------ */
/* render targets — unified with texture table                         */
/* ------------------------------------------------------------------ */

/* render_target_create allocates a color image + depth image,
 * transitions color to SHADER_READ_ONLY_OPTIMAL, writes the descriptor,
 * and returns a texture index usable directly in ui_instance.tex_index
 * and as target_id in render_begin_pipeline. */
uint32_t render_target_create(uint32_t width, uint32_t height,
        enum render_target_format format)
{
    for (uint32_t i = 0; i < MAX_TEXTURES; i++) {
        if (g_textures[i].active) continue;

        struct render_texture_slot *slot = &g_textures[i];

        uint32_t err = vulkan_image_alloc(&slot->image, width, height,
                render_target_format_to_vk(format),
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT           |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT      |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT);
        if (err) return INVALID_TEXTURE;

        err = vulkan_image_alloc(&slot->depth, width, height,
                VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT);
        if (err) {
            vulkan_image_free(&slot->image);
            return INVALID_TEXTURE;
        }

        /* prime the color image into SHADER_READ_ONLY_OPTIMAL so the
         * descriptor write is valid and the first frame transition is clean */
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(g_vk.device, &(VkCommandBufferAllocateInfo){
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = g_vk.upload_pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        }, &cmd);
        vkBeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        });
        vulkan_transition_image(cmd, &slot->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vulkan_submit_immediate(cmd);

        /* write into bindless descriptor set */
        texture_write_descriptor(i, slot->image.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                g_vk.samplers[SAMPLER_LINEAR]);

        slot->is_render_target = 1;
        slot->active           = 1;
        return i;
    }

    assert(0 && "texture slots exhausted — increase MAX_TEXTURES");
    return INVALID_TEXTURE;
}

void render_target_destroy(uint32_t id)
{
    if (id >= MAX_TEXTURES || !g_textures[id].active) return;
    vkDeviceWaitIdle(g_vk.device);
    vulkan_image_free(&g_textures[id].image);
    vulkan_image_free(&g_textures[id].depth);
    g_textures[id] = (struct render_texture_slot){0};
}

void render_target_prepare_sample(uint32_t id)
{
    VkCommandBuffer cmd = g_vk.frame_data[g_vk.current_frame].buffer;
    vulkan_transition_image(cmd, &g_textures[id].image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture_write_descriptor(id, g_textures[id].image.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        g_vk.samplers[SAMPLER_LINEAR]);
}

/* ------------------------------------------------------------------ */
/* textures                                                            */
/* ------------------------------------------------------------------ */

uint32_t render_texture_upload(const uint8_t *rgba, uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < MAX_TEXTURES; i++) {
        if (g_textures[i].active) continue;

        uint32_t err = vulkan_image_upload(&g_textures[i].image, rgba, width, height);
        if (err) return INVALID_TEXTURE;

        texture_write_descriptor(i, g_textures[i].image.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                g_vk.samplers[SAMPLER_NEAREST]);

        g_textures[i].active = 1;
        return i;
    }

    assert(0 && "texture slots exhausted — increase MAX_TEXTURES");
    return INVALID_TEXTURE;
}

void render_texture_destroy(uint32_t id)
{
    if (id >= MAX_TEXTURES || !g_textures[id].active) return;
    vkDeviceWaitIdle(g_vk.device);
    vulkan_image_free(&g_textures[id].image);
    g_textures[id] = (struct render_texture_slot){0};
}

/* ------------------------------------------------------------------ */
/* buffers                                                             */
/* ------------------------------------------------------------------ */

uint32_t render_buffer_create(const void *data, uint32_t size, enum buffer_usage usage)
{
    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        if (g_buffers[i].active) continue;

        struct render_buffer_slot *slot = &g_buffers[i];
        VkBufferUsageFlags vk_usage = buffer_usage_to_vk(usage);

        if (usage == BUFFER_USAGE_VERTEX && !data) {
            /* host-visible — updated frequently */
            if (vulkan_buffer_alloc(&slot->buffer, size, vk_usage,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) return INVALID_BUFFER;
            vkMapMemory(g_vk.device, slot->buffer.memory, 0, size, 0, &slot->buffer.mapped);
            slot->usage = usage; slot->active = 1;
            return i;
        }

        /* device-local — stage through temp host buffer */
        struct vulkan_buffer staging = {0};
        if (vulkan_buffer_alloc(&staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) return INVALID_BUFFER;

        if (data) {
            void *mapped;
            vkMapMemory(g_vk.device, staging.memory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(g_vk.device, staging.memory);
        }

        if (vulkan_buffer_alloc(&slot->buffer, size, vk_usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            vulkan_buffer_free(&staging);
            return INVALID_BUFFER;
        }

        if (data) vulkan_buffer_copy(staging.handle, slot->buffer.handle, size);
        vulkan_buffer_free(&staging);

        slot->usage = usage; slot->active = 1;
        return i;
    }
    return INVALID_BUFFER;
}

void render_buffer_update(uint32_t id, const void *data, uint32_t size)
{
    if (id >= MAX_BUFFERS || !g_buffers[id].active || !g_buffers[id].buffer.mapped) return;
    memcpy(g_buffers[id].buffer.mapped, data, size);
}

void render_buffer_destroy(uint32_t id)
{
    if (id >= MAX_BUFFERS || !g_buffers[id].active) return;
    vkDeviceWaitIdle(g_vk.device);
    vulkan_buffer_free(&g_buffers[id].buffer);
    g_buffers[id] = (struct render_buffer_slot){0};
}

/* ------------------------------------------------------------------ */
/* composite                                                           */
/* ------------------------------------------------------------------ */

void render_composite(uint32_t source_id)
{
    if (source_id == RENDER_TARGET_SWAPCHAIN) return;
    if (source_id >= MAX_TEXTURES || !g_textures[source_id].is_render_target) return;

    VkCommandBuffer      cmd = g_vk.frame_data[g_vk.current_frame].buffer;
    struct vulkan_image *src = &g_textures[source_id].image;
    struct vulkan_image *dst = &g_vk.swapchain_images[g_vk.image_index];

    vulkan_transition_image(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vulkan_transition_image(cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdBlitImage2(cmd, &(VkBlitImageInfo2){
        .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .srcImage       = src->handle,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage       = dst->handle,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount    = 1,
        .pRegions       = &(VkImageBlit2){
            .sType          = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
            .srcOffsets[1]  = { (int32_t)src->extent.width, (int32_t)src->extent.height, 1 },
            .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
            .dstOffsets[1]  = { (int32_t)dst->extent.width, (int32_t)dst->extent.height, 1 },
        },
        .filter = VK_FILTER_LINEAR,
    });

    vulkan_transition_image(cmd, dst, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

/* ------------------------------------------------------------------ */
/* static helpers                                                      */
/* ------------------------------------------------------------------ */

static uint32_t texture_write_descriptor(uint32_t slot, VkImageView view,
        VkImageLayout layout, VkSampler sampler)
{
    VkDescriptorImageInfo img_info = {
        .sampler     = sampler,
        .imageView   = view,
        .imageLayout = layout,
    };
    VkWriteDescriptorSet write = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = g_vk.descriptor_set,
        .dstBinding      = 0,
        .dstArrayElement = slot,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &img_info,
    };
    vkUpdateDescriptorSets(g_vk.device, 1, &write, 0, NULL);
    return VK_SUCCESS;
}

static void vulkan_submit_immediate(VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(g_vk.gfx_queue, 1, &(VkSubmitInfo){
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    }, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_vk.gfx_queue);
    vkFreeCommandBuffers(g_vk.device, g_vk.upload_pool, 1, &cmd);
}

static uint32_t vulkan_image_upload(struct vulkan_image *img,
        const uint8_t *rgba, uint32_t width, uint32_t height)
{
    VkResult err = vulkan_image_alloc(img, width, height,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    if (err) return err;

    VkDeviceSize size = (VkDeviceSize)width * height * 4;
    struct vulkan_buffer staging = {0};
    err = vulkan_buffer_alloc(&staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (err) return err;

    void *mapped;
    vkMapMemory(g_vk.device, staging.memory, 0, size, 0, &mapped);
    memcpy(mapped, rgba, size);
    vkUnmapMemory(g_vk.device, staging.memory);

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(g_vk.device, &(VkCommandBufferAllocateInfo){
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_vk.upload_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    }, &cmd);
    vkBeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    });

    vulkan_transition_image(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdCopyBufferToImage(cmd, staging.handle, img->handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy){
                .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
                .imageExtent      = { width, height, 1 },
            });
    vulkan_transition_image(cmd, img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vulkan_submit_immediate(cmd);
    vulkan_buffer_free(&staging);
    return VK_SUCCESS;
}

static uint32_t vulkan_image_alloc(struct vulkan_image *img,
        uint32_t width, uint32_t height,
        VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
    VkResult err;
    err = vkCreateImage(g_vk.device, &(VkImageCreateInfo){
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = fmt,
        .extent        = { width, height, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    }, g_vk.alloc, &img->handle);
    check(err);

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(g_vk.device, img->handle, &req);
    int32_t mem_type = vulkan_find_memory_type(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type < 0) return VK_ERROR_UNKNOWN;

    err = vkAllocateMemory(g_vk.device, &(VkMemoryAllocateInfo){
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = (uint32_t)mem_type,
    }, g_vk.alloc, &img->memory);
    check(err);

    err = vkBindImageMemory(g_vk.device, img->handle, img->memory, 0);
    check(err);

    err = vkCreateImageView(g_vk.device, &(VkImageViewCreateInfo){
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = img->handle,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = fmt,
        .components       = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = { aspect, 0, 1, 0, 1 },
    }, g_vk.alloc, &img->view);
    check(err);

    img->extent      = (VkExtent2D){ width, height };
    img->fmt         = fmt;
    img->layout      = VK_IMAGE_LAYOUT_UNDEFINED;
    img->aspect_mask = aspect;
    return VK_SUCCESS;
}

static void vulkan_image_free(struct vulkan_image *img)
{
    if (img->view)   vkDestroyImageView(g_vk.device, img->view,   g_vk.alloc);
    if (img->handle) vkDestroyImage    (g_vk.device, img->handle, g_vk.alloc);
    if (img->memory) vkFreeMemory      (g_vk.device, img->memory, g_vk.alloc);
    *img = (struct vulkan_image){0};
}

static void vulkan_transition_image(VkCommandBuffer cmd,
        struct vulkan_image *img, VkImageLayout new_layout)
{
    if (img->layout == new_layout) return;
    vkCmdPipelineBarrier2(cmd, &(VkDependencyInfo){
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &(VkImageMemoryBarrier2){
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout        = img->layout,
            .newLayout        = new_layout,
            .image            = img->handle,
            .subresourceRange = {
                img->aspect_mask, 0,
                VK_REMAINING_MIP_LEVELS, 0,
                VK_REMAINING_ARRAY_LAYERS,
            },
        },
    });
    img->layout = new_layout;
}

static uint32_t vulkan_buffer_alloc(struct vulkan_buffer *buf,
        VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
{
    VkResult err;
    err = vkCreateBuffer(g_vk.device, &(VkBufferCreateInfo){
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    }, g_vk.alloc, &buf->handle);
    check(err);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_vk.device, buf->handle, &req);
    int32_t mem_type = vulkan_find_memory_type(req.memoryTypeBits, flags);
    if (mem_type < 0) return VK_ERROR_UNKNOWN;

    err = vkAllocateMemory(g_vk.device, &(VkMemoryAllocateInfo){
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = (uint32_t)mem_type,
    }, g_vk.alloc, &buf->memory);
    check(err);

    err = vkBindBufferMemory(g_vk.device, buf->handle, buf->memory, 0);
    check(err);

    buf->size = size; buf->mapped = NULL;
    return VK_SUCCESS;
}

static void vulkan_buffer_copy(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(g_vk.device, &(VkCommandBufferAllocateInfo){
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_vk.upload_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    }, &cmd);
    vkBeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    });
    vkCmdCopyBuffer(cmd, src, dst, 1, &(VkBufferCopy){ .size = size });
    vulkan_submit_immediate(cmd);
}

static void vulkan_buffer_free(struct vulkan_buffer *buf)
{
    if (buf->mapped) vkUnmapMemory(g_vk.device, buf->memory);
    if (buf->handle) vkDestroyBuffer(g_vk.device, buf->handle, g_vk.alloc);
    if (buf->memory) vkFreeMemory(g_vk.device, buf->memory, g_vk.alloc);
    *buf = (struct vulkan_buffer){0};
}

static int32_t vulkan_find_gfx_queue(VkPhysicalDevice d, int32_t *idx, VkSurfaceKHR s)
{
    *idx = -1;
    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qfc, NULL);
    VkQueueFamilyProperties props[qfc];
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qfc, props);
    for (uint32_t i = 0; i < qfc; i++)
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) *idx = (int32_t)i;
    if (*idx < 0) return 0;
    VkBool32 present = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(d, (uint32_t)*idx, s, &present);
    return (int32_t)present;
}

static VkPresentModeKHR vulkan_find_present_mod(VkPhysicalDevice d, VkSurfaceKHR s)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, s, &count, NULL);
    VkPresentModeKHR modes[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, s, &count, modes);
    for (uint32_t i = 0; i < count; i++)
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) return VK_PRESENT_MODE_MAILBOX_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice d, VkSurfaceKHR s)
{
    const VkSurfaceFormatKHR err_fmt = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_MAX_ENUM_KHR };
    const VkFormat candidates[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,   VK_FORMAT_R8G8B8_UNORM,
    };
    uint32_t count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(d, s, &count, NULL)) return err_fmt;
    VkSurfaceFormatKHR fmts[count];
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(d, s, &count, fmts)) return err_fmt;
    for (uint32_t i = 0; i < count; i++)
        for (uint32_t j = 0; j < count_of(candidates); j++)
            if (fmts[i].format == candidates[j] &&
                fmts[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
                return fmts[i];
    return err_fmt;
}

static uint32_t vulkan_create_swapchain(uint32_t width, uint32_t height)
{
    VkResult err;
    g_vk.swapchain_surface_fmt = vulkan_find_surface_fmt(g_vk.physical_device, g_vk.surface);
    if (g_vk.swapchain_surface_fmt.format == VK_FORMAT_UNDEFINED) return VK_ERROR_UNKNOWN;

    VkSurfaceCapabilitiesKHR caps = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device, g_vk.surface, &caps);

    g_vk.swapchain_extent = caps.currentExtent;
    if (g_vk.swapchain_extent.width == UINT32_MAX) {
        g_vk.swapchain_extent.width  = clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        g_vk.swapchain_extent.height = clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    g_vk.swapchain_image_count = 4;
    if (g_vk.swapchain_image_count < caps.minImageCount)
        g_vk.swapchain_image_count = caps.minImageCount;

    VkSurfaceTransformFlagBitsKHR pretransform =
        (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;

    err = vkCreateSwapchainKHR(g_vk.device, &(VkSwapchainCreateInfoKHR){
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = g_vk.surface,
        .imageFormat      = g_vk.swapchain_surface_fmt.format,
        .imageColorSpace  = g_vk.swapchain_surface_fmt.colorSpace,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageExtent      = g_vk.swapchain_extent,
        .preTransform     = pretransform,
        .minImageCount    = g_vk.swapchain_image_count,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = vulkan_find_present_mod(g_vk.physical_device, g_vk.surface),
        .clipped          = VK_TRUE,
    }, g_vk.alloc, &g_vk.swapchain);
    check(err);

    err = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain,
            &g_vk.swapchain_image_count, NULL);
    check(err);
    VkImage sc_imgs[MAX_SWAPCHAIN_IMAGES] = {0};
    err = vkGetSwapchainImagesKHR(g_vk.device, g_vk.swapchain,
            &g_vk.swapchain_image_count, sc_imgs);
    check(err);

    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++) {
        g_vk.swapchain_images[i].handle      = sc_imgs[i];
        g_vk.swapchain_images[i].layout      = VK_IMAGE_LAYOUT_UNDEFINED;
        g_vk.swapchain_images[i].aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        g_vk.swapchain_images[i].fmt         = g_vk.swapchain_surface_fmt.format;
        g_vk.swapchain_images[i].extent      = g_vk.swapchain_extent;
        err = vkCreateImageView(g_vk.device, &(VkImageViewCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = sc_imgs[i],
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = g_vk.swapchain_surface_fmt.format,
            .components       = {
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        }, g_vk.alloc, &g_vk.swapchain_images[i].view);
        checkv(err);
    }

    g_vk.window_width    = g_vk.swapchain_extent.width;
    g_vk.window_height   = g_vk.swapchain_extent.height;
    g_vk.swapchain_dirty = 0;
    return VK_SUCCESS;
}

static void vulkan_destroy_swapchain(void)
{
    for (uint32_t i = 0; i < g_vk.swapchain_image_count; i++)
        if (g_vk.swapchain_images[i].view)
            vkDestroyImageView(g_vk.device, g_vk.swapchain_images[i].view, g_vk.alloc);
    vkDestroySwapchainKHR(g_vk.device, g_vk.swapchain, g_vk.alloc);
}

static VkFormat render_target_format_to_vk(enum render_target_format fmt)
{
    switch (fmt) {
        case RENDER_TARGET_FORMAT_RGBA8:   return VK_FORMAT_R8G8B8A8_UNORM;
        case RENDER_TARGET_FORMAT_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        default: assert(0 && "invalid render target format"); return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat resolve_pipeline_format(enum render_target_format fmt)
{
    if (fmt == RENDER_TARGET_FORMAT_SWAPCHAIN) return g_vk.swapchain_surface_fmt.format;
    return render_target_format_to_vk(fmt);
}

static VkBufferUsageFlags buffer_usage_to_vk(enum buffer_usage usage)
{
    switch (usage) {
        case BUFFER_USAGE_VERTEX:  return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BUFFER_USAGE_INDEX:   return VK_BUFFER_USAGE_INDEX_BUFFER_BIT   | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BUFFER_USAGE_STORAGE: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
}

static int32_t vulkan_find_memory_type(uint32_t filter, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(g_vk.physical_device, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++)
        if ((filter & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return (int32_t)i;
    return -1;
}

static VkFormat vertex_format_to_vk(uint32_t fmt)
{
    switch (fmt) {
        case VERTEX_FORMAT_FLOAT:  return VK_FORMAT_R32_SFLOAT;
        case VERTEX_FORMAT_FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
        case VERTEX_FORMAT_FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VERTEX_FORMAT_FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VERTEX_FORMAT_UINT:   return VK_FORMAT_R32_UINT;
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

static uint32_t vulkan_create_pipeline(const struct render_pipeline_config *cfg,
        struct vulkan_pipeline *pipe)
{
    VkResult err;
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size       = cfg->push_constant_size,
    };
    err = vkCreatePipelineLayout(g_vk.device, &(VkPipelineLayoutCreateInfo){
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &g_vk.descriptor_layout,
        .pushConstantRangeCount = cfg->push_constant_size > 0 ? 1 : 0,
        .pPushConstantRanges    = cfg->push_constant_size > 0 ? &pc_range : NULL,
    }, g_vk.alloc, &pipe->layout);
    check(err);

    VkShaderModule vert_module, frag_module;
    err = vkCreateShaderModule(g_vk.device, &(VkShaderModuleCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = cfg->shader_vertex_size, .pCode = cfg->shader_vertex,
    }, g_vk.alloc, &vert_module);
    check(err);
    err = vkCreateShaderModule(g_vk.device, &(VkShaderModuleCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = cfg->shader_fragment_size, .pCode = cfg->shader_fragment,
    }, g_vk.alloc, &frag_module);
    check(err);

    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_module, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module, .pName = "main" },
    };

    VkVertexInputBindingDescription   bindings[MAX_VERTEX_BINDINGS]    = {0};
    VkVertexInputAttributeDescription attributes[MAX_VERTEX_ATTRIBUTES] = {0};
    for (uint32_t i = 0; i < cfg->binding_count; i++)
        bindings[i] = (VkVertexInputBindingDescription){
            cfg->bindings[i].binding, cfg->bindings[i].stride,
            cfg->bindings[i].input_rate == VERTEX_INPUT_RATE_VERTEX
                ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE,
        };
    for (uint32_t i = 0; i < cfg->attribute_count; i++)
        attributes[i] = (VkVertexInputAttributeDescription){
            cfg->attributes[i].location, cfg->attributes[i].binding,
            vertex_format_to_vk(cfg->attributes[i].format), cfg->attributes[i].offset,
        };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    switch (cfg->blend_mode) {
        case BLEND_MODE_ALPHA:
            blend_attachment.blendEnable         = VK_TRUE;
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
            break;
        case BLEND_MODE_ADDITIVE:
            blend_attachment.blendEnable         = VK_TRUE;
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            break;
        case BLEND_MODE_NONE: break;
    }

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkFormat target_fmt = resolve_pipeline_format(cfg->target_format);

    err = vkCreateGraphicsPipelines(g_vk.device, NULL, 1, &(VkGraphicsPipelineCreateInfo){
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &(VkPipelineRenderingCreateInfo){
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &target_fmt,
            .depthAttachmentFormat   = (cfg->depth_test || cfg->depth_write)
                                       ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED,
        },
        .stageCount          = count_of(stages),
        .pStages             = stages,
        .pVertexInputState   = &(VkPipelineVertexInputStateCreateInfo){
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = cfg->binding_count,
            .pVertexBindingDescriptions      = bindings,
            .vertexAttributeDescriptionCount = cfg->attribute_count,
            .pVertexAttributeDescriptions    = attributes,
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo){
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = topology_to_vk(cfg->topology),
        },
        .pViewportState      = &(VkPipelineViewportStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1, .scissorCount = 1,
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo){
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode    = cull_mode_to_vk(cfg->cull_mode),
            .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth   = 1.0f,
        },
        .pMultisampleState   = &(VkPipelineMultisampleStateCreateInfo){
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        },
        .pDepthStencilState  = &(VkPipelineDepthStencilStateCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = cfg->depth_test,
            .depthWriteEnable = cfg->depth_write,
            .depthCompareOp   = VK_COMPARE_OP_LESS,
        },
        .pColorBlendState    = &(VkPipelineColorBlendStateCreateInfo){
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &blend_attachment,
        },
        .pDynamicState       = &(VkPipelineDynamicStateCreateInfo){
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = count_of(dynamic_states),
            .pDynamicStates    = dynamic_states,
        },
        .layout = pipe->layout,
    }, g_vk.alloc, &pipe->pipeline);

    vkDestroyShaderModule(g_vk.device, vert_module, g_vk.alloc);
    vkDestroyShaderModule(g_vk.device, frag_module, g_vk.alloc);
    check(err);

    pipe->depth_test     = cfg->depth_test;
    pipe->depth_write    = cfg->depth_write;
    pipe->clear          = cfg->clear;
    pipe->clear_color[0] = cfg->clear_color[0];
    pipe->clear_color[1] = cfg->clear_color[1];
    pipe->clear_color[2] = cfg->clear_color[2];
    pipe->clear_color[3] = cfg->clear_color[3];
    return VK_SUCCESS;
}

static VkBool32 debug_utils_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT  severity,
        VkDebugUtilsMessageTypeFlagsEXT         types,
        const VkDebugUtilsMessengerCallbackDataEXT *data,
        void *user_data)
{
    (void)types; (void)user_data;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        printf("[vk] %s", data->pMessage);
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        printf("[vk] %s", data->pMessage);
    return VK_FALSE;
}
