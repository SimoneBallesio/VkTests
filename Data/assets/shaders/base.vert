#version 450 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

layout (set = 0, binding = 0) uniform ubo
{
	mat4 VP;
} UBO;

layout (location = 0)
out vs_out
{
	vec2 TexCoord;
	vec3 Color;
} Out;

void main()
{
	gl_Position = UBO.VP * vec4(aPosition, 1.0);
	Out.TexCoord = aTexCoord;
	Out.Color = aColor;
}