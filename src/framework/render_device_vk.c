#include <stdio.h>
#include <vulkan/vulkan.h>

#include "framework.h"

#define check(err, msg) do{if (err){puts((msg)); return err;}} while (0);
#define checkv(err, msg, ...) do{if (err){printf((msg), __VA_ARGS__); return err;}} while (0);
#define MAX_SWAPCHAIN_IMAGES 8

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

struct vulkan_image {
    VkImage            handle;
    VkImageView        view;
    VkDeviceMemory     device_memory;

    VkExtent2D         extent;
    VkFormat           fmt;
    VkImageLayout      layout;
    VkImageAspectFlags aspect_mask;
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
    struct vulkan_image      swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkDebugUtilsMessengerEXT messenger;
};

static struct vulkan_state g_vk;

static VkBool32
debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT types,
                     const VkDebugUtilsMessengerCallbackDataEXT* data,
                     void* user_data);

static int32_t  vulkan_get_family_queue(VkPhysicalDevice dev, int32_t* idx, VkSurfaceKHR surf);
static uint32_t vulkan_create_swapchain(uint32_t width, uint32_t height);
static void     vulkan_destroy_swapchain(void);

static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice dev, VkSurfaceKHR surf);
static VkPresentModeKHR   vulkan_find_present_mode(VkPhysicalDevice d, VkSurfaceKHR s);

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
                PFN_vkCreateWin32SurfaceKHR create_surface= (PFN_vkCreateWin32SurfaceKHR )
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
        uint32_t devcount = 0;
        err = vkEnumeratePhysicalDevices(g_vk.instance, &devcount, NULL);
        if (err) return err;

        VkPhysicalDevice devices[devcount];
        err = vkEnumeratePhysicalDevices(g_vk.instance, &devcount, devices);
        if (err) return err;

        VkPhysicalDevice discrete_device = 0;
        VkPhysicalDevice integrated_device = 0;

        int32_t gfx_family = 0;

        for (uint32_t i = 0; i < devcount; i++) {
            VkPhysicalDeviceProperties props = {0};
            vkGetPhysicalDeviceProperties(devices[i], &props);
            printf("[inf][vk] evaluating device '%s'\n", props.deviceName);

            int suitable = vulkan_get_family_queue(devices[i], &gfx_family, g_vk.surface);
            if  ((props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) && suitable)
                discrete_device = devices[i];

            if ((props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) && suitable)
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
                    gfx_family);
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
        vulkan_get_family_queue(g_vk.physical_device, &queue_idx, g_vk.surface);
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

        uint32_t device_extension_count = 0;
        const char* device_extensions[VULKAN_DEVICE_EXT_COUNT] = { 0 };
        device_extensions[device_extension_count++] =  VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo device_ci = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = count_of(queue_cis),
            .pQueueCreateInfos = queue_cis,
            .enabledExtensionCount = device_extension_count,
            .ppEnabledExtensionNames = device_extensions,
            .pNext = &features_13,
        };

        err = vkCreateDevice(g_vk.physical_device, &device_ci, g_vk.alloc,
                             &g_vk.device);
        check(err, "[inf][vk] failed to create logical device\n");

        vkGetDeviceQueue(g_vk.device, g_vk.gfx_family, 0, &g_vk.gfx_queue);
        printf("[inf][vk] queues obtained\n");
    }
    {
        vulkan_create_swapchain(window->width, window->height);
    }

    printf("[inf][vk] vulkan initialization successful\n");

    return err;
}

void render_device_terminate_vulkan(void)
{
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

static int32_t vulkan_get_family_queue(VkPhysicalDevice dev, int32_t* idx, VkSurfaceKHR surf)
{
    *idx = -1;

    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfc, NULL);

    VkQueueFamilyProperties qprops[qfc];
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qfc, qprops);

    for (uint32_t j = 0; j < qfc; j++) {
        if (qprops[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *idx = j;
        }
    }

    if (*idx < 0) return 0;

    VkBool32 present = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(dev, *idx, surf, &present);
    return present;
}

static VkPresentModeKHR vulkan_find_present_mode(VkPhysicalDevice d, VkSurfaceKHR s)
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

static VkSurfaceFormatKHR vulkan_find_surface_fmt(VkPhysicalDevice dev, VkSurfaceKHR surf)
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
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmtcount, NULL);
    if (err) return err_fmt;

    VkSurfaceFormatKHR fmts[fmtcount];
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmtcount, fmts);
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

    VkPresentModeKHR pmode = vulkan_find_present_mode(g_vk.physical_device, g_vk.surface);

    VkSurfaceCapabilitiesKHR caps = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.physical_device, g_vk.surface, &caps);

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
    VkImage sc_imgs[g_vk.swapchain_image_count];

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

        err = vkCreateImageView(g_vk.device, &ci, g_vk.alloc, &g_vk.swapchain_images[i].view);
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
