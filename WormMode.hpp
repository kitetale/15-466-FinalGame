#pragma once

#include "Mode.hpp"

#include "BoneAnimation.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <SDL.h>
#include <glm/glm.hpp>

#include <vector>
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

    uint8_t morph = 0; // 0 is worm, 1 is cat sphere
    bool justChanged = true;

    glm::quat startingRotation; // for worm

    //goal to reach
	Scene::Transform *goal = nullptr;
	Scene::Transform *topGoal = nullptr;

    //player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
	} player;

	// camera 
	struct Character {
		Scene::Transform *character_transform = nullptr;
		Scene::Camera *camera = nullptr;
	} character;

	//scene:
	Scene scene;
	Scene::Drawable *worm = nullptr;
	float camera_radius = 10.0f;
	float camera_azimuth = glm::radians(60.0f);
	float camera_elevation = glm::radians(45.0f);

	std::vector< BoneAnimationPlayer > worm_animations;
};