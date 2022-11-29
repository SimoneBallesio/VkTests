#version 450 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

layout (set = 0, binding = 0) uniform ubo
{
	mat4 M;
} UBO;

layout (push_constant) uniform constants
{
	mat4 VP;
} PushConstants;

layout (location = 0)
out vs_out
{
	vec2 TexCoord;
	vec3 Color;
} Out;

void main()
{
	gl_Position = PushConstants.VP * UBO.M * vec4(aPosition, 0.0, 1.0);
	Out.TexCoord = aTexCoord;
	Out.Color = aColor;
}