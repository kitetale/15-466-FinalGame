#include "WormMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "BoneLitColorTextureProgram.hpp"
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
	auto ret = new MeshBuffer(data_path("worm.pnct"));
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
	return new Scene(data_path("worm.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
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

// *************************** WALK MESH ***********************
WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > worm_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("worm.w"));
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

        //create a player camera attached to a child of the player transform:
        scene.transforms.emplace_back();
        scene.cameras.emplace_back(&scene.transforms.back());
        character.camera = &scene.cameras.back();
        character.camera->fovy = glm::radians(90.0f);
        character.camera->near = 0.01f;
        character.camera->transform->position = glm::vec3(0.0f, -4.0f, 7.0f);

        //rotate camera facing direction (-z) to player facing direction (+y):
        //character.camera->transform->rotation = glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        player.transform->position = glm::vec3(0.0f,0.0f,0.0f);
        character.camera->transform->rotation = glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        //start player walking at nearest walk point:
        player.at = walkmesh->nearest_walk_point(player.transform->position);
    }

    // ANIMATION SETUP ---------------------------------------------------------
	{ 
        //put some worms around the edge:
		Scene::Drawable::Pipeline worm_info;
		worm_info = bone_lit_color_texture_program_pipeline;

		worm_info.vao = *worm_banims_for_bone_lit_color_texture_program;
		worm_info.start = worm_banims->mesh.start;
		worm_info.count = worm_banims->mesh.count;

        // add crawl animation to worm_animations list
		worm_animations.reserve(1);
        worm_animations.emplace_back(*worm_banims, *worm_banim_crawl, BoneAnimationPlayer::Loop, 0.0f);

        BoneAnimationPlayer *wormAnimation = &worm_animations.back();
    
        worm_info.set_uniforms = [wormAnimation](){
            wormAnimation->set_uniform(bone_lit_color_texture_program->BONES_mat4x3_array);
        };

        scene.transforms.emplace_back();
        Scene::Transform *transform = &scene.transforms.back();
        transform->position.x = 0.0f;
        transform->position.y = 0.0f;
        transform->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        scene.drawables.emplace_back(transform);
        Scene::Drawable *worm1 = &scene.drawables.back();
        worm1->pipeline = worm_info;

        this->worm = worm1;
        worm->transform->position = glm::vec3(0.0f,0.0f,0.0f);

        startingRotation = worm->transform->rotation;
		
		assert(worm_animations.size() == 1);
	}

    // character.camera->transform->parent =  worm->transform;
    // character.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // character.camera->transform->position = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::vec3(0.0f, -4.0f, 7.0f);

}

WormMode::~WormMode() {
}

bool WormMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = true;
        left = false;
        right = false;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_UP) {
		forward = false;
		return true;
	}
	if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = true;
        left = false;
        right = false;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_DOWN) {
		backward = false;
		return true;
	}
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
		left = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_LEFT) {
		left = false;
		return true;
	}
    if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
		right = true;
		return true;
	}
	if (evt.type == SDL_KEYUP && evt.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
		right = false;
		return true;
	}


	return false;
}

void WormMode::update(float elapsed) {
	
    float step = 0.0f;
    float sideways = 0.0f;
    float speedForward = 1.0f;
    float speedSideways = 10.0f;

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

    {
        //get move in world coordinate system:
		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(sideways, step, 0.0f, 0.0f);
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

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		// // update character mesh's position to respect walking
		// character.character_transform->position = player.transform->position;

        worm->transform->position.y += step;
        worm->transform->position.x += sideways;
        character.camera->transform->position = worm->transform->position + glm::vec3(0.0f, -4.0f, 7.0f);

        float angle = (sideways*60.0f);
        worm->transform->rotation *= glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

        if (!sideways && (worm->transform->rotation.w != -worm->transform->rotation.y)) {
            worm->transform->rotation = startingRotation;
        }

        
        worm_animations[0].position += step *0.8f;
        worm_animations[0].position -= std::floor(worm_animations[0].position);

        for (auto &anim : worm_animations) {
            anim.update(elapsed);
        }
    }
    
}

void WormMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	character.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

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

	scene.draw(*character.camera);

	GL_ERRORS();
}