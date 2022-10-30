#pragma once

#include "Mesh.hpp"
#include "make_vao_for_program.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <map>
#include <string>
#include <vector>

//"BoneAnimation" holds a mesh loaded from a file along with skin weights,
// a heirarchy of bones and their bind info,
// and a collection of animations defined on those bones

struct BoneAnimation {
	//Skinned mesh:
	GLuint buffer = 0;
	Attrib Position;
	Attrib Normal;
	Attrib Color;
	Attrib TexCoord;
	Attrib BoneWeights;
	Attrib BoneIndices;
	Mesh mesh;

	//Skeleton description:
	struct Bone {
		std::string name;
		uint32_t parent = -1U;
		glm::mat4x3 inverse_bind_matrix;
	};
	std::vector< Bone > bones;

	//Animation poses:
	struct PoseBone {
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	std::vector< PoseBone > frame_bones;

	PoseBone const *get_frame(uint32_t frame) const {
		return &frame_bones[frame * bones.size()];
	}

	//Animation index:
	struct Animation {
		std::string name;
		uint32_t begin = 0;
		uint32_t end = 0;
	};

	std::vector< Animation > animations;


	//construct from a file:
	// note: will throw if file fails to read.
	BoneAnimation(std::string const &filename);

	//look up a particular animation, will throw if not found:
	const Animation &lookup(std::string const &name) const;

	//build a vertex array object that links this vbo to attributes to a program:
	//  will throw if program defines attributes not contained in this buffer
	//  and warn if this buffer contains attributes not active in the program
	GLuint make_vao_for_program(GLuint program) const;
};

struct BoneAnimationPlayer {
	enum LoopOrOnce { Once, Loop };
	BoneAnimationPlayer(BoneAnimation const &banims, BoneAnimation::Animation const &anim, LoopOrOnce loop_or_once = Once, float speed = 1.0f);

	BoneAnimation const &banims;
	BoneAnimation::Animation const &anim;

	void set_speed(float speed, float fps = 24.0f) {
		position_per_second = speed / ((anim.end-1-anim.begin) / fps);
	}

	float position = 0.0f; //from 0.0 == beginning to 1.0 == end
	float position_per_second = 1.0f;
	LoopOrOnce loop_or_once = Once;

	void update(float elapsed);

	void set_uniform(GLint bones_mat4x3_array) const;

	bool done() const { return (loop_or_once == Once && position >= 1.0f); }

};