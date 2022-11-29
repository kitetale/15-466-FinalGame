#include "ParticleProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline particle_texture_program_pipeline;

Load< Particle Program > particle_texture_program(LoadTagEarly, []() -> LitColorTextureProgram const * {
	ParticleProgram *ret = new ParticleProgram();

	//----- build the pipeline template -----
	particle_texture_program_pipeline.program = ret->program;

	particle_texture_program_pipeline.PROJECTION_mat4 = ret->PROJECTION_mat4;
	particle_texture_program_pipeline.OFFSET_vec2 = ret->OFFSET_vec2;
	particle_texture_program_pipeline.COLOR_vec4 = ret->COLOR_vec4;

	/* This will be used later if/when we build a light loop into the Scene:
	particle_texture_program_pipeline.LIGHT_TYPE_int = ret->LIGHT_TYPE_int;
	particle_texture_program_pipeline.LIGHT_LOCATION_vec3 = ret->LIGHT_LOCATION_vec3;
	particle_texture_program_pipeline.LIGHT_DIRECTION_vec3 = ret->LIGHT_DIRECTION_vec3;
	particle_texture_program_pipeline.LIGHT_ENERGY_vec3 = ret->LIGHT_ENERGY_vec3;
	particle_texture_program_pipeline.LIGHT_CUTOFF_float = ret->LIGHT_CUTOFF_float;
	*/

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


	particle_texture_program_pipeline.textures[0].texture = tex;
	particle_texture_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

ParticleProgram::ParticleProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>\n"
		"out vec2 TexCoords;\n"
		"out vec4 ParticleColor;\n"
		"uniform mat4 projection;\n"
        "uniform vec2 offset;\n"
        "uniform vec4 color;\n"
		"void main() {\n"
		"	float scale = 10.0f;\n"
        "   TexCoords = vertex.zw;\n"
        "   ParticleColor = color;\n"
        "   gl_Position = projection * vec4((vertex.xy * scale) + offset, 0.0, 1.0);\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"in vec2 TexCoords;\n"
		"in vec4 ParticleColor;\n"
        "out vec4 color;\n"
        "uniform sampler2D sprite;\n"
		"void main() {\n"
		"	color = (texture(sprite, TexCoords) * ParticleColor);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of uniforms:
	PROJECTION_mat4 = glGetUniformLocation(program, "projection");
	OFFSET_vec2 = glGetUniformLocation(program, "offset");
	COLOR_vec4 = glGetUniformLocation(program, "color");
    GLuint SPRITE_sampler2D = glGetUniformLocation(program, "sprite");

    glUseProgram(program); //bind program -- glUniform* calls refer to this program now
    glUniform1i(SPRITE_sampler2D, 0); //set TEX to sample from GL_TEXTURE0
    glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

ParticleProgram::~ParticleProgram() {
	glDeleteProgram(program);
	program = 0;
}
