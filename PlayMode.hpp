#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	//collision check:
	struct Ray {
		glm::vec3 orig;
		glm::vec3 dir;
	} r1, r2, r3, r4;

	glm::vec3 r1_base;

	//width, length, height of character
	glm::vec3 dimension = glm::vec3 (1.0f, 1.0f, 2.0f); // height is from the floor
	glm::vec3 halfDim = dimension/2.0f; //half of each for ease of computation
	glm::vec3 minBound = glm::vec3 (0.0f, 0.0f, 0.0f); // in world coord, update by player pos
	glm::vec3 maxBound = glm::vec3 (1.0f, 1.0f, 2.0f); // in world coord, update by player pos

	bool BoxRayCollision(Ray r);

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform *ray1 = nullptr;
	glm::quat ray1_base_rot;

	float wobble = 0.0f;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by mouse left/right motion:
		Scene::Transform *transform = nullptr;
		int id; 
		
	} player;

	// camera 
	struct Character {
		Scene::Transform *character_transform = nullptr;
		Scene::Camera *camera = nullptr;
	} character;
	glm::quat char_base_rot;
};