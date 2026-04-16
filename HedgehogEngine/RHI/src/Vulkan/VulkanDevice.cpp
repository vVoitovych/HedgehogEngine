// VulkanDevice.cpp
// This file is the ONLY translation unit that defines VOLK_IMPLEMENTATION
// and VMA_IMPLEMENTATION.  Every other Vulkan backend .cpp just includes
// the headers without those defines.

#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vma/vk_mem_alloc.h>

#include "VulkanDevice.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanDescriptor.hpp"
#include "VulkanFramebuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanSampler.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanSyncPrimitive.hpp"
#include "VulkanTexture.hpp"

#include "Logger/Logger.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace
{
    inline constexpr bool ENABLE_VALIDATION_LAYERS =
#ifdef DEBUG
        true;
#else
        false;
#endif

    // Debug callback forwarded to the Logger.
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             /*messageType*/,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                       /*pUserData*/)
    {
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            LOGWARNING("[Vulkan] ", pCallbackData->pMessage);
        else
            LOGVERBOSE("[Vulkan] ", pCallbackData->pMessage);
        return VK_FALSE;
    }

    void PopulateDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo                 = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
    }
} // namespace

namespace RHI
{

// ── Factory (IRHIDevice::Create) ──────────────────────────────────────────────

std::unique_ptr<IRHIDevice> IRHIDevice::Create(void* nativeWindowHandle)
{
    return std::make_unique<VulkanDevice>(static_cast<GLFWwindow*>(nativeWindowHandle));
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

VulkanDevice::VulkanDevice(GLFWwindow* window)
    : m_Window(window)
{
    VkResult result = volkInitialize();
    assert(result == VK_SUCCESS && "Failed to initialize Volk (is a Vulkan runtime installed?)");

    m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
    m_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    CreateInstance();
    volkLoadInstance(m_Instance);

    SetupDebugMessenger();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
    volkLoadDevice(m_Device);

    CreateAllocator();
    CreateCommandPool();

    LOGINFO("VulkanDevice initialized.");
}

VulkanDevice::~VulkanDevice()
{
    WaitIdle();

    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vmaDestroyAllocator(m_Allocator);
    vkDestroyDevice(m_Device, nullptr);

    if (ENABLE_VALIDATION_LAYERS && m_DebugMessenger != VK_NULL_HANDLE)
    {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn)
            destroyFn(m_Instance, m_DebugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);
}

// ── Private initialisation ───────────────────────────────────────────────────

void VulkanDevice::CreateInstance()
{
    if (ENABLE_VALIDATION_LAYERS)
        assert(CheckValidationLayerSupport() && "Validation layers requested but not available.");

    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName   = "HedgehogEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "HedgehogEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    auto extensions = GetRequiredExtensions();

    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        PopulateDebugCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    assert(result == VK_SUCCESS && "Failed to create VkInstance.");
}

void VulkanDevice::SetupDebugMessenger()
{
    if (!ENABLE_VALIDATION_LAYERS)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugCreateInfo(createInfo);

    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    assert(createFn && "vkCreateDebugUtilsMessengerEXT not available.");
    createFn(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
}

void VulkanDevice::CreateSurface(GLFWwindow* window)
{
    VkResult result = glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface);
    assert(result == VK_SUCCESS && "Failed to create window surface.");
}

void VulkanDevice::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    assert(deviceCount > 0 && "No Vulkan-capable GPUs found.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    // Pick the best suitable device by score.
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device))
            candidates.insert({ GetDeviceScore(device), device });
    }

    assert(!candidates.empty() && "No suitable GPU found.");
    m_PhysicalDevice = candidates.rbegin()->second;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    LOGINFO("Selected GPU: ", props.deviceName);
}

void VulkanDevice::CreateLogicalDevice()
{
    m_Indices = FindQueueFamilies(m_PhysicalDevice);

    std::set<uint32_t> uniqueFamilies = {
        m_Indices.m_GraphicsFamily.value(),
        m_Indices.m_PresentFamily.value()
    };

    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        info.queueFamilyIndex = family;
        info.queueCount       = 1;
        info.pQueuePriorities = &priority;
        queueCreateInfos.push_back(info);
    }

    // Enable Vulkan 1.3 core features: synchronization2 + dynamic rendering.
    VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.fillModeNonSolid  = VK_TRUE;
    features2.pNext                      = &features13;

    VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.pNext                   = &features2;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(m_DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

    if (ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
    }

    VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
    assert(result == VK_SUCCESS && "Failed to create logical device.");

    vkGetDeviceQueue(m_Device, m_Indices.m_GraphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_Indices.m_PresentFamily.value(),  0, &m_PresentQueue);
}

void VulkanDevice::CreateAllocator()
{
    // Tell VMA to use Volk's dynamically loaded function pointers.
    VmaVulkanFunctions vmaFunctions{};
    vmaFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice   = m_PhysicalDevice;
    allocatorInfo.device           = m_Device;
    allocatorInfo.instance         = m_Instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &m_Allocator);
    assert(result == VK_SUCCESS && "Failed to create VMA allocator.");
}

void VulkanDevice::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    // Allow individual command buffer reset (needed by per-frame command lists).
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_Indices.m_GraphicsFamily.value();

    VkResult result = vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);
    assert(result == VK_SUCCESS && "Failed to create command pool.");
}

// ── Device selection helpers ──────────────────────────────────────────────────

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device) const
{
    if (!FindQueueFamilies(device).IsComplete())
        return false;

    if (!CheckDeviceExtensionSupport(device))
        return false;

    // Check swapchain support.
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

    return formatCount > 0 && presentModeCount > 0;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
    for (const auto& ext : available)
        required.erase(ext.extensionName);

    return required.empty();
}

int VulkanDevice::GetDeviceScore(VkPhysicalDevice device) const
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    score += static_cast<int>(props.limits.maxImageDimension2D);
    return score;
}

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.m_GraphicsFamily = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
        if (presentSupport)
            indices.m_PresentFamily = i;

        if (indices.IsComplete())
            break;
    }

    return indices;
}

std::vector<const char*> VulkanDevice::GetRequiredExtensions() const
{
    uint32_t     glfwCount     = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);

    if (ENABLE_VALIDATION_LAYERS)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool VulkanDevice::CheckValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    for (const char* layerName : m_ValidationLayers)
    {
        bool found = false;
        for (const auto& props : available)
        {
            if (strcmp(layerName, props.layerName) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

// ── IRHIDevice resource creation ─────────────────────────────────────────────

std::unique_ptr<IRHIBuffer> VulkanDevice::CreateBuffer(
    size_t size, BufferUsage usage, MemoryUsage memUsage)
{
    return std::make_unique<VulkanBuffer>(*this, size, usage, memUsage);
}

std::unique_ptr<IRHITexture> VulkanDevice::CreateTexture(const TextureDesc& desc)
{
    return std::make_unique<VulkanTexture>(*this, desc);
}

std::unique_ptr<IRHISampler> VulkanDevice::CreateSampler(const SamplerDesc& desc)
{
    return std::make_unique<VulkanSampler>(*this, desc);
}

std::unique_ptr<IRHIShader> VulkanDevice::CreateShader(
    const std::string& filePath, ShaderStage stage)
{
    return std::make_unique<VulkanShader>(*this, filePath, stage);
}

std::unique_ptr<IRHIDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(
    const std::vector<DescriptorBinding>& bindings)
{
    return std::make_unique<VulkanDescriptorSetLayout>(*this, bindings);
}

std::unique_ptr<IRHIDescriptorPool> VulkanDevice::CreateDescriptorPool(
    uint32_t maxSets, const std::vector<PoolSize>& poolSizes)
{
    return std::make_unique<VulkanDescriptorPool>(*this, maxSets, poolSizes);
}

std::unique_ptr<IRHIDescriptorSet> VulkanDevice::AllocateDescriptorSet(
    const IRHIDescriptorPool& pool, const IRHIDescriptorSetLayout& layout)
{
    const auto& vkPool   = static_cast<const VulkanDescriptorPool&>(pool);
    const auto& vkLayout = static_cast<const VulkanDescriptorSetLayout&>(layout);
    return std::make_unique<VulkanDescriptorSet>(*this, vkPool, vkLayout);
}

std::unique_ptr<IRHIRenderPass> VulkanDevice::CreateRenderPass(const RenderPassDesc& desc)
{
    return std::make_unique<VulkanRenderPass>(*this, desc);
}

std::unique_ptr<IRHIFramebuffer> VulkanDevice::CreateFramebuffer(const FramebufferDesc& desc)
{
    return std::make_unique<VulkanFramebuffer>(*this, desc);
}

std::unique_ptr<IRHIPipeline> VulkanDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc)
{
    return std::make_unique<VulkanPipeline>(*this, desc);
}

std::unique_ptr<IRHICommandList> VulkanDevice::CreateCommandList()
{
    return std::make_unique<VulkanCommandList>(*this);
}

std::unique_ptr<IRHISwapchain> VulkanDevice::CreateSwapchain(uint32_t width, uint32_t height)
{
    return std::make_unique<VulkanSwapchain>(*this, width, height);
}

std::unique_ptr<IRHIFence> VulkanDevice::CreateFence(bool signaled)
{
    return std::make_unique<VulkanFence>(*this, signaled);
}

std::unique_ptr<IRHISemaphore> VulkanDevice::CreateSemaphore()
{
    return std::make_unique<VulkanSemaphore>(*this);
}

// ── Submission ────────────────────────────────────────────────────────────────

void VulkanDevice::SubmitCommandList(
    const IRHICommandList&             commandList,
    const std::vector<IRHISemaphore*>& waitSemaphores,
    const std::vector<IRHISemaphore*>& signalSemaphores,
    IRHIFence*                         signalFence)
{
    const auto& vkCmd = static_cast<const VulkanCommandList&>(commandList);

    std::vector<VkSemaphoreSubmitInfo> waitInfos;
    waitInfos.reserve(waitSemaphores.size());
    for (auto* sem : waitSemaphores)
    {
        VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        info.semaphore = static_cast<const VulkanSemaphore&>(*sem).GetHandle();
        info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitInfos.push_back(info);
    }

    std::vector<VkSemaphoreSubmitInfo> signalInfos;
    signalInfos.reserve(signalSemaphores.size());
    for (auto* sem : signalSemaphores)
    {
        VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        info.semaphore = static_cast<const VulkanSemaphore&>(*sem).GetHandle();
        info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        signalInfos.push_back(info);
    }

    VkCommandBufferSubmitInfo cmdInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cmdInfo.commandBuffer = vkCmd.GetHandle();

    VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitInfo.waitSemaphoreInfoCount   = static_cast<uint32_t>(waitInfos.size());
    submitInfo.pWaitSemaphoreInfos      = waitInfos.data();
    submitInfo.commandBufferInfoCount   = 1;
    submitInfo.pCommandBufferInfos      = &cmdInfo;
    submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalInfos.size());
    submitInfo.pSignalSemaphoreInfos    = signalInfos.data();

    const VkFence fence = signalFence
        ? static_cast<const VulkanFence&>(*signalFence).GetHandle()
        : VK_NULL_HANDLE;

    VkResult result = vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, fence);
    assert(result == VK_SUCCESS && "vkQueueSubmit2 failed.");
}

void VulkanDevice::ExecuteImmediately(std::function<void(IRHICommandList&)> func)
{
    auto cmdList = CreateCommandList();
    cmdList->Begin(true /* oneTimeSubmit */);
    func(*cmdList);
    cmdList->End();

    const auto& vkCmd = static_cast<const VulkanCommandList&>(*cmdList);
    VkCommandBuffer cmd = vkCmd.GetHandle();

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);
}

void VulkanDevice::WaitIdle()
{
    vkDeviceWaitIdle(m_Device);
}

} // namespace RHI
