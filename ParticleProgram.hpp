#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, colored vertices:
struct ParticleProgram {
	ParticleProgram();
	~ParticleProgram();

	GLuint program = 0;
    //Attribute (per-vertex variable):
    // none
	//Uniform (per-invocation variable) locations:
	GLuint PROJECTION_mat4 = -1U;
	GLuint OFFSET_vec2 = -1U;
	GLuint COLOR_vec4 = -1U;
	//Textures:
	// none
};

extern Load< ParticleProgram > particle_program;

extern Scene::Drawable::Pipeline particle_texture_program_pipeline;
