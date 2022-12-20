#pragma once

#include "Rendering/Buffer.hpp"
#include "Rendering/Descriptors.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/VertexData.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <deque>

namespace VKP
{

	struct DeletionQueue
	{
		std::deque<std::function<void()>> Queue;

		void Push(std::function<void()>&& fn);
		void Flush();
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

	struct PushConstantData
	{
		glm::mat4 Model = glm::mat4(1.0f);
	};

	struct CameraData
	{
		glm::mat4 Projection;
		glm::mat4 View;
		glm::mat4 VP;
	};

	struct Renderable;

	class Context final
	{
	public:
		Context(Context&) = delete;

		~Context();

		bool BeforeWindowCreation();
		bool AfterWindowCreation();

		bool CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags bufferUsage, VmaAllocationCreateFlags memoryFlags);
		bool CopyBuffer(const Buffer& src, const Buffer& dst, VkDeviceSize size);
		bool CreateVertexBuffer(Buffer& vbo, const std::vector<Vertex>& vertices);
		bool CreateIndexBuffer(Buffer& ibo, const std::vector<uint32_t>& indices);
		bool CreateUniformBuffer(Buffer& ubo, VkDeviceSize size);
		void DestroyBuffer(Buffer& buffer);

		bool CreateImage(Texture& texture, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
		bool PopulateImage(Texture& texture, Buffer& staging, uint32_t width, uint32_t height);
		bool CreateImageView(Texture& texture, VkFormat format, VkImageAspectFlags aspectFlags);
		bool CreateImageSampler(Texture& texture);
		void DestroyTexture(Texture& texture);

		DescriptorSetAllocator* GetDescriptorSetAllocator() const;
		VkRenderPass GetDefaultRenderPass() const;

		bool SubmitTransfer(const std::function<void(VkCommandBuffer)>& fn);
		void SubmitRenderable(Renderable* renderable);

		void SwapBuffers();

		void OnResize(uint32_t width, uint32_t height);

		void GetTransferQueueData(VkSharingMode* mode, uint32_t* numQueues, const uint32_t** queuesPtr);
		VkSampleCountFlagBits GetMsaaMaxSamples() const;

		Context& operator=(Context&) = delete;

		static Context* Create();
		static Context& Get();

		static inline VkDevice GetDevice() { return s_Device; }
		static inline VmaAllocator GetMemoryAllocator() { return s_Allocator; }

	private:
		DeletionQueue m_DeletionQueue = {};

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

		PushConstantData m_PushData = {};

		std::vector<Renderable*> m_Renderables = {};

		uint32_t m_MinUboAlignment = 0;
		uint32_t m_MinSsboAlignment = 0;

		Buffer m_UBO = {};

		DescriptorSetAllocator* m_DescSetAllocator = nullptr;
		std::vector<DescriptorSetAllocator*> m_DynDescSetAllocators = {};

		VkDescriptorSet m_SceneDataSet = VK_NULL_HANDLE;
		uint32_t m_SceneDataOffset = 0;

		DescriptorSetLayoutCache* m_DescSetLayoutCache = nullptr;
		ShaderModuleCache* m_ShaderModuleCache = nullptr;
		PipelineLayoutCache* m_PipeLayoutCache = nullptr;
		MaterialCache* m_MaterialCache = nullptr;

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

		bool CreateCommandPool();
		bool AllocateCommandBuffer();

		bool CreateSyncObjects();

		bool CreateCaches();
		bool CreateDescriptorSetAllocators();

		bool RecordCommandBuffer(VkCommandBuffer buffer, size_t imageId);

		VkSampleCountFlagBits GetMsaaMaxSamples(const VkPhysicalDeviceProperties& props) const;

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