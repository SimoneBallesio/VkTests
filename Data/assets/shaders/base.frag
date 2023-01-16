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
	vec4 color = texture(diffuse, In.TexCoord).rgba;

	if (color.a < 0.5) discard;

	Color = vec4(color.rgb, 1.0);
}