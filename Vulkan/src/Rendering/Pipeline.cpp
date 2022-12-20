#include "Pch.hpp"

#include "Core/Definitions.hpp"

#include "Rendering/Context.hpp"
#include "Rendering/Pipeline.hpp"
#include "Rendering/VertexData.hpp"

namespace VKP
{

	GraphicsPipelineFactory::GraphicsPipelineFactory()
	{
		RasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		RasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
		RasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		RasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		RasterInfo.rasterizerDiscardEnable = VK_FALSE;
		RasterInfo.depthBiasEnable = VK_FALSE;
		RasterInfo.lineWidth = 1.0f;

		BlendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		BlendState.blendEnable = VK_FALSE;

		DepthInfo = {};
		DepthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		DepthInfo.depthTestEnable = VK_TRUE;
		DepthInfo.depthWriteEnable = VK_TRUE;
		DepthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		DepthInfo.depthBoundsTestEnable = VK_FALSE;
		DepthInfo.stencilTestEnable = VK_FALSE;
	}

	VkPipeline GraphicsPipelineFactory::Build(VkRenderPass pass, ShaderEffect* effect)
	{
		std::vector<VkPipelineShaderStageCreateInfo> stageInfos = {};
		stageInfos.reserve(effect->m_Stages.size());

		for (size_t i = 0; i < effect->m_Stages.size(); i++)
		{
			const auto& s = effect->m_Stages[i];
			auto& stageInfo = stageInfos.emplace_back();
			stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo.stage = s.Stage;
			stageInfo.module = s.Module->ModuleHandle;
			stageInfo.pName = "main";
		}
		
		const std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynInfo = {};
		dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynInfo.pDynamicStates = dynamicStates.data();
		dynInfo.dynamicStateCount = dynamicStates.size();

		std::vector<VkVertexInputBindingDescription> vertBind = {};
		std::vector<VkVertexInputAttributeDescription> descriptions = {};

		Vertex::PopulateBindingDescription(vertBind, descriptions);

		VkPipelineVertexInputStateCreateInfo vertInfo = {};
		vertInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertInfo.pVertexBindingDescriptions = vertBind.data();
		vertInfo.vertexBindingDescriptionCount = vertBind.size();
		vertInfo.vertexAttributeDescriptionCount = descriptions.size();
		vertInfo.pVertexAttributeDescriptions = descriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputInfo = {};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportInfo = {};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.scissorCount = 1;

		VkPipelineMultisampleStateCreateInfo sampleInfo = {};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = Context::Get().GetMsaaMaxSamples();

		VkPipelineColorBlendStateCreateInfo blendInfo = {};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.logicOpEnable = VK_FALSE;
		blendInfo.pAttachments = &BlendState;
		blendInfo.attachmentCount = 1;

		VkGraphicsPipelineCreateInfo pipeInfo = {};
		pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeInfo.pStages = stageInfos.data();
		pipeInfo.stageCount = stageInfos.size();
		pipeInfo.renderPass = pass;
		pipeInfo.subpass = 0;
		pipeInfo.layout = effect->m_PipeLayout;
		pipeInfo.pDynamicState = &dynInfo;
		pipeInfo.pVertexInputState = &vertInfo;
		pipeInfo.pInputAssemblyState = &inputInfo;
		pipeInfo.pViewportState = &viewportInfo;
		pipeInfo.pRasterizationState = &RasterInfo;
		pipeInfo.pMultisampleState = &sampleInfo;
		pipeInfo.pColorBlendState = &blendInfo;
		pipeInfo.pDepthStencilState = &DepthInfo;

		VkPipeline pipe = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(Context::GetDevice(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipe) != VK_SUCCESS)
		{
			VKP_ERROR("Unable to create pipeline");
			return VK_NULL_HANDLE;
		}

		return pipe;
	}

}