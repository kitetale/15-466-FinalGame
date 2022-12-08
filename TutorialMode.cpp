#include "TutorialMode.hpp"

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

#include <sstream>
#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <unordered_map>

// ************************* MESH ******************************
GLuint tutorial_worm_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > tutorial_worm_meshes(LoadTagDefault, [](){
	// auto ret = new MeshBuffer(data_path("worm.pnct"));
    auto ret = new MeshBuffer(data_path("level.pnct"));
    tutorial_worm_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

// ************************ ANIMATION **************************
BoneAnimation::Animation const *tutorial_worm_banim_crawl = nullptr;
Load< BoneAnimation > tutorial_worm_banims(LoadTagDefault, [](){
	auto ret = new BoneAnimation(data_path("level.banims"));
	tutorial_worm_banim_crawl = &(ret->lookup("Crawl"));
	return ret;
});

BoneAnimation::Animation const *tutorial_rect_banim_moveY = nullptr;
Load< BoneAnimation > tutorial_rect_banims(LoadTagDefault, [](){
	auto ret = new BoneAnimation(data_path("rect.banims"));
	tutorial_rect_banim_moveY = &(ret->lookup("MoveY"));
	return ret;
});

BoneAnimation::Animation const *tutorial_blob_banim_walk = nullptr;
BoneAnimation::Animation const *tutorial_blob_banim_flip = nullptr;
Load< BoneAnimation > tutorial_blob_banims(LoadTagDefault, [](){
	auto ret = new BoneAnimation(data_path("blob.banims"));
	tutorial_blob_banim_walk = &(ret->lookup("Walk"));
	tutorial_blob_banim_flip = &(ret->lookup("Flip"));
	return ret;
});

Load< GLuint > tutorial_worm_banims_for_bone_lit_color_texture_program(LoadTagDefault, [](){
	return new GLuint(tutorial_worm_banims->make_vao_for_program(bone_lit_color_texture_program->program));
});

Load< GLuint > tutorial_rect_banims_for_bone_lit_color_texture_program(LoadTagDefault, [](){
	return new GLuint(tutorial_rect_banims->make_vao_for_program(bone_lit_color_texture_program->program));
});

Load< GLuint > tutorial_blob_banims_for_bone_lit_color_texture_program(LoadTagDefault, [](){
	return new GLuint(tutorial_blob_banims->make_vao_for_program(bone_lit_color_texture_program->program));
});

// ************************** SCENE ****************************
Load< Scene > tutorial_worm_scene(LoadTagDefault, []() -> Scene const * {
	// return new Scene(data_path("worm.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
    return new Scene(data_path("level.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = tutorial_worm_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = tutorial_worm_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

// *************************** WALK MESH ***********************
WalkMesh const *tutorial_walkmesh = nullptr;
Load< WalkMeshes > tutorial_worm_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	// WalkMeshes *ret = new WalkMeshes(data_path("worm.w"));
    WalkMeshes *ret = new WalkMeshes(data_path("level.w"));
	tutorial_walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

// ************************ WORM MODE **************************
TutorialMode::TutorialMode() : scene(*tutorial_worm_scene) {
    // MESH & WALKMESH SETUP ---------------------------------------------------
    {
        //create a player transform:
        scene.transforms.emplace_back();
        player.transform = &scene.transforms.back();

        //get character mesh
        for (auto &transform : scene.transforms) {
            if (transform.name == "Catball") catball.ch_transform = &transform;
            if (transform.name == "Rectangle") rectangle.ch_transform = &transform;
            //if (transform.name == "Blob") blob.ch_transform = &transform;
            if (transform.name.substr(0, transform.name.size()-1) == "bead") {
                beads.push_back(&transform);
            }
            // if (transform.name.substr(0, 4) == "")
        }

        // Bead count 
        num_beads = beads.size();

        //create a player camera attached to a child of the player transform:
        scene.transforms.emplace_back();
        scene.cameras.emplace_back(&scene.transforms.back());
        camera = &scene.cameras.back();
        camera->fovy = glm::radians(60.0f);
        camera->near = 0.01f;
        camera->transform->position = camera_offset_pos;

        player.transform->position = start_pos;
        //rotate camera facing direction (-z) to player facing direction (+y):
        camera->transform->rotation = camera_offset_rot;

        cam_init_rot = camera->transform->rotation;
        start_rot = player.transform->rotation;

    }

    // Worm animation setup ----------------------------------------------------
	{ 
		Scene::Drawable::Pipeline worm_info;
		worm_info = bone_lit_color_texture_program_pipeline;

		worm_info.vao = *tutorial_worm_banims_for_bone_lit_color_texture_program;
		worm_info.start = tutorial_worm_banims->mesh.start;
		worm_info.count = tutorial_worm_banims->mesh.count;

        // Add crawl animation to worm_animations list
		worm_animations.reserve(1);
        worm_animations.emplace_back(*tutorial_worm_banims, *tutorial_worm_banim_crawl, BoneAnimationPlayer::Loop, 0.0f);

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

    // Rect animation setup ----------------------------------------------------
	{ 
		Scene::Drawable::Pipeline rect_info;
		rect_info = bone_lit_color_texture_program_pipeline;

		rect_info.vao = *tutorial_rect_banims_for_bone_lit_color_texture_program;
		rect_info.start = tutorial_rect_banims->mesh.start;
		rect_info.count = tutorial_rect_banims->mesh.count;

        // Add move y animation to rect_animations list
		rect_animations.reserve(1);
        rect_animations.emplace_back(*tutorial_rect_banims, *tutorial_rect_banim_moveY, BoneAnimationPlayer::Loop, 0.0f);

        BoneAnimationPlayer *rectAnimation = &rect_animations.back();
    
        rect_info.set_uniforms = [rectAnimation](){
            rectAnimation->set_uniform(bone_lit_color_texture_program->BONES_mat4x3_array);
        };

        assert(rect_animations.size() == 1);

        scene.transforms.emplace_back();
        Scene::Transform *transform = &scene.transforms.back();
        transform->position.x = 0.0f;
        transform->position.y = 0.0f;
        transform->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        scene.drawables.emplace_back(transform);
        Scene::Drawable *rect1 = &scene.drawables.back();
        rect1->pipeline = rect_info;

        // Initialize rectangle
        this->rectangle.ch_animate = rect1;
        rectangle.ch_animate->transform->position =  glm::vec3(0.0f);
        rectangle.ctype = false;
        rectangle.wstarting_rotation = rectangle.ch_animate->transform->rotation;  
	}

    // Blob animation setup ----------------------------------------------------
	{ 
		Scene::Drawable::Pipeline blob_info;
		blob_info = bone_lit_color_texture_program_pipeline;

		blob_info.vao = *tutorial_blob_banims_for_bone_lit_color_texture_program;
		blob_info.start = tutorial_blob_banims->mesh.start;
		blob_info.count = tutorial_blob_banims->mesh.count;

        // Add crawl animation to worm_animations list
		blob_animations.reserve(1);
        blob_animations.emplace_back(*tutorial_blob_banims, *tutorial_blob_banim_walk, BoneAnimationPlayer::Loop, 0.0f);
        //blob_animations.emplace_back(*tutorial_blob_banims, *tutorial_blob_banim_flip, BoneAnimationPlayer::Loop, 0.0f);

        BoneAnimationPlayer *blobAnimation = &blob_animations.back();
    
        blob_info.set_uniforms = [blobAnimation](){
            blobAnimation->set_uniform(bone_lit_color_texture_program->BONES_mat4x3_array);
        };

        assert(blob_animations.size() == 1);

        scene.transforms.emplace_back();
        Scene::Transform *transform = &scene.transforms.back();
        transform->position.x = 0.0f;
        transform->position.y = 0.0f;
        transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        scene.drawables.emplace_back(transform);
        Scene::Drawable *blob1 = &scene.drawables.back();
        blob1->pipeline = blob_info;

        // Initialize worm
        this->blob.ch_animate = blob1;
        blob.ch_animate->transform->position =  glm::vec3(0.0f);
        blob.ctype = false;  
	}

    // Insert all characters into game_characters
    {
        game_characters.insert(std::pair<int, Character>(0, worm)); 
        game_characters.insert(std::pair<int, Character>(1, catball));
        game_characters.insert(std::pair<int, Character>(2, rectangle));
        game_characters.insert(std::pair<int, Character>(3, blob));

        // Default is cat 
        morph = 1;
        if (morph == 1) {
	        worm.ch_animate->transform->position = start_pos;
            //start player walking at nearest walk point:
            player.at = tutorial_walkmesh->nearest_walk_point(player.transform->position);
        }

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
        //rectangle.ch_transform->position = character_off_pos;
    }

}

TutorialMode::~TutorialMode() {
}

bool TutorialMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	// Ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

    // Morphing - switching characters
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_1) {
        if (stage == 1 && time_elapsed > 16) {
            if (morph != 0) old_morph = morph;
            morph = 0;
            stage += 1;
            time_elapsed = 0.0f;
        }
		return true;
	}
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_2) {
        if (stage == 2 && time_elapsed > 14) {
            if (morph != 1) old_morph = morph;
            morph = 1;
            stage += 1;
            time_elapsed = 0.0f;
        }
		return true;
	}
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_3) {
        if (stage == 3 && time_elapsed > 16) {
            if (morph != 2) old_morph = morph;
            morph = 2;
            stage += 1;
            time_elapsed = 0.0f;
        }
        return true; 
    }
    if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_4) {
        if (stage == 4 && time_elapsed > 10) {
            if (morph != 3) old_morph = morph;
            morph = 3;
            stage += 1;
            time_elapsed = 0.0f;
        }
        return true;
    }

    // Movements
    if (stage > 2 || (stage == 2 && time_elapsed > 3)) {
        if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_w) {
            forward = true;
            if (morph == 0) {
                left = false;
                right = false;
            }
            // if (morph == 2) flipped = true;
            return true;
        }
        if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_w) {
            forward = false;
            return true;
        }
        if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_s) {
            backward = true;
            if (morph == 0) {
                left = false;
                right = false;
            }
            // if (morph == 2) flipped = true;
            return true;
        }
        if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_s) {
            backward = false;
            return true;
        }
        if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_a) {
            left = true;
            // if (morph == 2) flipped = true;
            return true;
        }
        if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_a) {
            left = false;
            return true;
        }
        if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_d) {
            right = true;
            // if (morph == 2) flipped = true;
            return true;
        }
        if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_d) {
            right = false;
            return true;
        }
    }
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_SPACE) {
        if (stage == 5 && time_elapsed > 3) {
            if (morph == 3) {
                justFlipped = true;
                isFlipped = !isFlipped;
            }
            stage += 1;
            time_elapsed = 8.0f;
        }
        return true;
    }
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_RETURN) { // to reset to initial position
        if (stage == 0 && time_elapsed > 5) {
            stage += 1;
            time_elapsed = 0.0f;
        } else if (stage == 7 || (stage == 6 && time_elapsed > 16)) {
            Mode::set_current(std::make_shared< WormMode >());
        } else {
            stage = 7;
            time_elapsed = 15.0f;
        }
        return true;
    }
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
        if (stage == 1 && time_elapsed > 10) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        return true;
    } else if (evt.type == SDL_MOUSEBUTTONDOWN) { // camera move
		if (stage == 1 && time_elapsed > 3) {
            if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                return true;
		    }
        }
	} else if (evt.type == SDL_MOUSEMOTION) {
        if (morph==0||morph==2||morph==3)return true; //no mouse move for worm
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
            glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
            // TODO : check axis the mesh and camera is rotating by
			glm::vec3 upDir = tutorial_walkmesh->to_world_triangle_normal(player.at);
			player.transform->rotation = glm::angleAxis(motion.x * camera->fovy, upDir) * player.transform->rotation;
            float pitch = glm::pitch(camera->transform->rotation);
			pitch += motion.y * camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			camera->transform->rotation = glm::angleAxis(pitch, upDir);


            for (auto &character : game_characters) {
                if (character.first == morph) {
                    Character &ch = character.second;
                    // store angle/ direction facing for character and reconstruct 
                    ch.cangle += motion.x * camera->fovy;
                    //std::cout<<"("<<upDir.x<<", "<<upDir.y<<", "<<upDir.z<<")"<<std::endl;
                    ch.ch_transform->rotation = glm::angleAxis(ch.cangle, upDir);
                }
            }
			return true;
		}
    }
    return false;
}

void TutorialMode::update(float elapsed) {
    time_elapsed += elapsed;
    
    // Change character if input provided 
    this->morphCharacter(false); 

    // Compute movement of chararacter
    glm::vec3 move = glm::vec3(0.0f);
    {
        if (morph == 0) {
            float step = 0.0f;
            float sideways = 0.0f;
            float speedForward = 2.5f;
            float speedSideways = 12.0f;
            float dir = isFlipped ? -1.0f : 1.0f;

            if (forward) step += 1.0f;
            if (backward) step -= 1.0f;
            if (worm_animations[0].position <= 0.2f || worm_animations[0].position>=0.8f) {
                if (left) sideways -= 1.0f*dir;
                if (right) sideways += 1.0f*dir;
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
            float PlayerSpeed = 4.0f * accel;
            float dir = isFlipped ? -1.0f : 1.0f;
            if (jumpDir == 1.0f) {
                accel *= 0.98f;
            } else {
                accel *= 1.05f;
            }
            moveZ = jumpDir * PlayerSpeed * elapsed;

            if (left && !right) move.x =-1.0f*dir;
            if (!left && right) move.x = 1.0f*dir;
            if (backward && !forward) move.y =-1.0f;
            if (!backward && forward) move.y = 1.0f;
            // Make it so that moving diagonally doesn't go faster:
            if (move != glm::vec3(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;
        } else if (morph == 2) {
            float dir = isFlipped ? -1.0f : 1.0f;
            float PlayerSpeed = 10.0f;

            if (left && !right) move.x =-1.0f * dir;
            if (!left && right) move.x = 1.0f * dir;
            if (backward && !forward) move.y =-1.0f;
            if (!backward && forward) move.y = 1.0f;

            // Make it so that moving diagonally doesn't go faster:
            if (move != glm::vec3(0.0f)) move = glm::normalize(move) * 2.0f * PlayerSpeed * elapsed;

        } else if (morph == 3) {
            float PlayerSpeed = 6.0f;

            float dir = isFlipped ? -1.0f : 1.0f;

            if (left && !right) move.x =-1.0f * dir;
            if (!left && right) move.x = 1.0f * dir;
            if (backward && !forward) move.y =-1.0f;
            if (!backward && forward) move.y = 1.0f;

            if (move != glm::vec3(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;
        }
    }

    // Get move in world coordinate system:
    glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);
    glm::vec3 animate_pos = player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);
    {
        // Using a for() instead of a while() here so that if walkpoint gets stuck in
		// Some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			tutorial_walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				// Finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			// Some step remains:
			remain *= (1.0f - time);
			// Try to step over edge:
			glm::quat rotation;
			if (tutorial_walkmesh->cross_edge(player.at, &end, &rotation, morph)) {
				// Stepped to a new triangle:
				player.at = end;
				// Rotate step to follow surface:
				remain = rotation * remain;
			} else {
				// Ran into a wall, bounce / slide along it:
				glm::vec3 const &a = tutorial_walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = tutorial_walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = tutorial_walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				// Check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					// bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					// If it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}
    }

    // Update character positions according to tutorial_walkmesh
    {
        player.transform->position = tutorial_walkmesh->to_world_point(player.at);
        // update character mesh's position to respect walking
        if (game_characters[morph].ctype) {
            game_characters[morph].ch_transform->position = player.transform->position;
        } else {
            game_characters[morph].ch_animate->transform->position = player.transform->position;
        }
    }

    // Perform animations and other in-game interactions
    {
        if (morph == 0) {
            // Animation
            float angle = (animate_pos.x*60.0f);
            worm.ch_animate->transform->rotation *= glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

            if (!animate_pos.x && (worm.ch_animate->transform->rotation.w != -worm.ch_animate->transform->rotation.y)) {
                worm.ch_animate->transform->rotation = worm.wstarting_rotation;
                if (isFlipped){
                    worm.ch_animate->transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(.0f, 0.0f, 1.0f)); 
                }

            }

            worm_animations[0].position += animate_pos.y*0.8f;
            worm_animations[0].position -= std::floor(worm_animations[0].position);

            for (auto &anim : worm_animations) {
                anim.update(elapsed);
            }
        }
        if (morph == 1) {
            if (isFlipped) {
                currZ -= moveZ;
                if (currZ < ((-1 * jumpDist.at(jumpNum)) + floorZ)) {
                    float extra = abs(currZ - ((-1 * jumpDist.at(jumpNum)) + floorZ));
                    currZ = ((-1 * jumpDist.at(jumpNum)) + floorZ) + extra;
                    jumpDir = -1.0f;
                }
                if (currZ > floorZ) {
                    float extra = abs(floorZ - currZ);
                    currZ = floorZ - extra;
                    jumpDir = 1.0f;
                    jumpNum = (jumpNum + 1) % 3;
                    if (jumpNum == 0) {
                        accel = 1.0f;
                    }
                }
            } else {
                currZ += moveZ;
                if (currZ > jumpDist.at(jumpNum) + floorZ) {
                    float extra = currZ - (jumpDist.at(jumpNum) + floorZ);
                    currZ = (jumpDist.at(jumpNum) + floorZ) - extra;
                    jumpDir = -1.0f;
                }
                if (currZ < floorZ) {
                    float extra = floorZ - currZ;
                    currZ = floorZ + extra;
                    jumpDir = 1.0f;
                    jumpNum = (jumpNum + 1) % 3;
                    if (jumpNum == 0) {
                        accel = 1.0f;
                    }
                }
            }

            // Walkmesh
            player.transform->position = tutorial_walkmesh->to_world_point(player.at);
            player.transform->position.z = currZ;
            if (isFlipped) {
                player.transform->position.z += 1.75f;
            }

            // update character mesh's position to respect walking
            game_characters[morph].ch_transform->position = tutorial_walkmesh->to_world_point(player.at);
            game_characters[morph].ch_transform->position.z = currZ;
        }
        else if (morph == 2) {
            // Animation
            rectangle.ch_animate->transform->position.z = 1.0f;
            rect_animations[0].position += abs(animate_pos.x)* 0.4f  + abs(animate_pos.y)* 0.4f ;
            rect_animations[0].position -= std::floor(rect_animations[0].position);
            // std::cout<<"animation pos: "<<rect_animations[0].position<<std::endl;

            if (animate_pos.x == 0.0f && animate_pos.y == 0.0f){
                rectangle.ch_animate->transform->rotation = 
                    rectangle.wstarting_rotation;
            }

            if (animate_pos.x != 0.0f || animate_pos.y != 0.0f){
                float prev = std::floor(rectRot);
                rectRot += elapsed*10.0f;
                float cur = std::floor(rectRot)-prev;
                if (animate_pos.x != 0.0f){
                    xDir = true;
                    if (xDir != prevDir) {
                        rectangle.ch_animate->transform->rotation = rectangle.wstarting_rotation;
                        prevDir = xDir;
                    }
                    if (animate_pos.x < 0.0f) {
                        rectangle.ch_animate->transform->rotation *= 
                        glm::angleAxis(-cur*10.0f, glm::vec3(0.0f,1.0f,0.0f));
                    } else {
                        rectangle.ch_animate->transform->rotation *= 
                        glm::angleAxis(cur*10.0f, glm::vec3(0.0f,1.0f,0.0f));
                    }
                } else {
                    xDir = false;
                    if (xDir != prevDir) {
                        rectangle.ch_animate->transform->rotation = rectangle.wstarting_rotation;
                        prevDir = xDir;
                    }
                    glm::vec3 prevPos = rectangle.ch_animate->transform->position;
                    rectangle.ch_animate->transform->position = glm::vec3(0.0f,0.0f,0.0f);
                    rectangle.ch_animate->transform->rotation *= 
                        glm::angleAxis(cur*10.0f, glm::vec3(0.0f,0.0f,1.0f));
                    rectangle.ch_animate->transform->position = prevPos;
                }
            }
            // Walkmesh
            player.transform->position = tutorial_walkmesh->to_world_point(player.at);
            floorZ = player.transform->position.z;
            if (isFlipped) {
                floorZ -= 1.2f;
            }
           

            // update character mesh's position to respect walking
            game_characters[morph].ch_animate->transform->position = tutorial_walkmesh->to_world_point(player.at);

            game_characters[morph].ch_animate->transform->position += tutorial_walkmesh->to_world_triangle_normal(player.at);
            if (!isFlipped) {
                game_characters[morph].ch_animate->transform->position.z += 2.0f;
            }

            game_characters[morph].ch_animate->transform->position.z += isFlipped ? -1.0f : 1.0f;

            for (auto &anim : rect_animations) {
                anim.update(elapsed);
            }
        }
        else if (morph == 3) {
            // Flip if inverted
            if (justFlipped) {
                floorZ -= isFlipped;
                blob.ch_animate->transform->position.z *= -1;
                blob.ch_animate->transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

                    // flip other characters too
                    for (auto &character : game_characters) {
                        if (character.first != morph) {
                            Character &ch = character.second;
                            if (ch.ctype) {
                                ch.ch_transform->position.z *= -1;
                                ch.ch_transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                            } else {
                                ch.ch_animate->transform->position.z *= -1;
                                ch.ch_animate->transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                            } 
                        }
                    }
                    
                    
                    camera_offset_pos.z *= -1;
                    camera_offset_rot = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * camera_offset_rot;
                    justFlipped = false;
                }

            // Animation
            blob_animations[0].position += abs(animate_pos.x) * 0.4f + abs(animate_pos.y) * 0.4f;
            blob_animations[0].position -= std::floor(blob_animations[0].position);

            for (auto &anim : blob_animations) {
                anim.update(elapsed);
            }
        }

        // Update camera location and rotation
        camera->transform->rotation = player.transform->rotation * camera_offset_rot;
        camera->transform->position = (player.transform->position + (player.transform->rotation *camera_offset_pos));
    }
    
    // Check for collision with beads
    beadCollision(0.1f);
}



/*
void TutorialMode::draw(glm::uvec2 const &drawable_size) {
    if (num_beads <= 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        main_text_renderer->set_drawable_size(drawable_size);
        glm::uvec2 center = glm::uvec2(drawable_size.x / 2, drawable_size.y / 2);
        float size_ratio = drawable_size.y / 1200.0f;

        main_text_renderer->renderText("You Win", center.x - 450.0f * size_ratio, center.y - 50.0f * size_ratio, 1.2f * size_ratio, win_text_color);
        main_text_renderer->renderText("Press ENTER to Play Again", center.x - 625.0f * size_ratio, 150.0f * size_ratio, 0.5f * size_ratio, main_text_color);
        return;
    }

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

    // // Walkmesh 
    // {
	// 	glDisable(GL_DEPTH_TEST);
	// 	DrawLines lines(camera->make_projection() * glm::mat4(camera->transform->make_world_to_local()));
	// 	for (auto const &tri : tutorial_walkmesh->triangles) {
	// 		lines.draw(tutorial_walkmesh->vertices[tri.x], tutorial_walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 		lines.draw(tutorial_walkmesh->vertices[tri.y], tutorial_walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 		lines.draw(tutorial_walkmesh->vertices[tri.z], tutorial_walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 	}
	// }

    {
        main_text_renderer->set_drawable_size(drawable_size);
        // glm::uvec2 center = glm::uvec2(drawable_size.x / 2, drawable_size.y / 2);
        float size_ratio = drawable_size.y / 1200.0f;

        main_text_renderer->renderText("beads remaining: " + std::to_string(num_beads), 50.0f, 50.0f, main_text_size * size_ratio, main_text_color);
        std::string new_time = std::to_string(game_time);
        std::size_t pos = new_time.find(".");
        main_text_renderer->renderText("time: " + new_time.substr(0,pos+3), drawable_size.x - 350.0f * size_ratio, 50.0f, main_text_size * size_ratio, main_text_color);
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

        // TODO: add num bead counts in a better format 
		constexpr float H = 0.09f;
		lines.draw_text("WASD moves; Space for specialty, NumKeys Morphs                            beads remaining: " + std::to_string(num_beads),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves; Space for specialty, NumKeys Morphs                            beads remaining: " + std::to_string(num_beads),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	GL_ERRORS();
}
*/

void TutorialMode::draw(glm::uvec2 const &drawable_size) {
    main_text_renderer->set_drawable_size(drawable_size);
    glm::uvec2 center = glm::uvec2(drawable_size.x / 2, drawable_size.y / 2);
    float size_ratio = drawable_size.y / 1200.0f;
    if (stage == 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        main_text_renderer->renderWrappedText(intro_text, center.y - 250.0f * size_ratio, main_text_size * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed), true);
        main_text_renderer->renderWrappedText("Press ENTER to continue", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-5), true);
        return;
    }

    if (stage == 7 || (stage == 6 && time_elapsed > 15)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        main_text_renderer->renderWrappedText("Press ENTER to start the game", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-9), true);
        return;
    }

    // if (num_beads <= 8) {
    //     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //     glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //     main_text_renderer->set_drawable_size(drawable_size);
    //     glm::uvec2 center = glm::uvec2(drawable_size.x / 2, drawable_size.y / 2);
    //     float size_ratio = drawable_size.y / 1200.0f;

    //     main_text_renderer->renderText("You Win", center.x - 450.0f * size_ratio, center.y - 50.0f * size_ratio, 1.2f * size_ratio, win_text_color);
    //     main_text_renderer->renderText("Press ENTER to Play Again", center.x - 625.0f * size_ratio, 150.0f * size_ratio, 0.5f * size_ratio, main_text_color);
    //     return;
    // }

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

    // // Walkmesh 
    // {
	// 	glDisable(GL_DEPTH_TEST);
	// 	DrawLines lines(camera->make_projection() * glm::mat4(camera->transform->make_world_to_local()));
	// 	for (auto const &tri : tutorial_walkmesh->triangles) {
	// 		lines.draw(tutorial_walkmesh->vertices[tri.x], tutorial_walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 		lines.draw(tutorial_walkmesh->vertices[tri.y], tutorial_walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 		lines.draw(tutorial_walkmesh->vertices[tri.z], tutorial_walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
	// 	}
	// }

    main_text_renderer->renderWrappedText("Press ENTER to skip to end of tutorial", 0.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed), true);

    
    if (stage == 1 && time_elapsed > 2 && time_elapsed < 7) {
        main_text_renderer->renderWrappedText("Pan the world by clicking and moving around", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    if (stage == 1 && time_elapsed > 9 && time_elapsed < 13) {
        main_text_renderer->renderWrappedText("Press escape to stop panning", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-10), true);
    }
    if (stage == 1 && time_elapsed > 15) {
        main_text_renderer->renderWrappedText("Press the 1 key to morph into a worm", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-16), true);
    }
    if (stage == 2 && time_elapsed > 2 && time_elapsed < 9) {
        main_text_renderer->renderWrappedText("Use the WASD keys to move", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    if (stage == 2 && time_elapsed > 13) {
        main_text_renderer->renderWrappedText("Press the 2 key to morph back into a cat", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-14), true);
    }
    if (stage == 3 && time_elapsed > 2 && time_elapsed < 7) {
        main_text_renderer->renderWrappedText("Each morph has unique movements and abilities", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    if (stage == 3 && time_elapsed > 9 && time_elapsed < 13) {
        main_text_renderer->renderWrappedText("The cat has a calculated jump sequence", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-10), true);
    }
    if (stage == 3 && time_elapsed > 15) {
        main_text_renderer->renderWrappedText("Press the 3 key to morph into a cube", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-16), true);
    }
    if (stage == 4 && time_elapsed > 2 && time_elapsed < 7) {
        main_text_renderer->renderWrappedText("The cube can scale surfaces", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    if (stage == 4 && time_elapsed > 9) {
        main_text_renderer->renderWrappedText("Press the 4 key to morph into the last character", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-10), true);
    }
    if (stage == 5 && time_elapsed > 2) {
        main_text_renderer->renderWrappedText("Press the spacebar to send the blob through the floor", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    if (stage == 6 && time_elapsed > 2 && time_elapsed < 7) {
        main_text_renderer->renderWrappedText("You have now entered the inverted world", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-3), true);
    }
    // if (stage == 6 && time_elapsed > 9 && time_elapsed < 13) {
    //     main_text_renderer->renderWrappedText("Pan the world by clicking and moving around", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-10), true);
    // }
    // if (stage == 6 && time_elapsed > 15) {
    //     main_text_renderer->renderWrappedText("Press escape to stop panning", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-16), true);
    // }
    if (stage == 6 && time_elapsed > 9 && time_elapsed < 13) {
        main_text_renderer->renderWrappedText("Explore the village to find the 9 beads to bring you home!", 50.0f * size_ratio, 0.2f * size_ratio, lerp(dark_text_color, main_text_color, time_elapsed-10), true);
    }




    {
        main_text_renderer->renderText("beads remaining: " + std::to_string(num_beads), 50.0f, 50.0f, main_text_size * size_ratio, main_text_color);
        std::string new_time = std::to_string(game_time);
        std::size_t pos = new_time.find(".");
        main_text_renderer->renderText("time: " + new_time.substr(0,pos+3), drawable_size.x - 350.0f * size_ratio, 50.0f, main_text_size * size_ratio, main_text_color);
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

        // TODO: add num bead counts in a better format 
		constexpr float H = 0.09f;
		lines.draw_text("WASD moves; Space for specialty, NumKeys Morphs                            beads remaining: " + std::to_string(num_beads),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD moves; Space for specialty, NumKeys Morphs                            beads remaining: " + std::to_string(num_beads),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	GL_ERRORS();
}

void TutorialMode::morphCharacter(bool forced) {
    if (old_morph != morph || forced) { 
        // Get position of previous active character
        glm::vec3 pos; 
        glm::quat rot;
        Character old_ch = game_characters[old_morph];
        Character new_ch = game_characters[morph];
        if (old_ch.ctype) {
            pos = old_ch.ch_transform->position; 
            rot = old_ch.ch_transform->rotation;
        } else {
            pos = old_ch.ch_animate->transform->position; 
            rot = old_ch.ch_animate->transform->rotation;
        }
        if (old_morph == 2) {
            pos.z += isFlipped ? -1.0f : 1.0f;
        }
        // std::cout << "old_pos: " << pos.x << " " << pos.y << " " << pos.z << std::endl; 
        // Update position & rotation of new character
        if (new_ch.ctype) {
            game_characters[morph].ch_transform->position = pos; 
            game_characters[morph].ch_transform->rotation = rot;
            game_characters[morph].cangle = old_ch.cangle;
            new_ch.cangle = old_ch.cangle;
            game_characters[morph].ch_transform->rotation = glm::angleAxis(new_ch.cangle, glm::vec3(0.0f,0.0f,1.0f));
        } else { 
            new_ch.ch_animate->transform->position = pos; 
            // Instead of updating rotation of character, update cam rotation
            camera->transform->rotation = cam_init_rot;
            player.transform->rotation = start_rot;
            new_ch.cangle = 0.0f;
        }
        if (morph == 1) {
            currZ = floorZ;
            accel = 1.0f;
            jumpNum = 0;
            jumpDir = 1.0f;
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

void TutorialMode::beadCollision(float eps) { 
    Character ch = game_characters[morph];
    glm::vec3 ch_pos;
    float threshold; 
    if (ch.ctype) {
        ch_pos = ch.ch_transform->position; 
        threshold = 2+eps;
    }
    else {
        ch_pos = ch.ch_animate->transform->position; 
        threshold = 6.5f + eps; 
    }

    for (size_t i = 0; i < beads.size(); i++) { 
        auto bead = beads[i];
        if (!bead->include) continue;
        glm::vec3 bead_pos = bead->position; 
        glm::vec3 pos_diff = ch_pos - bead_pos;
        if (abs(glm::dot(pos_diff,pos_diff)) <= threshold) {
            bead->include = false;
            num_beads -= 1;
            break;
        }
        // TODO: update to make winning more glamorous 
        if (num_beads == 0) {
            std::cout << "YOU WIN\n";
            exit(0);
        }        
    }
}