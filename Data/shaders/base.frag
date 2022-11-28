#version 450 core

layout (location = 0)
in vs_out
{
	vec3 Color;
} In;

layout (location = 0) out vec4 Color;

void main()
{
	Color = vec4(In.Color, 1.0);
}