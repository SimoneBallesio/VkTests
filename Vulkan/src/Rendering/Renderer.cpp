#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Renderer.hpp"
#include "Rendering/Camera.hpp"
#include "Rendering/Mesh.hpp"
#include "Rendering/State.hpp"

namespace VKP
{

	Renderer3DData Renderer3D::s_Data = {};
	MeshPass Renderer3D::s_ForwardPass = {};

	bool Renderer3D::Init()
	{
		bool success = CreateRenderPass();
		if (success) success = CreateFramebuffers();
		if (success) success = CreateBuffers();

		if (success)
		{
			s_Data.Materials = MaterialCache::Create(Impl::State::Data->Device);
			s_Data.Textures = TextureCache::Create();
			s_Data.Meshes = MeshCache::Create();

			s_ForwardPass.UnbatchedObjects.reserve(10000);

			Impl::State::Data->DeletionQueue.Push([=]()
			{
				delete s_Data.Textures;
				delete s_Data.Materials;
				delete s_Data.Meshes;
			});
		}

		return success;
	}

	void Renderer3D::Destroy()
	{
		Impl::DestroyBuffer(Impl::State::Data, &s_Data.GlobalIBO);
		Impl::DestroyBuffer(Impl::State::Data, &s_Data.GlobalVBO);

		Impl::DestroyBuffer(Impl::State::Data, &s_Data.ObjectSSBO);
		Impl::DestroyBuffer(Impl::State::Data, &s_Data.GlobalUBO);

		for (auto f : s_Data.DefaultFramebuffers)
			vkDestroyFramebuffer(Impl::State::Data->Device, f, nullptr);

		if (s_Data.DefaultPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(Impl::State::Data->Device, s_Data.DefaultPass, nullptr);
	}

	bool Renderer3D::OnResize(uint32_t width, uint32_t height)
	{
		for (auto f : s_Data.DefaultFramebuffers)
			vkDestroyFramebuffer(Impl::State::Data->Device, f, nullptr);

		VkFramebufferCreateInfo frameInfo = {};
		frameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameInfo.width = Impl::State::Data->SwcData.CurrentExtent.width;
		frameInfo.height = Impl::State::Data->SwcData.CurrentExtent.height;
		frameInfo.renderPass = s_Data.DefaultPass;
		frameInfo.layers = 1;

		for (size_t i = 0; i < s_Data.DefaultFramebuffers.size(); i++)
		{
			std::vector<VkImageView> views = { Impl::State::Data->SwcMsaaTexture.ViewHandle, Impl::State::Data->SwcDepthTexture.ViewHandle, Impl::State::Data->SwcImages[i].ViewHandle };

			frameInfo.pAttachments = views.data();
			frameInfo.attachmentCount = views.size();

			if (vkCreateFramebuffer(Impl::State::Data->Device, &frameInfo, nullptr, &s_Data.DefaultFramebuffers[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create framebuffer for Vulkan swapchain image view #{}", i);
				return false;
			}
		}

		if (Impl::State::Data->SwcData.PrevFormat.colorSpace != Impl::State::Data->SwcData.Format.colorSpace || Impl::State::Data->SwcData.PrevFormat.format != Impl::State::Data->SwcData.Format.format)
		{
			VKP_WARN("Mismatch between previous and current presentation formats");

			vkDestroyRenderPass(Impl::State::Data->Device, s_Data.DefaultPass, nullptr);
			return CreateRenderPass();
		}

		return true;
	}

	VkRenderPass Renderer3D::GetDefaultRenderPass()
	{
		VKP_ASSERT(s_Data.DefaultPass != VK_NULL_HANDLE, "Default render pass has not been initialized");
		return s_Data.DefaultPass;
	}

	void Renderer3D::SubmitRenderable(Renderable* obj)
	{
		VKP_ASSERT(obj != nullptr, "Null-pointer submitted as Renderable");
		s_ForwardPass.UnbatchedObjects.push_back(obj);
	}

	void Renderer3D::Flush(Camera* camera)
	{
		VkDescriptorBufferInfo uboInfo = {};
		uboInfo.buffer = s_Data.GlobalUBO.BufferHandle;
		uboInfo.offset = 0;
		uboInfo.range = s_Data.GlobalUBO.Size;

		DescriptorSetFactory builder(Impl::State::Data->Device, Impl::State::Data->DescriptorSetLayouts, Impl::State::Data->Frames[Impl::State::Data->CurrentFrame].DynDescriptorSetAlloc);
		builder.BindBuffer(0, &uboInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);
		builder.Build(s_Data.GlobalDataDescSet);
		s_Data.GlobalDataDescSetOffset = Impl::State::Data->CurrentFrame * s_Data.GlobalUBO.AlignedSize;

		VkDescriptorBufferInfo ssboInfo = {};
		ssboInfo.buffer = s_Data.ObjectSSBO.BufferHandle;
		ssboInfo.offset = 0;
		ssboInfo.range = s_Data.ObjectSSBO.Size;

		builder.BindBuffer(0, &ssboInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		builder.Build(s_Data.ObjectDataDescSet);

		std::vector<VkClearValue> clearColors(2);

		clearColors[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
		clearColors[1].depthStencil = {1.0f, 0};

		VkRenderPassBeginInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		passInfo.renderArea.offset = {0, 0};
		passInfo.renderArea.extent = Impl::State::Data->SwcData.CurrentExtent;
		passInfo.framebuffer = s_Data.DefaultFramebuffers[Impl::State::Data->SwcData.CurrentImageId];
		passInfo.renderPass = s_Data.DefaultPass;
		passInfo.pClearValues = clearColors.data();
		passInfo.clearValueCount = clearColors.size();

		vkCmdBeginRenderPass(Impl::State::Data->CurrentCmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(Impl::State::Data->CurrentCmdBuffer, 0, 1, &Impl::State::Data->Viewport);
		vkCmdSetScissor(Impl::State::Data->CurrentCmdBuffer, 0, 1, &Impl::State::Data->Scissor);

		// if (s_Renderables.size() > 0)
		// {
		// 	VkPipeline pipeline = VK_NULL_HANDLE;
		// 	VkDescriptorSet textureSet = VK_NULL_HANDLE;
		// 	VkDeviceSize offset = 0;

		// 	const float aspect = (float)Impl::State::Data->SurfaceWidth / (float)Impl::State::Data->SurfaceHeight;
		// 	const glm::mat4 proj = glm::perspective(glm::radians(70.0f), aspect, 0.1f, 100.0f);
		// 	const glm::mat4 vp = proj * camera->ViewMatrix();

		// 	uint8_t* data = nullptr;
		// 	const uint32_t uboOffset = s_Data.GlobalUBO.AlignedSize * Impl::State::Data->CurrentFrame;

		// 	vmaMapMemory(Impl::State::Data->MemAllocator, s_Data.GlobalUBO.MemoryHandle, (void**)&data);

		// 	data += uboOffset;
		// 	memcpy(data, &vp[0][0], sizeof(glm::mat4));

		// 	vmaUnmapMemory(Impl::State::Data->MemAllocator, s_Data.GlobalUBO.MemoryHandle);

		// 	glm::mat4* matrices;
		// 	vmaMapMemory(Impl::State::Data->MemAllocator, s_Data.ObjectSSBO.MemoryHandle, (void**)&matrices);

		// 	for (size_t i = 0; i < s_Renderables.size(); i++)
		// 		memcpy(&matrices[i], &s_Renderables[i]->Matrix[0][0], sizeof(glm::mat4));

		// 	vmaUnmapMemory(Impl::State::Data->MemAllocator, s_Data.ObjectSSBO.MemoryHandle);

		// 	for (size_t i = 0; i < s_Renderables.size(); i++)
		// 	{
		// 		const auto& r = s_Renderables[i];

		// 		if (pipeline != r->Mat->Template->Pipe)
		// 		{
		// 			pipeline = r->Mat->Template->Pipe;

		// 			vkCmdBindPipeline(Impl::State::Data->CurrentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		// 			vkCmdBindDescriptorSets(Impl::State::Data->CurrentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->Mat->Template->PipeLayout, 0, 1, &s_Data.GlobalDataDescSet, 1, &s_Data.GlobalDataDescSetOffset);
		// 			vkCmdBindDescriptorSets(Impl::State::Data->CurrentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->Mat->Template->PipeLayout, 1, 1, &s_Data.ObjectDataDescSet, 0, nullptr);
		// 		}

		// 		if (textureSet != r->Mat->TextureSet)
		// 		{
		// 			textureSet = r->Mat->TextureSet;
		// 			vkCmdBindDescriptorSets(Impl::State::Data->CurrentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->Mat->Template->PipeLayout, 2, 1, &textureSet, 0, nullptr);
		// 		}

		// 		vkCmdBindVertexBuffers(Impl::State::Data->CurrentCmdBuffer, 0, 1, &r->Model->VBO.BufferHandle, &offset);
		// 		vkCmdBindIndexBuffer(Impl::State::Data->CurrentCmdBuffer, r->Model->IBO.BufferHandle, 0, VK_INDEX_TYPE_UINT32);

		// 		vkCmdDrawIndexed(Impl::State::Data->CurrentCmdBuffer, r->Model->NumIndices, 1, 0, 0, i);
		// 	}
		// }

		vkCmdEndRenderPass(Impl::State::Data->CurrentCmdBuffer);
	}

	bool Renderer3D::CreateRenderPass()
	{
		std::vector<VkAttachmentDescription> attachments;
		attachments.reserve(2);

		auto& colorPass = attachments.emplace_back();
		colorPass.samples = Impl::State::Data->SwcData.NumSamples;
		colorPass.format = Impl::State::Data->SwcData.Format.format;
		colorPass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorPass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorPass.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		auto& depthPass = attachments.emplace_back();
		depthPass.samples = Impl::State::Data->SwcData.NumSamples;
		depthPass.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		depthPass.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthPass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthPass.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		auto& resolvePass = attachments.emplace_back();
		resolvePass.format = Impl::State::Data->SwcData.Format.format;
		resolvePass.samples = VK_SAMPLE_COUNT_1_BIT;
		resolvePass.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolvePass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		resolvePass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolvePass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		resolvePass.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		resolvePass.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthRef = {};
		depthRef.attachment = 1;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference resolveRef = {};
		resolveRef.attachment = 2;
		resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = &colorRef;
		subpass.colorAttachmentCount = 1;
		subpass.pResolveAttachments = &resolveRef;
		subpass.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency subd = {};
		subd.srcSubpass = VK_SUBPASS_EXTERNAL;
		subd.dstSubpass = 0;
		subd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		subd.srcAccessMask = 0;
		subd.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		subd.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo passInfo = {};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.pSubpasses = &subpass;
		passInfo.subpassCount = 1;
		passInfo.pAttachments = attachments.data();
		passInfo.attachmentCount = attachments.size();
		passInfo.pDependencies = &subd;
		passInfo.dependencyCount = 1;

		if (vkCreateRenderPass(Impl::State::Data->Device, &passInfo, nullptr, &s_Data.DefaultPass) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create default render pass");
			return false;
		}

		return true;
	}

	bool Renderer3D::CreateFramebuffers()
	{
		s_Data.DefaultFramebuffers.resize(Impl::State::Data->SwcImages.size(), VK_NULL_HANDLE);

		VkFramebufferCreateInfo frameInfo = {};
		frameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameInfo.width = Impl::State::Data->SwcData.CurrentExtent.width;
		frameInfo.height = Impl::State::Data->SwcData.CurrentExtent.height;
		frameInfo.renderPass = s_Data.DefaultPass;
		frameInfo.layers = 1;

		for (size_t i = 0; i < s_Data.DefaultFramebuffers.size(); i++)
		{
			std::vector<VkImageView> views = { Impl::State::Data->SwcMsaaTexture.ViewHandle, Impl::State::Data->SwcDepthTexture.ViewHandle, Impl::State::Data->SwcImages[i].ViewHandle };

			frameInfo.pAttachments = views.data();
			frameInfo.attachmentCount = views.size();

			if (vkCreateFramebuffer(Impl::State::Data->Device, &frameInfo, nullptr, &s_Data.DefaultFramebuffers[i]) != VK_SUCCESS)
			{
				VKP_ERROR("Unable to create framebuffer for Vulkan swapchain image view #{}", i);
				return false;
			}
		}

		return true;
	}

	bool Renderer3D::CreateBuffers()
	{
		bool success = Impl::CreateUniformBuffer(Impl::State::Data, &s_Data.GlobalUBO, sizeof(GlobalData));
		if (success) success = Impl::CreateStorageBuffer(Impl::State::Data, &s_Data.ObjectSSBO, 10000 * sizeof(ObjectData));
		if (success) success = Impl::CreateBuffer(Impl::State::Data, &s_Data.GlobalVBO, 15'000'000 * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
		if (success) success = Impl::CreateBuffer(Impl::State::Data, &s_Data.GlobalIBO, 5'000'000 * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

		return success;
	}

}