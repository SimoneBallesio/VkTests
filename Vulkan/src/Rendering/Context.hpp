#pragma once

#include "Rendering/VertexData.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace VKP
{

	struct Buffer
	{
		VkBuffer BufferHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
	};

	struct Texture
	{
		VkImage ImageHandle = VK_NULL_HANDLE;
		VkImageView ViewHandle = VK_NULL_HANDLE;
		VkSampler SamplerHandle = VK_NULL_HANDLE;
		VmaAllocation MemoryHandle = VK_NULL_HANDLE;
		uint32_t MipLevels = 1;
	};

	struct QueueIndices
	{
		uint32_t Graphics = UINT32_MAX;
		uint32_t Presentation = UINT32_MAX;
		uint32_t Transfer = UINT32_MAX;

		std::vector<uint32_t> ConcurrentQueues;
		bool DedicatedTransferQueue = false;

		bool AreValid() const;
	};

	struct SwapchainData
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkPresentModeKHR> PresentModes;
		std::vector<VkSurfaceFormatKHR> SurfaceFormats;
		VkExtent2D CurrentExtent;
		VkSurfaceFormatKHR Format;
		VkSurfaceFormatKHR PrevFormat;
		VkSampleCountFlagBits NumSamples = VK_SAMPLE_COUNT_1_BIT;

		bool IsValid() const;
	};

	class Context final
	{
	public:
		Context(Context&) = delete;

		~Context();

		bool BeforeWindowCreation();
		bool AfterWindowCreation();

		bool CreateBuffer(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags);
		bool CopyBuffer(const Buffer &src, const Buffer &dst, VkDeviceSize size);

		bool CreateImage(Texture& texture, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
		bool PopulateImage(Texture& texture, Buffer &staging, uint32_t width, uint32_t height);
		bool CreateImageView(Texture& texture, VkFormat format, VkImageAspectFlags aspectFlags);
		bool CreateImageSampler(Texture& texture);

		bool SubmitTransfer(const std::function<void(VkCommandBuffer)>& fn);
		bool SubmitRender(const std::function<void(VkCommandBuffer)>& fn);

		void SwapBuffers();

		void OnResize(uint32_t width, uint32_t height);

		void GetTransferQueueData(VkSharingMode* mode, uint32_t* numQueues, const uint32_t** queuesPtr);

		Context& operator=(Context&) = delete;

		static Context* Create();
		static Context& Get();

		static inline VkDevice GetDevice() { return s_Device; }
		static inline VmaAllocator GetMemoryAllocator() { return s_Allocator; }

	private:
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysDevice = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

		SwapchainData m_SwapchainData = {};
		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		std::vector<Texture> m_SwapImages;
		Texture m_SwapDepth = {};
		Texture m_SwapSample = {};

		VkViewport m_Viewport = {};
		VkRect2D m_Scissor = {};

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
		VkCommandPool m_TransferPool = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> m_CmdBuffer;

		Buffer m_ObjVBO = {};
		Buffer m_ObjIBO = {};
		Texture m_ObjTexture = {};
		uint32_t m_NumIndices = 0;

		std::vector<Buffer> m_UniformBuffers;

		VkDescriptorSetLayout m_DescSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool m_DescPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> m_DescSets;

		std::vector<VkSemaphore> m_CanAcquireImage; // Can get image from the swapchain for rendering
		std::vector<VkSemaphore> m_CanPresentImage; // Render finished, can push image to the swapchain
		std::vector<VkFence> m_PrevFrameRenderEnded; // GPU operations on the previous frame are finished

		uint32_t m_CurrentFrame = 0;
		float m_AspectRatio = 16.0f / 9.0f;
		bool m_Resized = false;

		static inline VkDevice s_Device = VK_NULL_HANDLE;
		static inline VmaAllocator s_Allocator = VK_NULL_HANDLE;

		static inline Context* s_Context = nullptr;

		Context() = default;

		bool CreateInstance();
		bool CreateSurface();

		bool ChoosePhysicalDevice();
		bool CreateDevice();

		bool CreateMemoryAllocator();

		bool CreateMsaaResources();
		bool CreateDepthResources();

		bool CreateSwapchain();
		bool RecreateSwapchain();
		void DestroySwapchain();

		bool CreateImageViews();

		bool CreateRenderPass();
		bool CreateFramebuffers();

		bool CreateUniformBuffers();

		bool CreateDescriptorSetLayout();
		bool CreateDescriptorPool();
		bool AllocateDescriptorSets();

		bool CreatePipeline();

		bool CreateCommandPool();
		bool AllocateCommandBuffer();

		bool CreateSyncObjects();

		bool LoadObjModel();
		bool LoadObjTexture();

		bool CreateVertexBuffer(const std::vector<Vertex>& vertices);
		bool CreateIndexBuffer(const std::vector<uint32_t>& indices);

		bool RecordCommandBuffer(VkCommandBuffer buffer, size_t imageId);

		VkSampleCountFlagBits GetMsaaMaxSamples() const;

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