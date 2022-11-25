#pragma once

#include <vulkan/vulkan.h>

namespace VKP
{

	struct QueueIndices
	{
		uint32_t Graphics = UINT32_MAX;
		uint32_t Presentation = UINT32_MAX;
		uint32_t Transfer = UINT32_MAX;

		bool AreValid() const;
	};

	struct SwapchainData
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkPresentModeKHR> PresentModes;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		VkExtent2D CurrentExtent;
		VkSurfaceFormatKHR Format;

		bool IsValid() const;
	};

	class Context final
	{
	public:
		Context(Context&) = delete;

		~Context();

		bool BeforeWindowCreation();
		bool AfterWindowCreation();

		void SwapBuffers();

		Context& operator=(Context&) = delete;

		static Context* Create();
		static Context& Get();

	private:
		VkInstance m_Instance = VK_NULL_HANDLE;

		VkPhysicalDevice m_PhysDevice = VK_NULL_HANDLE;
		VkDevice m_Device = VK_NULL_HANDLE;

		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

		SwapchainData m_SwapchainData = {};
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_SwapImages;
		std::vector<VkImageView> m_SwapImageViews;

		VkRenderPass m_DefaultRenderPass = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> m_DefaultFramebuffers;

		VkPipelineLayout m_DefaultPipeLayout = VK_NULL_HANDLE;
		VkPipeline m_DefaultPipeline = VK_NULL_HANDLE;

#ifdef VKP_DEBUG
		VkDebugUtilsMessengerEXT m_Debug = VK_NULL_HANDLE;
#endif

		QueueIndices m_QueueIndices = {};
		VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue m_PresentQueue = VK_NULL_HANDLE;
		VkQueue m_TransferQueue = VK_NULL_HANDLE;

		VkCommandPool m_CmdPool = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> m_CmdBuffer;

		std::vector<VkSemaphore> m_CanAcquireImage; // Can get image from the swapchain for rendering
		std::vector<VkSemaphore> m_CanPresentImage; // Render finished, can push image to the swapchain
		std::vector<VkFence> m_PrevFrameRenderEnded; // GPU operations on the previous frame are finished

		uint32_t m_CurrentFrame = 0;

		static inline Context* s_Context = nullptr;

		Context() = default;

		bool CreateInstance();
		bool CreateSurface();

		bool ChoosePhysicalDevice();
		bool CreateDevice();

		bool CreateSwapchain();
		bool RecreateSwapchain();
		void DestroySwapchain();

		bool CreateImageViews();

		bool CreateRenderPass();
		bool CreateFramebuffers();

		bool CreatePipeline();

		bool CreateCommandPool();
		bool AllocateCommandBuffer();

		bool CreateSyncObjects();

		bool RecordCommandBuffer(const VkCommandBuffer& buffer, size_t imageId);

		VkExtent2D ChooseExtents() const;
		VkSurfaceFormatKHR ChooseSurfaceFormat() const;
		VkPresentModeKHR ChoosePresentMode() const;

#ifdef VKP_DEBUG

		VKAPI_ATTR VkResult VKAPI_CALL CreateDebugUtilsMessengerEXT(
			VkInstance instance,
			const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator,
			VkDebugUtilsMessengerEXT* pMessenger);

		VKAPI_ATTR void VKAPI_CALL DestroyDebugUtilsMessengerEXT(
			VkInstance instance,
			VkDebugUtilsMessengerEXT messenger,
			const VkAllocationCallbacks* pAllocator);

		static VkBool32 DebugUtilsMessengerCallbackEXT(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageTypes,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

#endif
	};

}