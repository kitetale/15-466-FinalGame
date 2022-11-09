#include "WormMode.hpp"

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

// ************************* MESH ******************************
GLuint worm_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > worm_meshes(LoadTagDefault, [](){
	// auto ret = new MeshBuffer(data_path("worm.pnct"));
    auto ret = new MeshBuffer(data_path("hvj-worm.pnct"));
    worm_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

// ************************ ANIMATION **************************
BoneAnimation::Animation const *worm_banim_crawl = nullptr;

Load< BoneAnimation > worm_banims(LoadTagDefault, [](){
	auto ret = new BoneAnimation(data_path("worm.banims"));
	worm_banim_crawl = &(ret->lookup("Crawl"));
	return ret;
});

Load< GLuint > worm_banims_for_bone_lit_color_texture_program(LoadTagDefault, [](){
	return new GLuint(worm_banims->make_vao_for_program(bone_lit_color_texture_program->program));
});

// ************************** SCENE ****************************
Load< Scene > worm_scene(LoadTagDefault, []() -> Scene const * {
	// return new Scene(data_path("worm.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
    return new Scene(data_path("hvj-worm.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = worm_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = worm_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

// *************************** WALKMESH ************************
WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > worm_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	// WalkMeshes *ret = new WalkMeshes(data_path("worm.w"));
    WalkMeshes *ret = new WalkMeshes(data_path("hvj-worm.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

// ************************ WORM MODE **************************
WormMode::WormMode() : scene(*worm_scene) {
    // MESH & WALKMESH SETUP ---------------------------------------------------
    {
        //create a player transform:
        scene.transforms.emplace_back();
        player.transform = &scene.transforms.back();

        //get character mesh
        for (auto &transform : scene.transforms) {
            if (transform.name == "catball") catball.ch_transform = &transform;
            if (transform.name == "rectangle") rectangle.ch_transform = &transform;
            if (transform.name == "goal") blob.ch_transform = &transform;
            // if (transform.name == "goal") goal = &transform;
            if (transform.name == "topGoal") topGoal = &transform;
            if (transform.name == "rectGoal") rectGoal = &transform;
        }

        //create a player camera attached to a child of the player transform:
        scene.transforms.emplace_back();
        scene.cameras.emplace_back(&scene.transforms.back());
        camera = &scene.cameras.back();
        camera->fovy = glm::radians(90.0f);
        camera->near = 0.01f;
        camera->transform->position = glm::vec3(0.0f, -4.0f, 7.0f);

        player.transform->position = glm::vec3(0.0f,0.0f,0.0f);
        //rotate camera facing direction (-z) to player facing direction (+y):
        camera->transform->rotation = glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // Default is cat 
        morph = 1;
        if (morph == 1) {
            player.transform->position = start_pos;
	        catball.ch_transform->position = start_pos;
            //start player walking at nearest walk point:
            player.at = walkmesh->nearest_walk_point(player.transform->position);
        }
    }

    // Worm animation setup ----------------------------------------------------
	{ 
        // Put some worms around the edge:
		Scene::Drawable::Pipeline worm_info;
		worm_info = bone_lit_color_texture_program_pipeline;

		worm_info.vao = *worm_banims_for_bone_lit_color_texture_program;
		worm_info.start = worm_banims->mesh.start;
		worm_info.count = worm_banims->mesh.count;

        // Add crawl animation to worm_animations list
		worm_animations.reserve(1);
        worm_animations.emplace_back(*worm_banims, *worm_banim_crawl, BoneAnimationPlayer::Loop, 0.0f);

        BoneAnimationPlayer *wormAnimation = &worm_animations.back();
    
        worm_info.set_uniforms = [wormAnimation](){
            wormAnimation->set_uniform(bone_lit_color_texture_program->BONES_mat4x3_array);
        };

        assert(worm_animations.size() == 1);

        scene.transforms.emplace_back();
        Scene::Transform *transform = &scene.transforms.back();
        transform->position.x = 0.0f;
        transform->position.y = 0.0f;
        transform->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        scene.drawables.emplace_back(transform);
        Scene::Drawable *worm1 = &scene.drawables.back();
        worm1->pipeline = worm_info;

        // Initialize worm
        this->worm.ch_animate = worm1;
        worm.ch_animate->transform->position =  glm::vec3(0.0f);
        worm.ctype = false; 
        worm.wstarting_rotation = worm.ch_animate->transform->rotation; 
	}

    // Insert all characters into game_characters
    {
        game_characters.insert(std::pair<int, Character>(0, worm)); 
        game_characters.insert(std::pair<int, Character>(1, catball));
        game_characters.insert(std::pair<int, Character>(2, rectangle));
        game_characters.insert(std::pair<int, Character>(3, blob));

        // Set all other characters offscreen
        for (auto &character : game_characters) {
            if (character.first != morph) {
                Character &ch = character.second;
                if (ch.ctype) {
                    ch.ch_transform->position = character_off_pos;
                } else {
                    ch.ch_animate->transform->position = character_off_pos;
                } 
            }
        }
    }

    // camera->transform->parent =  worm->transform;
    // camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // camera->transform->position = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::vec3(0.0f, -4.0f, 7.0f);

}

WormMode::~WormMode() {
}

bool WormMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	// Ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

    // Morphing - switching characters
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_1) {
        if (morph != 0) old_morph = morph;
		morph = 0;
		return true;
	}
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_2) {
        if (morph != 1) old_morph = morph;
		morph = 1;
		return true;
	}
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_3) {
        if (morph != 2) old_morph = morph;
        morph = 2; 
        return true; 
    }
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_4) {
        if (morph != 3) old_morph = morph;
        morph = 3; 
        return true; 
    }

    // movements
	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = true;
        if (morph == 0) {
            left = false;
            right = false;
        }
        // if (morph == 2) flipped = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = false;
		return true;
	}
	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = true;
        if (morph == 0) {
            left = false;
            right = false;
        }
        // if (morph == 2) flipped = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = false;
		return true;
	}
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
		left = true;
        // if (morph == 2) flipped = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
		left = false;
		return true;
	}
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
		right = true;
        // if (morph == 2) flipped = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
		right = false;
		return true;
	}
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
        if (morph == 3) {
            justFlipped = true;
            isFlipped = !isFlipped;
        }
        return true;
    }
	return false;
}

void WormMode::update(float elapsed) {
    // Change character if input provided 
    this->morphCharacter(false); 

    glm::vec3 move = glm::vec3(0.0f);
    if (morph == 0) {
        float step = 0.0f;
        float sideways = 0.0f;
        float speedForward = 1.0f;
        float speedSideways = 12.0f;

        if (forward) step += 1.0f;
        if (backward) step -= 1.0f;
        if (worm_animations[0].position <= 0.2f || worm_animations[0].position>=0.8f) {
            if (left) sideways -= 1.0f;
            if (right) sideways += 1.0f;
        }
        if (step != 0.0f) {
            step = step * speedForward * elapsed;
            sideways = 0.0f;
        }
        if (sideways != 0.0f) {
            sideways = sideways * speedSideways * elapsed;
            step = 0.0f;
        }

        move.x = sideways;
        move.y = step;
    }
    else if (morph == 1) {
        float PlayerSpeed = 4.0f;

        if (left && !right) move.x =-1.0f;
		if (!left && right) move.x = 1.0f;
		if (backward && !forward) move.y =-1.0f;
		if (!backward && forward) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec3(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;
    } else if (morph == 2 && flipped < 1) {
        if (left && !right) {
            move.x =-2.0f;
            rectangle.count += 1;
            if (rectangle.count % 7 == 1){
                if (game_characters[2].rtall_side) {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.707107f, 0.0f, 0.707107f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.707107f, 0.0f, 0.707107f);
                    move.z = -1.0f;
                    
                } else {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    move.z = 1.0f;
                }
            }
        }
		if (!left && right) {
            move.x = 2.0f;
            rectangle.count += 1;
            if (rectangle.count % 7 == 1){
                if (game_characters[2].rtall_side) {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.707107f, 0.0f, 0.707107f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.707107f, 0.0f, 0.707107f);
                    move.z = -1.0f;
                    
                } else {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    move.z = 1.0f;
                }
            }
        }
		if (backward && !forward) {
            move.y =-2.0f;
            rectangle.count += 1;
            if (rectangle.count % 7 == 1){
                if (game_characters[2].rtall_side) {
                    rectangle.ch_transform->rotation = glm::quat(0.5f, 0.5f, -0.5f, 0.5f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.5f, 0.5f, -0.5f, 0.5f);
                    move.z = -1.0f;
                    
                } else {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    move.z = 1.0f;
                }
            }
        }
		if (!backward && forward) {
            move.y = 2.0f;
            rectangle.count += 1;
            if (rectangle.count % 7 == 1){
                if (game_characters[2].rtall_side) {
                    rectangle.ch_transform->rotation = glm::quat(0.5f, 0.5f, -0.5f, 0.5f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.5f, 0.5f, -0.5f, 0.5f);
                    move.z = -1.0f;
                    
                } else {
                    rectangle.ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    game_characters[2].ch_transform->rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
                    move.z = 1.0f;
                }
            }
        }

        game_characters[2].rtall_side = !game_characters[2].rtall_side;
        // game_characters[2].game_characters[2].rtall_side = !game_characters[2].game_characters[2].rtall_side;
    } else if (morph == 3) {
        float PlayerSpeed = 12.0f;

        float dir = isFlipped ? -1.0f : 1.0f;

        if (left && !right) move.x =-1.0f * dir;
        if (!left && right) move.x = 1.0f * dir;
        if (backward && !forward) move.y =-1.0f;
        if (!backward && forward) move.y = 1.0f;

        if (move != glm::vec3(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;
    }


    if (morph == 1 || morph == 2) {
        //get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, move.z, 0.0f);
        //using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}
    }

    {
		// update character mesh's position to respect walking
        if (morph == 0) {
            // worm->transform->position.y += move.y;
            // worm->transform->position.x += move.x;

            // player.transform->position = worm->transform->position;

            // camera->transform->position = worm->transform->position + camera_offset_pos;
            // camera->transform->rotation = camera_offset_rot;

            // float angle = (move.x*60.0f);
            // worm->transform->rotation *= glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

            // if (!move.x && (worm->transform->rotation.w != -worm->transform->rotation.y)) {
            //     worm->transform->rotation = startingRotation;
            // }

            worm.ch_animate->transform->position.y += move.y;
            worm.ch_animate->transform->position.x += move.x;

            player.transform->position = worm.ch_animate->transform->position;

            camera->transform->position = worm.ch_animate->transform->position + camera_offset_pos;
            camera->transform->rotation = camera_offset_rot;

            float angle = (move.x*60.0f);
            worm.ch_animate->transform->rotation *= glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

            if (!move.x && (worm.ch_animate->transform->rotation.w != -worm.ch_animate->transform->rotation.y)) {
                worm.ch_animate->transform->rotation = worm.wstarting_rotation;

            }

            worm_animations[0].position += move.y *0.8f;
            worm_animations[0].position -= std::floor(worm_animations[0].position);

            for (auto &anim : worm_animations) {
                anim.update(elapsed);
            }
        }
        if (morph == 1) {
            catball.ch_transform->position.x += move.x;
            catball.ch_transform->position.y += move.y;            
            player.transform->position = catball.ch_transform->position;

            camera->transform->position = player.transform->position + camera_offset_pos;
            camera->transform->rotation = camera_offset_rot;

            // //update player's position to respect walking:
		    // player.transform->position = walkmesh->to_world_point(player.at);

            // catball.ch_transform->position = walkmesh->to_world_point(player.at);
            
            // camera->transform->position = player.transform->position + camera_offset_pos;
            // camera->transform->rotation = camera_offset_rot;
        }
        if (morph == 2) {
            move.x *= elapsed;
            move.y *= elapsed;
            rectangle.ch_transform->position.y += move.y;
            rectangle.ch_transform->position.x += move.x;
            
            player.transform->position = rectangle.ch_transform->position;

            camera->transform->position = player.transform->position + camera_offset_pos;
            camera->transform->rotation = camera_offset_rot;

            flipped += 1;
        }
        if (morph == 3) {
            blob.ch_transform->position.y += move.y;
            blob.ch_transform->position.x += move.x;

            if (justFlipped) {
                blob.ch_transform->position.z *= -1;
                blob.ch_transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                blob.ch_transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                
                camera_offset_pos.z *= -1;
                camera_offset_rot = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * camera_offset_rot;
                justFlipped = false;
            }

            player.transform->position = blob.ch_transform->position;

            camera->transform->position = blob.ch_transform->position + camera_offset_pos;
            camera->transform->rotation = camera_offset_rot;
        }
    }

    // worm.ch_animate = worm;
}

void WormMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//Draw scene:
    //set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

    // grey world background 
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

    // Walkmesh 
    {
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(camera->make_projection() * glm::mat4(camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
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
		lines.draw_text("Arrows moves; Space for specialty, NumKeys Morphs",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Arrows moves; Space for specialty, NumKeys Morphs",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	GL_ERRORS();
}

void WormMode::morphCharacter(bool forced) {
    if (old_morph != morph || forced) { 
        // Get position of previous active character
        glm::vec3 pos; 
        Character old_ch = game_characters[old_morph];
        Character new_ch = game_characters[morph];
        if (old_ch.ctype) {
            pos = old_ch.ch_transform->position; 
        } else {
            pos = old_ch.ch_animate->transform->position; 
        }
        
        // Update position of new character
        if (new_ch.ctype) {
            new_ch.ch_transform->position = pos; 
        } else { 
            new_ch.ch_animate->transform->position = pos; 
        }

        // Move all morphs (characters) offscreen
        for (auto &character : game_characters) {
            int ch_num = character.first;
            Character &ch = character.second; 
            if (ch_num != morph) {
                if (ch.ctype) {
                    ch.ch_transform->position = character_off_pos;
                } else {
                    ch.ch_animate->transform->position = character_off_pos;
                } 
            }
        }
        old_morph = morph;
    }
}