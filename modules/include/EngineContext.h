#pragma once

#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <EASTL/vector.h>

#include "Constants.h"

struct EngineContext
{
	GLFWwindow* window;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	eastl::vector<VkImage> swapchainImages;
	VkFormat swapchainFormat;
	VkExtent2D swapchainExtent;
	eastl::vector<VkImageView> swapchainImageViews;
	eastl::vector<VkFramebuffer> swapchainFramebuffers;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];

	VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

	uint32_t currentFrame;
};
