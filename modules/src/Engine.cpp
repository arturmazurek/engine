#include "Engine.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "EASTL/algorithm.h"
#include "EASTL/numeric_limits.h"
#include "EASTL/optional.h"
#include "EASTL/set.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "ArraySize.h"
#include "EngineContext.h"
#include "Log.h"

void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
	return new uint8_t[size];
}

void* __cdecl operator new[](size_t size, size_t alignment, size_t offset, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
	assert(false);
	return new uint8_t[size];
}

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

static const char ShadersFolder[] = "D:\\Projects\\Engine\\shaders\\";

static const char* const ValidationLayers[] = {
	"VK_LAYER_KHRONOS_validation"
};

static const char* const DeviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
static const bool EnableValidationLayers = false;
#else
static const bool EnableValidationLayers = true;
#endif

static void initWindow(EngineContext& context);
static void initVulkan(EngineContext& context);
static void cleanupVulkan(EngineContext& context);
static void cleanupWindow(EngineContext& context);

static void createInstance(EngineContext& context);

static bool checkValidationLayerSupport();
static eastl::vector<const char*> getRequiredExtensions();

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData);
static VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo();
static void setupDebugCallback(EngineContext& context);
static void destroyDebugCallback(EngineContext& context);

static void pickPhysicalDevice(EngineContext& context);
static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR onSurface);

static void createLogicalDevice(EngineContext& context);
static void getQueueHandles(EngineContext& context);

static void createSurface(EngineContext& context);
static void destroySurface(EngineContext& context);

static bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice);

struct QueueFamilyIndices
{
	eastl::optional<uint32_t> graphicsFamily;
	eastl::optional<uint32_t> presentFamily;

	bool isComplete() const
	{
		return graphicsFamily.has_value() &&
			presentFamily.has_value();
	}
};
static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	eastl::vector<VkSurfaceFormatKHR> formats;
	eastl::vector<VkPresentModeKHR> presentModes;
};
static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const eastl::vector<VkSurfaceFormatKHR>& availableFormats);
static VkPresentModeKHR chooseSwapPresentMode(const eastl::vector<VkPresentModeKHR>& availablePresentModes);
static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
static void createSwapchain(EngineContext& context);
static void createSwapchainImageViews(EngineContext& context);

static void createRenderPass(EngineContext& context);
static void createGraphicsPipeline(EngineContext& context);
static eastl::vector<uint8_t> readShaderFile(const char* filename);
static VkShaderModule createShaderModule(VkDevice device, const eastl::vector<uint8_t>& code);

static void createFramebuffers(EngineContext& context);

static void createCommandPool(EngineContext& context);
static void createCommandBuffer(EngineContext& context);
static void recordCommandBuffer(EngineContext& context, VkCommandBuffer commandBuffer, uint32_t imageIndex);

static void createSyncObjects(EngineContext& context);

static void drawFrame(EngineContext& context);

void init(EngineContext& context)
{
	initWindow(context);
	initVulkan(context);
	
}

void run(EngineContext& context)
{
	while (!glfwWindowShouldClose(context.window)) 
	{
		glfwPollEvents();
		drawFrame(context);
	}

	vkDeviceWaitIdle(context.device);
}

void cleanup(EngineContext& context)
{
	cleanupVulkan(context);
	cleanupWindow(context);
}

static void initWindow(EngineContext& context)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	context.window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

static void initVulkan(EngineContext& context)
{
	createInstance(context);
	setupDebugCallback(context);
	createSurface(context);
	pickPhysicalDevice(context);
	createLogicalDevice(context);
	getQueueHandles(context);
	createSwapchain(context);
	createSwapchainImageViews(context);
	createRenderPass(context);
	createGraphicsPipeline(context);
	createFramebuffers(context);
	createCommandPool(context);
	createCommandBuffer(context);
	createSyncObjects(context);
}

static void cleanupWindow(EngineContext& context)
{
	glfwDestroyWindow(context.window);
	glfwTerminate();
}

static void cleanupVulkan(EngineContext& context)
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(context.device, context.imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(context.device, context.renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(context.device, context.inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(context.device, context.commandPool, nullptr);
	for (VkFramebuffer& framebuffer : context.swapchainFramebuffers)
	{
		vkDestroyFramebuffer(context.device, framebuffer, nullptr);
	}
	vkDestroyPipeline(context.device, context.pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, context.pipelineLayout, nullptr);
	vkDestroyRenderPass(context.device, context.renderPass, nullptr);
	for (VkImageView& swapchainImageView : context.swapchainImageViews)
	{
		vkDestroyImageView(context.device, swapchainImageView, nullptr);
	}
	vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
	vkDestroyDevice(context.device, nullptr);
	destroySurface(context);
	destroyDebugCallback(context);
	vkDestroyInstance(context.instance, nullptr);
}

static void createInstance(EngineContext& context)
{
	if (EnableValidationLayers && !checkValidationLayerSupport())
	{
		Log::error("Validation layers needed, but not available.\n");
		std::exit(1);
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.pEngineName = "EngineUnknown";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	eastl::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (EnableValidationLayers)
	{
		createInfo.enabledLayerCount = ARRAY_SIZE(ValidationLayers);
		createInfo.ppEnabledLayerNames = ValidationLayers;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = makeDebugMessengerCreateInfo();
		createInfo.pNext = &debugCreateInfo;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &context.instance);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Couldn't create instance.\n");
	}
}

static bool checkValidationLayerSupport()
{
	uint32_t numLayers = 0;
	vkEnumerateInstanceLayerProperties(&numLayers, nullptr);

	eastl::vector<VkLayerProperties> availableLayers(numLayers);
	vkEnumerateInstanceLayerProperties(&numLayers, availableLayers.data());

	for (const char* validationLayer : ValidationLayers)
	{
		bool layerFound = false;

		for (const VkLayerProperties& layerProperties : availableLayers)
		{
			if (strcmp(validationLayer, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

static eastl::vector<const char*> getRequiredExtensions()
{
	uint32_t numGlfwExtensions = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&numGlfwExtensions);

	eastl::vector<const char*> extensions(glfwExtensions, glfwExtensions + numGlfwExtensions);

	if (EnableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	//static const VkDebugUtilsMessageSeverityFlagBitsEXT minSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	static const VkDebugUtilsMessageSeverityFlagBitsEXT minSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	//static const VkDebugUtilsMessageSeverityFlagBitsEXT minSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

	if (messageSeverity < minSeverity)
	{
		return VK_FALSE;
	}

	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		Log::error("Validation layer: %s\n", pCallbackData->pMessage);
	}
	else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		Log::warning("Validation layer: %s\n", pCallbackData->pMessage);
	}
	else
	{
		Log::log("Validation layer: %s\n", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

static void setupDebugCallback(EngineContext& context)
{
	if (!EnableValidationLayers)
	{
		return;
	}

	PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT"));
	if (!func)
	{
		Log::fatal("Cannot load function for debug messenger.\n");
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo = makeDebugMessengerCreateInfo();
	func(context.instance, &createInfo, nullptr, &context.debugMessenger);
}

static void destroyDebugCallback(EngineContext& context)
{
	if (!EnableValidationLayers)
	{
		return;
	}

	PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (!func)
	{
		Log::fatal("Can't find extension address");
	}

	func(context.instance, context.debugMessenger, nullptr);
}

static VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo() 
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = vulkanDebugCallback;

	return createInfo;
}

static void pickPhysicalDevice(EngineContext& context)
{
	context.physicalDevice = VK_NULL_HANDLE;

	uint32_t numDevices = 0;
	vkEnumeratePhysicalDevices(context.instance, &numDevices, nullptr);

	if (numDevices == 0)
	{
		Log::fatal("No Vulkan-capable devices");
	}

	eastl::vector<VkPhysicalDevice> devices(numDevices);
	vkEnumeratePhysicalDevices(context.instance, &numDevices, devices.data());

	for (const VkPhysicalDevice& device : devices)
	{
		if (isDeviceSuitable(device, context.surface))
		{
			context.physicalDevice = device;
			break;
		}
	}

	if (context.physicalDevice == VK_NULL_HANDLE)
	{
		Log::fatal("No suitable GPUs found");
	}
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR onSurface)
{
	QueueFamilyIndices indices = findQueueFamilies(device, onSurface);
	if (!indices.isComplete())
	{
		return false;
	}

	bool extensionsSupported = checkDeviceExtensionSupport(device);
	if (!extensionsSupported)
	{
		return false;
	}

	bool swapChainSuitable = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainDetails = querySwapChainSupport(device, onSurface);
		swapChainSuitable = !swapChainDetails.formats.empty() && !swapChainDetails.presentModes.empty();
	}
	if (!swapChainSuitable)
	{
		return false;
	}

	return true;
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t numQueueFamilies = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, nullptr);

	eastl::vector<VkQueueFamilyProperties> queueFamilies(numQueueFamilies);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilies.data());

	for (int i = 0; i < queueFamilies.size(); ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}

		if (indices.isComplete())
		{
			break;
		}
	}

	return indices;
}

static void createLogicalDevice(EngineContext& context)
{
	QueueFamilyIndices indices = findQueueFamilies(context.physicalDevice, context.surface);
	assert(indices.isComplete());

	// deduplicated queue indices
	eastl::vector<uint32_t> uniqueQueueFamilies;
	uniqueQueueFamilies.push_back(indices.graphicsFamily.value());
	if (eastl::find(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end(), indices.presentFamily.value()) == uniqueQueueFamilies.end())
	{
		uniqueQueueFamilies.push_back(indices.presentFamily.value());
	}

	eastl::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	float queuePriority = 1.0f;
	for (uint32_t queueIndex : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	
	VkPhysicalDeviceFeatures deviceFeatures = {};
	createInfo.pEnabledFeatures = &deviceFeatures;

	if (EnableValidationLayers)
	{
		createInfo.enabledLayerCount = ARRAY_SIZE(ValidationLayers);
		createInfo.ppEnabledLayerNames = ValidationLayers;
	}

	createInfo.enabledExtensionCount = ARRAY_SIZE(DeviceExtensions);
	createInfo.ppEnabledExtensionNames = DeviceExtensions;

	VkResult result = vkCreateDevice(context.physicalDevice, &createInfo, nullptr, &context.device);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Can't create device, result is %d", result);
	}
}

static void getQueueHandles(EngineContext& context)
{
	QueueFamilyIndices indices = findQueueFamilies(context.physicalDevice, context.surface);

	vkGetDeviceQueue(context.device, indices.graphicsFamily.value(), 0, &context.graphicsQueue);
	vkGetDeviceQueue(context.device, indices.presentFamily.value(), 0, &context.presentQueue);
}

static void createSurface(EngineContext& context)
{
	VkResult result = glfwCreateWindowSurface(context.instance, context.window, nullptr, &context.surface);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Cant' create surface");
	}
}

static void destroySurface(EngineContext& context)
{
	vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
{
	uint32_t numExtensions = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExtensions, nullptr);

	eastl::vector<VkExtensionProperties> availableExtensions(numExtensions);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExtensions, availableExtensions.data());

	// true if an extension from DeviceExtensions is supported, false if no
	eastl::set<eastl::string> requiredExtensions(eastl::begin(DeviceExtensions), eastl::end(DeviceExtensions));

	for (const VkExtensionProperties& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t numFormats = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &numFormats, nullptr);

	if (numFormats)
	{
		details.formats.resize(numFormats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &numFormats, details.formats.data());
	}

	uint32_t numPresentModes = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &numPresentModes, nullptr);

	if (numPresentModes)
	{
		details.presentModes.resize(numPresentModes);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &numPresentModes, details.presentModes.data());
	}

	return details;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const eastl::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const eastl::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const VkPresentModeKHR& availableMode : availablePresentModes)
	{
		if (availableMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availableMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
	if (capabilities.currentExtent.width != eastl::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent =
		{
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = eastl::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = eastl::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

static void createSwapchain(EngineContext& context)
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(context.physicalDevice, context.surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, context.window);

	uint32_t numImages = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && numImages > swapChainSupport.capabilities.maxImageCount)
	{
		numImages = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = context.surface;
	createInfo.minImageCount = numImages;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(context.physicalDevice, context.surface);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = ARRAY_SIZE(queueFamilyIndices);
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = nullptr;

	VkResult result = vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Couldn't create swapchain");
	}

	uint32_t numSwapchainImages = 0;
	vkGetSwapchainImagesKHR(context.device, context.swapchain, &numSwapchainImages, nullptr);
	context.swapchainImages.resize(numSwapchainImages);
	vkGetSwapchainImagesKHR(context.device, context.swapchain, &numSwapchainImages, context.swapchainImages.data());

	context.swapchainFormat = surfaceFormat.format;
	context.swapchainExtent = extent;
}

static void createSwapchainImageViews(EngineContext& context)
{
	context.swapchainImageViews.resize(context.swapchainImages.size());

	for (int i = 0; i < context.swapchainImages.size(); ++i)
	{
		const VkImage& swapchainImage = context.swapchainImages[i];

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImage;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = context.swapchainFormat;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(context.device, &createInfo, nullptr, &context.swapchainImageViews[i]);
		if (result != VK_SUCCESS)
		{
			Log::fatal("Cannot create swapchain image views");
		}
	}
}

static void createGraphicsPipeline(EngineContext& context)
{
	eastl::vector<uint8_t> vertShaderCode = readShaderFile("shader.vert.spv");
	eastl::vector<uint8_t> fragShaderCode = readShaderFile("shader.frag.spv");

	VkShaderModule vertShadereModule = createShaderModule(context.device, vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(context.device, fragShaderCode);

	VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
	vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStageCreateInfo.module = vertShadereModule;
	vertStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
	fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageCreateInfo.module = fragShaderModule;
	fragStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageCreateInfo, fragStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.width = static_cast<float>(context.swapchainExtent.width);
	viewport.height = static_cast<float>(context.swapchainExtent.height);
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent = context.swapchainExtent;

	VkDynamicState dynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = ARRAY_SIZE(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo	colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	VkResult pipelineLayoutResult = vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &context.pipelineLayout);
	if (pipelineLayoutResult != VK_SUCCESS)
	{
		Log::fatal("Couldn't create pipeline layout");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = ARRAY_SIZE(shaderStages);
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = context.pipelineLayout;
	pipelineInfo.renderPass = context.renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VkResult pipelineResult = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &context.pipeline);
	if (pipelineResult != VK_SUCCESS)
	{
		Log::fatal("Couldn't create pipeline");
	}

	vkDestroyShaderModule(context.device, fragShaderModule, nullptr);
	vkDestroyShaderModule(context.device, vertShadereModule, nullptr);
}

static eastl::vector<uint8_t> readShaderFile(const char* filename)
{
	eastl::string fullName = ShadersFolder;
	fullName += filename;

	FILE* f = fopen(fullName.c_str(), "rb");
	fseek(f, 0, SEEK_END);

	uint64_t fileLength = ftell(f);

	eastl::vector<uint8_t> loadedData(fileLength);
	fseek(f, 0, SEEK_SET);
	fread(loadedData.data(), 1, fileLength, f);
	fclose(f);

	return loadedData;
}

static VkShaderModule createShaderModule(VkDevice device, const eastl::vector<uint8_t>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	assert(reinterpret_cast<intptr_t>(code.data()) % sizeof(uint32_t) == 0);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Cannot create shader module");
	}

	return shaderModule;
}

static void createRenderPass(EngineContext& context)
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = context.swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkResult passCreateResult = vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &context.renderPass);
	if (passCreateResult != VK_SUCCESS)
	{
		Log::fatal("Couldn't create render pass");
	}
}

static void createFramebuffers(EngineContext& context)
{
	context.swapchainFramebuffers.resize(context.swapchainImageViews.size());

	for (int i = 0; i < context.swapchainImageViews.size(); ++i)
	{
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = context.renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &context.swapchainImageViews[i];
		framebufferInfo.width = context.swapchainExtent.width;
		framebufferInfo.height = context.swapchainExtent.height;
		framebufferInfo.layers = 1;

		VkResult createResult = vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &context.swapchainFramebuffers[i]);
		if (createResult != VK_SUCCESS)
		{
			Log::fatal("Cannot create framebuffer");
		}
	}
}

static void createCommandPool(EngineContext& context)
{
	QueueFamilyIndices indices = findQueueFamilies(context.physicalDevice, context.surface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

	VkResult poolCreateResult = vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool);
	if (poolCreateResult != VK_SUCCESS)
	{
		Log::fatal("Cannot create command pool");
	}
}

static void createCommandBuffer(EngineContext& context)
{
	VkCommandBufferAllocateInfo bufferAllocationInfo = {};
	bufferAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferAllocationInfo.commandPool = context.commandPool;
	bufferAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferAllocationInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	VkResult bufferAllocationResult = vkAllocateCommandBuffers(context.device, &bufferAllocationInfo, context.commandBuffers);
	if (bufferAllocationResult != VK_SUCCESS)
	{
		Log::fatal("Cannot allocate command buffers");
	}
}

static void recordCommandBuffer(EngineContext& context, VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Cannot begin command buffer");
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = context.renderPass;
	renderPassInfo.framebuffer = context.swapchainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = context.swapchainExtent;

	VkClearValue clearColor = {};
	clearColor.color.float32[3] = 1.0f; // alpha 1

	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline);

	VkViewport viewport = {};
	viewport.width = static_cast<float>(context.swapchainExtent.width);
	viewport.height = static_cast<float>(context.swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = context.swapchainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS)
	{
		Log::fatal("Failed to record command buffer");
	}
}

static void createSyncObjects(EngineContext& context)
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{

		VkResult createResult = vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.imageAvailableSemaphores[i]);
		if (createResult != VK_SUCCESS)
		{
			Log::fatal("Couldn't create image available semaphore");
		}

		createResult = vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &context.renderFinishedSemaphores[i]);
		if (createResult != VK_SUCCESS)
		{
			Log::fatal("Couldn't create render finished available semaphore");
		}

		createResult = vkCreateFence(context.device, &fenceInfo, nullptr, &context.inFlightFences[i]);
		if (createResult != VK_SUCCESS)
		{
			Log::fatal("Couldn't create inflight fence");
		}
	}
}

static void drawFrame(EngineContext& context) 
{
	vkWaitForFences(context.device, 1, &context.inFlightFences[context.currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(context.device, 1, &context.inFlightFences[context.currentFrame]);

	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, context.imageAvailableSemaphores[context.currentFrame], VK_NULL_HANDLE, &imageIndex);

	vkResetCommandBuffer(context.commandBuffers[context.currentFrame], 0);

	recordCommandBuffer(context, context.commandBuffers[context.currentFrame], imageIndex);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &context.imageAvailableSemaphores[context.currentFrame];
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &context.commandBuffers[context.currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &context.renderFinishedSemaphores[context.currentFrame];

	VkResult submitResult = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, context.inFlightFences[context.currentFrame]);
	if (submitResult != VK_SUCCESS)
	{
		Log::fatal("Couldn't submit commandlist");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &context.swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &context.renderFinishedSemaphores[context.currentFrame];
	VkResult presentResult = vkQueuePresentKHR(context.presentQueue, &presentInfo);

	context.currentFrame = (context.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
