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

	//controls:
	bool mouse_captured = false;
	bool forward = false;
	bool backward = false;
    bool left = false;
    bool right = false;

	uint8_t morph = 0; // 0 is worm, 1 is cat sphere, 2 is rectangle
	uint8_t old_morph = 0; // For changing between two characters
    bool justChanged = true;

    glm::quat startingRotation; // for worm

    //goal to reach
	Scene::Transform *goal = nullptr;
	Scene::Transform *topGoal = nullptr;
	Scene::Transform *rectGoal = nullptr;

    // Player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
	} player;

	// Camera 
	Scene::Camera *camera = nullptr;
	float camera_radius = 10.0f;
	float camera_azimuth = glm::radians(60.0f);
	float camera_elevation = glm::radians(45.0f);
	glm::vec3 camera_offset_pos = glm::vec3(0.0f, -4.0f, 7.0f);
	glm::quat camera_offset_rot = glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 character_off_pos = glm::vec3(0.0f, -20.0f, 0.0f);

	// Scene:
	Scene scene;

	// Different characters info
	// 0: worm
	Scene::Drawable *worm = nullptr;

	// 1: catball
	struct Character {
		Scene::Transform *character_transform = nullptr; // all 'standard' assets
		Scene::Drawable *character_animate = nullptr; // all animated assets i.e. worm 
		bool ctype = true; // true if standard false if animated
		bool isTallSide = true; // rectangle only 
		int64_t count = 0; // rectangle only
	} new_worm, catball, rectangle;

	// 2: rectangle only
	bool isTallSide = true; // rectangle only 
	// int flipped = false;

	std::unordered_map<int, Character> game_characters; 

	std::vector< BoneAnimationPlayer > worm_animations;
};