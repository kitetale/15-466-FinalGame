#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, lit, textured vertices tinted with vertex colors and animated by ~skeletal animation~:
struct BoneLitColorTextureProgram {
	BoneLitColorTextureProgram();
	~BoneLitColorTextureProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;
	GLuint BoneWeights_vec4 = -1U;
	GLuint BoneIndices_uvec4 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint OBJECT_TO_CLIP_mat4 = -1U;
	GLuint OBJECT_TO_LIGHT_mat4x3 = -1U;
	GLuint NORMAL_TO_LIGHT_mat3 = -1U;

	GLuint BONES_mat4x3_array = -1U;

	enum : uint32_t {
		MaxBones = 40
	};

	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord
};

extern Load< BoneLitColorTextureProgram > bone_lit_color_texture_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
extern Scene::Drawable::Pipeline bone_lit_color_texture_program_pipeline;