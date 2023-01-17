#pragma once

#include "Rendering/Buffer.hpp"
#include "Rendering/State.hpp"
#include "Rendering/Renderable.hpp"
#include "Rendering/Texture.hpp"
#include "Rendering/Material.hpp"

#include <vulkan/vulkan.h>

namespace VKP
{

	struct Camera;
	struct Renderable;
	class MeshCache;

	struct GlobalData
	{
		glm::mat4 VP;
	};

	struct Renderer3DData
	{
		Buffer GlobalUBO = {};
		VkDescriptorSet GlobalDataDescSet = VK_NULL_HANDLE;
		uint32_t GlobalDataDescSetOffset = 0;

		VkRenderPass DefaultPass = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> DefaultFramebuffers = {};

		MaterialCache* Materials = nullptr;
		TextureCache* Textures = nullptr;
		MeshCache* Meshes = nullptr;
	};

	class Renderer3D final
	{
	public:
		Renderer3D(Renderer3D&) = delete;
		~Renderer3D() = default;

		Renderer3D& operator=(Renderer3D&) = delete;

		static bool Init();
		static void Destroy();

		static bool OnResize(uint32_t width, uint32_t height);

		static VkRenderPass GetDefaultRenderPass();

		static void SubmitRenderable(Renderable* obj);
		static void Flush(Camera* camera);

	private:
		static Renderer3DData s_Data;
		static std::vector<Renderable*> s_Renderables;

		Renderer3D() = default;

		static bool CreateRenderPass();
		static bool CreateFramebuffers();
		static bool CreateBuffers();
	};

}