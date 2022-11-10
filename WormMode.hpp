#pragma once

#include "Mode.hpp"

#include "BoneAnimation.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <vector>
#include <map> 
#include <deque>
#include <list>

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
    glm::vec3 character_off_pos = glm::vec3(-100.0f, -100.0f, -100.0f);

	// Scene:
	Scene scene;

	// In-game attributes: 
	glm::vec3 start_pos = glm::vec3(0.0f,0.0f,0.0f);

	int morph = 1; // 0 is worm, 1 is cat sphere, 2 is rectangle
	int old_morph = 1; // For changing between two characters

    //goal to reach
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
		bool ctype = true; // true if standard false if animated
		glm::quat wstarting_rotation; 
		bool isTallSide = true; // rectangle only 
		int64_t count = 0; // rectangle only
	} worm, catball, rectangle, blob;
	std::unordered_map<int, Character> game_characters; 

	// 0: worm animations 
	std::vector< BoneAnimationPlayer > worm_animations;

	// 2: rectangle only
	bool isTallSide = true; 
	// int flipped = false;

	bool justFlipped = false; // for morph 2 flipping scene
	bool isFlipped = false; // for morph 2 flipping scene

	
	// Beads - goal of the game 
	std::vector< Scene::Transform*> beads;
	size_t num_beads; 

	// In-game events: 
	void morphCharacter(bool forced); // Change character
	void beadCollision(float eps);


};