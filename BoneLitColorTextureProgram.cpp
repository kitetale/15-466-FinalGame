#include "BoneLitColorTextureProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline bone_lit_color_texture_program_pipeline;

Load< BoneLitColorTextureProgram > bone_lit_color_texture_program(LoadTagEarly, []() -> BoneLitColorTextureProgram const * {
	BoneLitColorTextureProgram *ret = new BoneLitColorTextureProgram();

	//----- build the pipeline template -----
	bone_lit_color_texture_program_pipeline.program = ret->program;

	bone_lit_color_texture_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;
	bone_lit_color_texture_program_pipeline.OBJECT_TO_LIGHT_mat4x3 = ret->OBJECT_TO_LIGHT_mat4x3;
	bone_lit_color_texture_program_pipeline.NORMAL_TO_LIGHT_mat3 = ret->NORMAL_TO_LIGHT_mat3;

	//make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);


	bone_lit_color_texture_program_pipeline.textures[0].texture = tex;
	bone_lit_color_texture_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

BoneLitColorTextureProgram::BoneLitColorTextureProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform mat4x3 OBJECT_TO_LIGHT;\n"
		"uniform mat3 NORMAL_TO_LIGHT;\n"
		"uniform mat4x3 BONES[" + std::to_string(MaxBones) + "];\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"in vec4 BoneWeights;\n"
		"in uvec4 BoneIndices;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
//Considering (just) the Add/Mul counts:
/*  Variation (1): mul = 4*(12+3) = 60,  add = 4*9 + 3*3 = 45
		"	vec3 blended_Position = (\n"
		"		(BONES[BoneIndices.x] * Position) * BoneWeights.x\n"
		"		+ (BONES[BoneIndices.y] * Position) * BoneWeights.y\n"
		"		+ (BONES[BoneIndices.z] * Position) * BoneWeights.z\n"
		"		+ (BONES[BoneIndices.w] * Position) * BoneWeights.w\n"
		"		);\n"
*/
/*  Variation (2): mul = 4*12 + 12 = 60,  add = 3*12 + 9 = 45
		"	vec3 blended_Position = (\n"
		"		(BONES[BoneIndices.x] * BoneWeights.x\n"
		"		+ BONES[BoneIndices.y] * BoneWeights.y\n"
		"		+ BONES[BoneIndices.z] * BoneWeights.z\n"
		"		+ BONES[BoneIndices.w] * BoneWeights.w\n"
		"		) * Position;\n"
*/
		"	vec3 blended_Position = (\n"
		"		(BONES[BoneIndices.x] * Position) * BoneWeights.x\n"
		"		+ (BONES[BoneIndices.y] * Position) * BoneWeights.y\n"
		"		+ (BONES[BoneIndices.z] * Position) * BoneWeights.z\n"
		"		+ (BONES[BoneIndices.w] * Position) * BoneWeights.w\n"
		"		);\n"
		"	vec3 blended_Normal = (\n"
		"		mat3(BONES[BoneIndices.x]) * Normal * BoneWeights.x\n"
		"		+ mat3(BONES[BoneIndices.y]) * Normal * BoneWeights.y\n"
		"		+ mat3(BONES[BoneIndices.z]) * Normal * BoneWeights.z\n"
		"		+ mat3(BONES[BoneIndices.w]) * Normal * BoneWeights.w\n"
		"		);\n"
		"	gl_Position = OBJECT_TO_CLIP * vec4(blended_Position, 1.0);\n"
		"	position = OBJECT_TO_LIGHT * vec4(blended_Position, 1.0);\n"
		"	normal = NORMAL_TO_LIGHT * blended_Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 n = normalize(normal);\n"
		"	vec3 l = normalize(vec3(0.1, 0.1, 1.0));\n"
		"	vec4 albedo = texture(TEX, texCoord) * color;\n"
		//simple hemispherical lighting model:
		"	vec3 light = mix(vec3(0.0,0.0,0.1), vec3(1.0,1.0,0.95), dot(n,l)*0.5+0.5);\n"
		"	fragColor = vec4(light*albedo.rgb, albedo.a);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");
	BoneWeights_vec4 = glGetAttribLocation(program, "BoneWeights");
	BoneIndices_uvec4 = glGetAttribLocation(program, "BoneIndices");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OBJECT_TO_LIGHT_mat4x3 = glGetUniformLocation(program, "OBJECT_TO_LIGHT");
	NORMAL_TO_LIGHT_mat3 = glGetUniformLocation(program, "NORMAL_TO_LIGHT");
	BONES_mat4x3_array = glGetUniformLocation(program, "BONES");

	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

BoneLitColorTextureProgram::~BoneLitColorTextureProgram() {
	glDeleteProgram(program);
	program = 0;
}
