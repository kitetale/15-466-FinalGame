#include "BoneAnimation.hpp"

#include "read_write_chunk.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <set>
#include <fstream>
#include <algorithm>

BoneAnimation::BoneAnimation(std::string const &filename) {
	std::cout << "Reading bone-based animation from '" << filename << "'." << std::endl;

	std::ifstream file(filename, std::ios::binary);

	std::vector< char > strings;
	read_chunk(file, "str0", &strings);

	{ //read bones:
		struct BoneInfo {
			uint32_t name_begin, name_end;
			uint32_t parent;
			glm::mat4x3 inverse_bind_matrix;
		};
		static_assert(sizeof(BoneInfo) == 4*2 + 4 + 4*12, "BoneInfo is packed.");

		std::vector< BoneInfo > file_bones;
		read_chunk(file, "bon0", &file_bones);
		bones.reserve(file_bones.size());
		for (auto const &file_bone : file_bones) {
			if (!(file_bone.name_begin <= file_bone.name_end && file_bone.name_end <= strings.size())) {
				throw std::runtime_error("bone has out-of-range name begin/end");
			}
			if (!(file_bone.parent == -1U || file_bone.parent < bones.size())) {
				throw std::runtime_error("bone has invalid parent");
			}
			bones.emplace_back();
			Bone &bone = bones.back();
			bone.name = std::string(&strings[0] + file_bone.name_begin, &strings[0] + file_bone.name_end);
			bone.parent = file_bone.parent;
			bone.inverse_bind_matrix = file_bone.inverse_bind_matrix;
		}
	}

	static_assert(sizeof(PoseBone) == 3*4 + 4*4 + 3*4, "PoseBone is packed.");
	read_chunk(file, "frm0", &frame_bones);
	if (frame_bones.size() % bones.size() != 0) {
		throw std::runtime_error("frame bones is not divisible by bones");
	}

	uint32_t frames = uint32_t(frame_bones.size() / bones.size());

	{ //read actions (animations):
		struct AnimationInfo {
			uint32_t name_begin, name_end;
			uint32_t begin, end;
		};
		static_assert(sizeof(AnimationInfo) == 4*2 + 4*2, "AnimationInfo is packed.");

		std::vector< AnimationInfo > file_animations;
		read_chunk(file, "act0", &file_animations);
		animations.reserve(file_animations.size());
		for (auto const &file_animation : file_animations) {
			if (!(file_animation.name_begin <= file_animation.name_end && file_animation.name_end <= strings.size())) {
				throw std::runtime_error("animation has out-of-range name begin/end");
			}
			if (!(file_animation.begin <= file_animation.end && file_animation.end <= frames)) {
				throw std::runtime_error("animation has out-of-range frames begin/end");
			}
			animations.emplace_back();
			Animation &animation = animations.back();
			animation.name = std::string(&strings[0] + file_animation.name_begin, &strings[0] + file_animation.name_end);
			animation.begin = file_animation.begin;
			animation.end = file_animation.end;
		}
	}

	{ //read actual mesh:
		struct Vertex {
			glm::vec3 Position;
			glm::vec3 Normal;
			glm::u8vec4 Color;
			glm::vec2 TexCoord;
			glm::vec4 BoneWeights;
			glm::uvec4 BoneIndices;
		};
		static_assert(sizeof(Vertex) == 3*4+3*4+4*1+2*4+4*4+4*4, "Vertex is packed.");
		//GLAttribBuffer< glm::vec3, glm::vec3, glm::u8vec4, glm::vec2, glm::vec4, glm::uvec4 > buffer;
		std::vector< Vertex > data;
		read_chunk(file, "msh0", &data);

		//check bone indices:
		for (auto const &vertex : data) {
			if ( vertex.BoneIndices.x >= bones.size()
				|| vertex.BoneIndices.y >= bones.size()
				|| vertex.BoneIndices.z >= bones.size()
				|| vertex.BoneIndices.w >= bones.size()
			) {
				throw std::runtime_error("animation mesh has out of range vertex index");
			}
		}

		{ //DEBUG: dump bounding box info
			glm::vec3 min = glm::vec3(std::numeric_limits< float >::infinity());
			glm::vec3 max = glm::vec3(-std::numeric_limits< float >::infinity());
			for (auto const &v : data) {
				min = glm::min(min, v.Position);
				max = glm::max(max, v.Position);
			}
			std::cout << "INFO: bounding box of animation mesh in '" << filename << "' is [" << min.x << "," << max.x << "]x[" << min.y << "," << max.y << "]x[" << min.z << "," << max.z << "]" << std::endl;
		}

		//upload data:
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(Vertex), data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//specify the (only) mesh:
		mesh.start = 0;
		mesh.count = GLuint(data.size());

		//store attributes for later vao creation:
		Position = Attrib(buffer, 3, GL_FLOAT, Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, Position));
		Normal = Attrib(buffer, 3, GL_FLOAT, Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, Normal));
		Color = Attrib(buffer, 4, GL_UNSIGNED_BYTE, Attrib::AsFloatFromFixedPoint, sizeof(Vertex), offsetof(Vertex, Color));
		TexCoord = Attrib(buffer, 2, GL_FLOAT, Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, TexCoord));
		BoneWeights = Attrib(buffer, 4, GL_FLOAT, Attrib::AsFloat, sizeof(Vertex), offsetof(Vertex, BoneWeights));
		BoneIndices = Attrib(buffer, 4, GL_UNSIGNED_INT, Attrib::AsInteger, sizeof(Vertex), offsetof(Vertex, BoneIndices));

	}

	GL_ERRORS();
}

const BoneAnimation::Animation &BoneAnimation::lookup(std::string const &name) const {
	for (auto const &animation : animations) {
		if (animation.name == name) return animation;
	}
	throw std::runtime_error("Animation with name '" + name + "' does not exist.");
}

GLuint BoneAnimation::make_vao_for_program(GLuint program) const {
	std::map< std::string, Attrib const * > attribs;

	attribs["Position"] = &Position;
	attribs["Normal"] = &Normal;
	attribs["Color"] = &Color;
	attribs["TexCoord"] = &TexCoord;
	attribs["BoneWeights"] = &BoneWeights;
	attribs["BoneIndices"] = &BoneIndices;

	return ::make_vao_for_program(attribs, program);
}

// - - - - - - - - - - - - - - - - - - - - - - - - -

BoneAnimationPlayer::BoneAnimationPlayer(BoneAnimation const &banims_, BoneAnimation::Animation const &anim_, LoopOrOnce loop_or_once_, float speed) : banims(banims_), anim(anim_), loop_or_once(loop_or_once_) {
	set_speed(speed);
}

void BoneAnimationPlayer::update(float elapsed) {
	position += elapsed * position_per_second;
	if (loop_or_once == Loop) {
		position -= std::floor(position);
	} else { //(loop_or_once == Once)
		position = std::max(std::min(position, 1.0f), 0.0f);
	}
}

void BoneAnimationPlayer::set_uniform(GLint bones_mat4x3_array) const {
	std::vector< glm::mat4x3 > bone_to_object(banims.bones.size()); //needed for hierarchy
	std::vector< glm::mat4x3 > bones(banims.bones.size()); //actual uniforms

	int32_t frame = int32_t(std::floor((anim.end - 1 - anim.begin) * position + anim.begin));
	if (frame < int32_t(anim.begin)) frame = anim.begin;
	if (frame > int32_t(anim.end)-1) frame = int32_t(anim.end)-1;
	BoneAnimation::PoseBone const *pose = banims.get_frame(frame);
	for (uint32_t b = 0; b < bones.size(); ++b) {
		BoneAnimation::PoseBone const &pose_bone = pose[b];
		BoneAnimation::Bone const &bone = banims.bones[b];

		glm::mat3 r = glm::mat3_cast(pose_bone.rotation);
		glm::mat3 rs = glm::mat3(
			r[0] * pose_bone.scale.x,
			r[1] * pose_bone.scale.y,
			r[2] * pose_bone.scale.z
		);
		glm::mat4x3 trs = glm::mat4x3(
			rs[0], rs[1], rs[2], pose_bone.position
		);

		if (bone.parent == -1U) {
			bone_to_object[b] = trs;
			bone_to_object[b] = glm::mat4x3(1.0f); //clear root position
		} else {
			bone_to_object[b] = bone_to_object[bone.parent] * glm::mat4(trs);
		}
		bones[b] = bone_to_object[b] * glm::mat4(bone.inverse_bind_matrix);
	}
	glUniformMatrix4x3fv(bones_mat4x3_array, GLsizei(bones.size()), GL_FALSE, glm::value_ptr(bones[0]));
}