#version 450 core

layout (location = 0)
in vs_out
{
	vec2 TexCoord;
	vec3 Color;
} In;

layout (set = 0, binding = 0) uniform ubo
{
	mat4 VP;
} UBO;

layout (set = 1, binding = 0) uniform sampler2D diffuse;

layout (location = 0) out vec4 Color;

void main()
{
	Color = vec4(In.Color * texture(diffuse, In.TexCoord).rgb, 1.0);
}