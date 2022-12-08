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

struct WormMode : public Mode {
	WormMode();
	virtual ~WormMode();

	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// Controls:
	bool mouse_captured = false;
	bool forward = false;
	bool backward = false;
   	bool left = false;
    bool right = false;

	// Camera 
	Scene::Camera *camera = nullptr;
	float camera_radius = 10.0f;
	float camera_azimuth = glm::radians(60.0f);
	float camera_elevation = glm::radians(45.0f);
	glm::vec3 camera_offset_pos = glm::vec3(0.0f, -6.0f, 6.0f);
	glm::quat camera_offset_rot = glm::angleAxis(glm::radians(70.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::vec3 character_off_pos = glm::vec3(-100.0f);
	glm::quat cam_init_rot;

	// Scene:
	Scene scene;

	// In-game attributes: 
	glm::vec3 start_pos = glm::vec3(0.0f,0.0f,0.0f);
	glm::quat start_rot;

	int morph = 1; // 0 is worm, 1 is cat sphere, 2 is rectangle
	int old_morph = 1; // For changing between two characters

	// Goal to reach
	Scene::Transform *goal = nullptr;
	Scene::Transform *topGoal = nullptr;
	Scene::Transform *rectGoal = nullptr;

   	// Player info:
	struct Player {
		WalkPoint at;
		// Transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
	} player;

	// Different characters info
	struct Character {
		Scene::Transform *ch_transform = nullptr; // all 'standard' assets
		Scene::Drawable *ch_animate = nullptr; // all animated assets i.e. worm 
		float cangle = 0.0f; // character's rotation angle based on mouse move
		bool ctype = true; // true if standard false if animated
		glm::quat wstarting_rotation; 
	} worm, catball, rectangle, blob;
	std::unordered_map<int, Character> game_characters; 


	// Character specific variables:

	// 0: worm
	std::vector< BoneAnimationPlayer > worm_animations;
	std::vector< BoneAnimationPlayer > blob_animations;
	std::vector< BoneAnimationPlayer > rect_animations;

	// 1: catball
	std::vector<float> jumpDist = { 2.0f, 4.0f, 8.0f};
	int jumpNum = 0;
	float jumpDir = 1.0f; // flip between up (1.0f and down -1.0f)
	float floorZ = 0.0f;
	float accel = 1.0f;
	float currZ = 0.0f;
	float moveZ = 0.0f;

	// 2: rectangle
	float rectRot = 0.0f;
	bool xDir = false;
	bool prevDir = false;

	// 3: flipping sphere
	bool justFlipped = false;
	bool isFlipped = false;

	// In-game attributes: 
	void morphCharacter(bool forced); // Change character
	void beadCollision(float eps); // Check for collision with beads
	
	// Beads - goal of the game 
	std::vector< Scene::Transform* > beads;
	size_t num_beads; 

	// Lives and collisions 
	uint8_t num_lives = 3;
	std::vector < Scene::Transform* > obstacles;
	
	// Timing
	float game_time = 0.0f;

	// Font
	TextRenderer *mansalva_renderer = new TextRenderer(data_path("fonts/Mansalva-Regular.ttf"), 200);
	TextRenderer *main_text_renderer = mansalva_renderer;

	float main_text_size = 0.3f;
    glm::vec3 main_text_color = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 win_text_color = glm::vec3(0.0f, 1.0f, 0.1f);
};
