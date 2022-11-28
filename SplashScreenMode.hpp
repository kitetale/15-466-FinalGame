#pragma once

#include "Mode.hpp"

#include "BoneAnimation.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "WalkMesh.hpp"

#include "data_path.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <vector>
#include <map> 
#include <deque>
#include <list>

#include "TextRendering.hpp"

struct SplashScreenMode : public Mode {
	SplashScreenMode();
	virtual ~SplashScreenMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;


	// TextRenderer *roboto_renderer = new TextRenderer(data_path("fonts/Roboto-Medium.ttf"), 54);
	// TextRenderer *patua_renderer = new TextRenderer(data_path("fonts/PatuaOne-Regular.ttf"), 54);
	TextRenderer *rubik_renderer = new TextRenderer(data_path("fonts/RubikDirt-Regular.ttf"), 200);
	TextRenderer *main_text_renderer = rubik_renderer;

	float main_text_size = 0.5f;
    glm::vec3 main_text_color = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::vec3 highlight_text_color = glm::vec3(1.0f, 1.0f, 1.0f);

	glm::vec3 tutorial_text_color = main_text_color;
	glm::vec3 game_text_color = main_text_color;

	float time_elapsed;
	//lerp copied from https://graphicscompendium.com/opengl/22-interpolation
    static glm::vec3 lerp(glm::vec3 x, glm::vec3 y, float t) {
        return x * (1.f - t) + y * t;
    }




    int level = -1;
	float press_elapsed = 0.0f;

};