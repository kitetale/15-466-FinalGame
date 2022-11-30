#include "SplashScreenMode.hpp"

#include "WormMode.hpp"
// #include "TutorialMode.hpp"
#include "LitColorTextureProgram.hpp"
#include "BoneLitColorTextureProgram.hpp"
#include "DrawLines.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <unordered_map>

#define FONT "Roboto-Medium.ttf"

SplashScreenMode::SplashScreenMode() {
}

SplashScreenMode::~SplashScreenMode() {
}

bool SplashScreenMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_1) {
        tutorial_text_color = highlight_text_color;
        level = 0;
        return true;
    }
    else if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_2) {
	    game_text_color = highlight_text_color;
        level = 1;
        return true;
    }
	return false;
}

void SplashScreenMode::update(float elapsed) {
    if (time_elapsed > 1) {
        time_elapsed = 1;
    } else {
        time_elapsed += elapsed;
    }

    if (level >= 0) {
        press_elapsed += elapsed;
        if (press_elapsed > 1) {
            if (level == 0) {
                // Mode::set_current(std::make_shared< TutorialMode >());
                Mode::set_current(std::make_shared< SplashScreenMode >());
            }
            else if (level == 1) {
                Mode::set_current(std::make_shared< WormMode >());
            }
        }
    }
}

void SplashScreenMode::draw(glm::uvec2 const &drawable_size) {

    // grey world background 
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

    
    
    {
        main_text_renderer->set_drawable_size(drawable_size);
        glm::uvec2 center = glm::uvec2(drawable_size.x / 2, drawable_size.y / 2);
        float size_ratio = drawable_size.y / 1200.0f;

        main_text_renderer->renderText("Morphology Mania", center.x - 775.0f * size_ratio, center.y + 100.0f * size_ratio, 0.8f * size_ratio, highlight_text_color);
        main_text_renderer->renderText("1: Tutorial", center.x - 650.0f * size_ratio, center.y - 100.0f * size_ratio, main_text_size * size_ratio, lerp(main_text_color, tutorial_text_color, press_elapsed));
        main_text_renderer->renderText("2: Game", center.x + 200.0f * size_ratio, center.y - 100.0f * size_ratio, main_text_size * size_ratio, lerp(main_text_color, game_text_color, press_elapsed));
    }


    { //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
 
		constexpr float H = 0.09f;
		lines.draw_text("SplashScreenMode",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("SplashScreenMode",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	GL_ERRORS();
}