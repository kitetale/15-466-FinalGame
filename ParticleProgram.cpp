#include "ParticleProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< ParticleProgram > particle_program(LoadTagEarly);

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
