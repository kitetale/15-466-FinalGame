#include "WormMode.hpp"
#include "gl_errors.hpp"

//Reference : https://learnopengl.com/In-Practice/2D-Game/Particles

uint32_t lastUsedParticle = 0;
uint32_t amount = 500;


int firstUnusedParticle();
void respawnParticle(Particle &particle, Character object, glm::vec2 offset);
