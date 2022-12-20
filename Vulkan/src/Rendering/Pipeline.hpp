#pragma once

#include "Rendering/Shader.hpp"

#include <vulkan/vulkan.h>

namespace VKP
{

	// TODO: A "Material template" has one pipeline data struct per pass type
	struct Pipeline
	{
		VkPipeline Pipe = VK_NULL_HANDLE;
		VkPipelineLayout PipeLayout = VK_NULL_HANDLE;
	};

	struct GraphicsPipelineFactory
	{
		VkPipelineRasterizationStateCreateInfo RasterInfo = {};
		VkPipelineColorBlendAttachmentState BlendState = {};
		VkPipelineDepthStencilStateCreateInfo DepthInfo = {};

		GraphicsPipelineFactory();
		VkPipeline Build(VkRenderPass pass, ShaderEffect* effect);
	};

}