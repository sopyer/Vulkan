#define VULKAN_ENABLE_LUNARG_VALIDATION

#include <stdint.h>
#include <stdarg.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>


enum {
    MAX_DEVICE_COUNT = 8,
    MAX_QUEUE_COUNT = 4, //ATM there should be at most transfer, graphics, compute, graphics+compute families
    MAX_PRESENT_MODE_COUNT = 6, // At the moment in spec
    MAX_SWAPCHAIN_IMAGES = 3,
    FRAME_COUNT = 2,
    PRESENT_MODE_MAILBOX_IMAGE_COUNT = 3,
    PRESENT_MODE_DEFAULT_IMAGE_COUNT = 2,
};

static uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
{
    return value < min ? min : (value > max ? max : value);
}

//----------------------------------------------------------

const VkApplicationInfo appInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Vulkan SDL tutorial",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_0,
};

const VkInstanceCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledExtensionCount = 2,
    .ppEnabledExtensionNames = (const char* const[]) { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME },
};

#ifdef VULKAN_ENABLE_LUNARG_VALIDATION
const VkInstanceCreateInfo createInfoLunarGValidation = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = 1,
    .ppEnabledLayerNames = (const char* const[]) { "VK_LAYER_LUNARG_standard_validation" },
    .enabledExtensionCount = 3,
    .ppEnabledExtensionNames = (const char* const[]) { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME },
};

PFN_vkCreateDebugReportCallbackEXT vkpfn_CreateDebugReportCallbackEXT = 0;
PFN_vkDestroyDebugReportCallbackEXT vkpfn_DestroyDebugReportCallbackEXT = 0;

VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanReportFunc(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData)
{
    printf("VULKAN VALIDATION: %s\n", msg);
    return VK_FALSE;
}

VkDebugReportCallbackCreateInfoEXT debugCallbackCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
    .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
    .pfnCallback = VulkanReportFunc,
};
#endif

VkInstance instance = VK_NULL_HANDLE;

int init_vulkan()
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
#ifdef VULKAN_ENABLE_LUNARG_VALIDATION
    result = vkCreateInstance(&createInfoLunarGValidation, 0, &instance);
    if (result == VK_SUCCESS)
    {
        vkpfn_CreateDebugReportCallbackEXT =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        vkpfn_DestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if (vkpfn_CreateDebugReportCallbackEXT && vkpfn_DestroyDebugReportCallbackEXT)
        {
            vkpfn_CreateDebugReportCallbackEXT(instance, &debugCallbackCreateInfo, 0, &debugCallback);
        }
    }
#endif
    if (result != VK_SUCCESS)
    {
        result = vkCreateInstance(&createInfo, 0, &instance);
    }

    return result == VK_SUCCESS;
}

void fini_vulkan()
{
#ifdef VULKAN_ENABLE_LUNARG_VALIDATION
    if (vkpfn_DestroyDebugReportCallbackEXT && debugCallback)
    {
        vkpfn_DestroyDebugReportCallbackEXT(instance, debugCallback, 0);
    }
#endif
    vkDestroyInstance(instance, 0);
}

//----------------------------------------------------------

SDL_Window* window = 0;
VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
VkSurfaceKHR surface = VK_NULL_HANDLE;
uint32_t width = 1280;
uint32_t height = 720;

int init_window()
{
    SDL_Window* window = SDL_CreateWindow(
        "Vulkan Sample",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        0
    );

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    surfaceCreateInfo.hinstance = GetModuleHandle(0);
    surfaceCreateInfo.hwnd = info.info.win.window;

    VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface);

    return window != 0 && result == VK_SUCCESS;
}

void fini_window()
{
    vkDestroySurfaceKHR(instance, surface, 0);
    SDL_DestroyWindow(window);
}

//----------------------------------------------------------

VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
uint32_t queueFamilyIndex;
VkQueue queue;
VkDevice device = VK_NULL_HANDLE;

VkDeviceCreateInfo deviceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = (const char* const[]) { VK_KHR_SWAPCHAIN_EXTENSION_NAME },
};

int init_device()
{
    uint32_t physicalDeviceCount;
    VkPhysicalDevice deviceHandles[MAX_DEVICE_COUNT];
    VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_COUNT];

    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
    physicalDeviceCount = physicalDeviceCount > MAX_DEVICE_COUNT ? MAX_DEVICE_COUNT : physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

    for (uint32_t i = 0; i < physicalDeviceCount; ++i)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, NULL);
        queueFamilyCount = queueFamilyCount > MAX_QUEUE_COUNT ? MAX_QUEUE_COUNT : queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);

        for (uint32_t j = 0; j < queueFamilyCount; ++j) {

            VkBool32 supportsPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(deviceHandles[i], j, surface, &supportsPresent);

            if (supportsPresent && (queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                queueFamilyIndex = j;
                physicalDevice = deviceHandles[i];
                break;
            }
        }

        if (physicalDevice)
        {
            break;
        }
    }

    deviceCreateInfo.queueCreateInfoCount = 1,
    deviceCreateInfo.pQueueCreateInfos = (VkDeviceQueueCreateInfo[]) {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamilyIndex,
                .queueCount = 1,
                .pQueuePriorities = (const float[]) { 1.0f }
        }
    };
    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    return result == VK_SUCCESS;
}

void fini_device()
{
    vkDestroyDevice(device, 0);
}

//----------------------------------------------------------

VkSwapchainKHR swapchain;
uint32_t swapchainImageCount;
VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
VkExtent2D swapchainExtent;
VkSurfaceFormatKHR surfaceFormat;

int init_swapchain()
{
    //Use first available format
    uint32_t formatCount = 1;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0); // suppress validation layer
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, &surfaceFormat);
    surfaceFormat.format = surfaceFormat.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : surfaceFormat.format;

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);
    VkPresentModeKHR presentModes[MAX_PRESENT_MODE_COUNT];
    presentModeCount = presentModeCount > MAX_PRESENT_MODE_COUNT ? MAX_PRESENT_MODE_COUNT : presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;   // always supported.
    for (uint32_t i = 0; i < presentModeCount; ++i)
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    swapchainImageCount = presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? PRESENT_MODE_MAILBOX_IMAGE_COUNT : PRESENT_MODE_DEFAULT_IMAGE_COUNT;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    swapchainExtent = surfaceCapabilities.currentExtent;
    if (swapchainExtent.width == UINT32_MAX)
    {
        swapchainExtent.width = clamp_u32(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainExtent.height = clamp_u32(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swapchainImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1, // 2 for stereo
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };

    VkResult result = vkCreateSwapchainKHR(device, &swapChainCreateInfo, 0, &swapchain);
    if (result != VK_SUCCESS)
    {
        return 0;
    }

    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);

    return 1;
}

void fini_swapchain()
{
    vkDestroySwapchainKHR(device, swapchain, 0);
}

//----------------------------------------------------------

VkRenderPass renderPass;

int createRenderPass()
{
    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &(VkAttachmentDescription) {
            .format = surfaceFormat.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        .subpassCount = 1,
        .pSubpasses = &(VkSubpassDescription) {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &(VkAttachmentReference) {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        .dependencyCount = 1,
        .pDependencies = &(VkSubpassDependency) {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        },
    };

    vkCreateRenderPass(device, &renderPassCreateInfo, 0, &renderPass);

    return 1;
}

void destroyRenderPass()
{
    vkDestroyRenderPass(device, renderPass, NULL);
}

VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

int createFramebuffers()
{
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCreateImageView(device, &createInfo, 0, &swapchainImageViews[i]);

        VkFramebufferCreateInfo framebufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews[i],
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(device, &framebufferCreateInfo, 0, &framebuffers[i]);
    }

    return 1;
}

void destroyFramebuffers()
{
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }
}

uint32_t frameIndex = 0;
VkCommandPool commandPool;
VkCommandBuffer commandBuffers[FRAME_COUNT];
VkFence frameFences[FRAME_COUNT]; // Create with VK_FENCE_CREATE_SIGNALED_BIT.
VkSemaphore imageAvailableSemaphores[FRAME_COUNT];
VkSemaphore renderFinishedSemaphores[FRAME_COUNT];

int init_render()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool);

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = FRAME_COUNT,
    };

    vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers);

    VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    vkCreateSemaphore(device, &semaphoreCreateInfo, 0, &imageAvailableSemaphores[0]);
    vkCreateSemaphore(device, &semaphoreCreateInfo, 0, &imageAvailableSemaphores[1]);
    vkCreateSemaphore(device, &semaphoreCreateInfo, 0, &renderFinishedSemaphores[0]);
    vkCreateSemaphore(device, &semaphoreCreateInfo, 0, &renderFinishedSemaphores[1]);

    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    vkCreateFence(device, &fenceCreateInfo, 0, &frameFences[0]);
    vkCreateFence(device, &fenceCreateInfo, 0, &frameFences[1]);

    createRenderPass();
    createFramebuffers();

    return 1;
}

void fini_render()
{
    vkDeviceWaitIdle(device);
    destroyFramebuffers();
    destroyRenderPass();
    vkDestroyFence(device, frameFences[0], 0);
    vkDestroyFence(device, frameFences[1], 0);
    vkDestroySemaphore(device, renderFinishedSemaphores[0], 0);
    vkDestroySemaphore(device, renderFinishedSemaphores[1], 0);
    vkDestroySemaphore(device, imageAvailableSemaphores[0], 0);
    vkDestroySemaphore(device, imageAvailableSemaphores[1], 0);
    vkDestroyCommandPool(device, commandPool, 0);
}

void draw_frame()
{
    uint32_t index = (frameIndex++) % FRAME_COUNT;
    vkWaitForFences(device, 1, &frameFences[index], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &frameFences[index]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[index], VK_NULL_HANDLE, &imageIndex);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffers[index], &beginInfo);

    vkCmdBeginRenderPass(commandBuffers[index],
        &(VkRenderPassBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffers[imageIndex],
        .clearValueCount = 1,
        .pClearValues = &(VkClearValue) { 0.0f, 0.1f, 0.2f, 1.0f },
        .renderArea.offset = (VkOffset2D) { .x = 0,.y = 0 },
        .renderArea.extent = swapchainExtent,
        },
        VK_SUBPASS_CONTENTS_INLINE
    );
    vkCmdEndRenderPass(commandBuffers[index]);

    vkEndCommandBuffer(commandBuffers[index]);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores[index],
        .pWaitDstStageMask = (VkPipelineStageFlags[]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores[index],
    };
    vkQueueSubmit(queue, 1, &submitInfo, frameFences[index]);

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores[index],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    vkQueuePresentKHR(queue, &presentInfo);
}

//----------------------------------------------------------

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    int run = init_vulkan() && init_window() && init_device() && init_swapchain() && init_render();
    while (run)
    {
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT)
            {
                run = 0;
            }
        }

        draw_frame();
    }

    fini_render();
    fini_swapchain();
    fini_device();
    fini_window();
    fini_vulkan();

    SDL_Quit();

    return 0;
}
