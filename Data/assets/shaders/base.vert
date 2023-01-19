#version 450 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

layout (set = 0, binding = 0) uniform scene_data
{
	mat4 VP;
} Scene;

struct ObjectData
{
	mat4 Model;
};

layout (std140, set = 1, binding = 0) readonly buffer obj_data
{
	ObjectData Matrices[];
} Objects;

layout (location = 0)
out vs_out
{
	vec2 TexCoord;
	vec3 Color;
} Out;

void main()
{
	gl_Position = Scene.VP * Objects.Matrices[gl_InstanceIndex].Model * vec4(aPosition, 1.0);
	Out.TexCoord = aTexCoord;
	Out.Color = aColor;
}