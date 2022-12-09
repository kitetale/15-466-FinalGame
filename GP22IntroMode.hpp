#pragma once

/*
 * GP22 intro animation / splash screen.
 *
 */

#include "Mode.hpp"
#include "Sound.hpp"

#include "GL.hpp"

#include <memory>

struct GP22IntroMode : Mode {
	//pass a pointer to the mode that will be invoked when this one over / skipped:
	GP22IntroMode(std::shared_ptr< Mode > const &next_mode);
	virtual ~GP22IntroMode();

	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//will Mode::set_current(next_mode) when done animating:
	std::shared_ptr< Mode > next_mode;

	//will start playing music on launch, will silence music on quit:
	std::shared_ptr< Sound::PlayingSample > music;

	//will draw a fancy set of lines with dynamically generated vertices:
	GLuint color_program = 0;
	GLuint OBJECT_TO_CLIP_mat4 = -1U;

	GLuint vertex_buffer = 0;
	GLuint vertex_buffer_for_color_program = 0;

	struct Vertex {
		Vertex(glm::vec2 const &Position_, glm::u8vec4 Color_) : Position(Position_), Color(Color_) { }
		glm::vec2 Position;
		glm::u8vec4 Color;
	};

	struct LetterPath {
		LetterPath();
		float ofs_init, ofs_K;
		std::vector< glm::vec2 > base;
		void compute(std::vector< Vertex > *attribs, float time) const;
		float strike = 5.0f;
	};

	std::vector< glm::vec2 > blob;
	std::vector< LetterPath > letters;
	std::vector< std::vector< glm::vec2 > > middle;

	//animation is timed based on this accumulator:
	float time = 0.0f;
};