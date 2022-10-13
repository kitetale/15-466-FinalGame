#include "WalkMesh.hpp"

#include "read_write_chunk.hpp"

#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::vec3 > const &normals_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), normals(normals_), triangles(triangles_) {

	//construct next_vertex map (maps each edge to the next vertex in the triangle):
	next_vertex.reserve(triangles.size()*3);
	auto do_next = [this](uint32_t a, uint32_t b, uint32_t c) {
		auto ret = next_vertex.insert(std::make_pair(glm::uvec2(a,b), c));
		assert(ret.second);
	};
	for (auto const &tri : triangles) {
		do_next(tri.x, tri.y, tri.z);
		do_next(tri.y, tri.z, tri.x);
		do_next(tri.z, tri.x, tri.y);
	}

	//DEBUG: are vertex normals consistent with geometric normals?
	for (auto const &tri : triangles) {
		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];
		glm::vec3 out = glm::normalize(glm::cross(b-a, c-a));

		float da = glm::dot(out, normals[tri.x]);
		float db = glm::dot(out, normals[tri.y]);
		float dc = glm::dot(out, normals[tri.z]);

		assert(da > 0.1f && db > 0.1f && dc > 0.1f);
	}
}

//project pt to the plane of triangle a,b,c and return the barycentric weights of the projected point:
glm::vec3 barycentric_weights(glm::vec3 const &a, glm::vec3 const &b, glm::vec3 const &c, glm::vec3 const &pt) {
	// math equation from: 
	// https://math.stackexchange.com/questions/544946/determine-if-projection-of-3d-point-onto-plane-is-within-a-triangle
	
	glm::vec3 ab = b-a;
	glm::vec3 ac = c-a;
	
	glm::vec3 n = glm::cross(ab,ac);
	
	glm::vec3 ap = pt-a;
	
	float magnitude = glm::dot(n,n);
	
	float z = glm::dot(glm::cross(ab,ap),n)/(magnitude);
	float y = glm::dot(glm::cross(ap,ac),n)/(magnitude);
	float x = 1-z-y;
	
	return glm::vec3(x,y,z);
}

WalkPoint WalkMesh::nearest_walk_point(glm::vec3 const &world_point) const {
	assert(!triangles.empty() && "Cannot start on an empty walkmesh");

	WalkPoint closest;
	float closest_dis2 = std::numeric_limits< float >::infinity();

	for (auto const &tri : triangles) {
		//find closest point on triangle:

		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];

		//get barycentric coordinates of closest point in the plane of (a,b,c):
		glm::vec3 coords = barycentric_weights(a,b,c, world_point);

		//is that point inside the triangle?
		if (coords.x >= 0.0f && coords.y >= 0.0f && coords.z >= 0.0f) {
			//yes, point is inside triangle.
			float dis2 = glm::length2(world_point - to_world_point(WalkPoint(tri, coords)));
			if (dis2 < closest_dis2) {
				closest_dis2 = dis2;
				closest.indices = tri;
				closest.weights = coords;
			}
		} else {
			//check triangle vertices and edges:
			auto check_edge = [&world_point, &closest, &closest_dis2, this](uint32_t ai, uint32_t bi, uint32_t ci) {
				glm::vec3 const &a = vertices[ai];
				glm::vec3 const &b = vertices[bi];

				//find closest point on line segment ab:
				float along = glm::dot(world_point-a, b-a);
				float max = glm::dot(b-a, b-a);
				glm::vec3 pt;
				glm::vec3 coords;
				if (along < 0.0f) {
					pt = a;
					coords = glm::vec3(1.0f, 0.0f, 0.0f);
				} else if (along > max) {
					pt = b;
					coords = glm::vec3(0.0f, 1.0f, 0.0f);
				} else {
					float amt = along / max;
					pt = glm::mix(a, b, amt);
					coords = glm::vec3(1.0f - amt, amt, 0.0f);
				}

				float dis2 = glm::length2(world_point - pt);
				if (dis2 < closest_dis2) {
					closest_dis2 = dis2;
					closest.indices = glm::uvec3(ai, bi, ci);
					closest.weights = coords;
				}
			};
			check_edge(tri.x, tri.y, tri.z);
			check_edge(tri.y, tri.z, tri.x);
			check_edge(tri.z, tri.x, tri.y);
		}
	}
	assert(closest.indices.x < vertices.size());
	assert(closest.indices.y < vertices.size());
	assert(closest.indices.z < vertices.size());
	return closest;
}


void WalkMesh::walk_in_triangle(WalkPoint const &start, glm::vec3 const &step, WalkPoint *end_, float *time_) const {
	assert(end_);
	auto &end = *end_;

	assert(time_);
	auto &time = *time_;

	glm::vec3 const &a = vertices[start.indices.x];
	glm::vec3 const &b = vertices[start.indices.y];
	glm::vec3 const &c = vertices[start.indices.z];
	
	glm::vec3 pos = start.weights.x * a +
				    start.weights.y * b + 
				    start.weights.z * c;
	
	glm::vec3 endPos = glm::vec3(pos.x+step.x, pos.y+step.y, pos.z+step.z);
	glm::vec3 endWeights = barycentric_weights(a,b,c,endPos);
	
	float wa = endWeights.x;
	float wb = endWeights.y;
	float wc = endWeights.z;

	// has crossed an edge (know when barycentric is below 0)
	if (wc <= 0 || wb <= 0 || wa <= 0) {
		//a + t(a'-a) = 0
		// t = a/(a_end-a)

		float at = 1.0f;
		float bt = 1.0f;
		float ct = 1.0f;
		int side = 2; //0:a 1:b 2:c
		
		if (wa <= 0) {
			at = -start.weights.x / (wa - start.weights.x);
			side = 0;
		}
		if (wb <= 0) {
			bt = -start.weights.y / (wb - start.weights.y);
			if (glm::min(at,bt)==bt) side = 1;
		}
		if (wc <= 0) {
			ct = -start.weights.z / (wc - start.weights.z);
			if (glm::min(glm::min(at,bt),ct)==ct) side = 2;
		}
		float t = glm::min(glm::min(at,bt),ct);
		
		 // crosses edge
		glm::vec3 endPosEdge = glm::vec3(pos.x+step.x*t, pos.y+step.y*t, pos.z+step.z*t);
		glm::vec3 endPosWeights = barycentric_weights(a,b,c,endPosEdge);
		
		if (side == 0) {
			end.weights.x = endPosWeights.y;
			end.weights.y = endPosWeights.z;
			end.weights.z = endPosWeights.x;
			end.indices.x = start.indices.y;
			end.indices.y = start.indices.z;
			end.indices.z = start.indices.x;
		} else if (side == 1) {
			end.weights.x = endPosWeights.z;
			end.weights.y = endPosWeights.x;
			end.weights.z = endPosWeights.y;
			end.indices.x = start.indices.z;
			end.indices.y = start.indices.x;
			end.indices.z = start.indices.y;
		} else {
			end.weights.x = endPosWeights.x;
			end.weights.y = endPosWeights.y;
			end.weights.z = endPosWeights.z;
			end.indices.x = start.indices.x;
			end.indices.y = start.indices.y;
			end.indices.z = start.indices.z;
		}
		
		time = t;
			
	} else {
		end = start;
		end.weights.x = wa;
		end.weights.y = wb;
		end.weights.z = wc;
		time = 1;
	}

	//Remember: our convention is that when a WalkPoint is on an edge,
	// then wp.weights.z == 0.0f (so will likely need to re-order the indices)
}	

bool WalkMesh::cross_edge(WalkPoint const &start, WalkPoint *end_, glm::quat *rotation_) const {
	assert(start.weights.z <= 0.001f);
	assert(start.indices.x <= vertices.size() && start.indices.y <= vertices.size() && start.indices.z <= vertices.size());

	assert(end_);
	auto &end = *end_;

	assert(rotation_);
	auto &rotation = *rotation_;

	glm::uvec2 edge = glm::uvec2(start.indices);

	//check if 'edge' is a non-boundary edge:
	glm::vec3 nextTri;
	auto f = next_vertex.find(glm::vec2(edge.y, edge.x));
	if (f != next_vertex.end()) {
		nextTri = glm::vec3(edge.y, edge.x, f->second);
	} else {
		end = start;
		rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		return false;
	}

	// another triangle found, set end's weights and indicies on that triangle:
	end = start;
	end.weights.x = start.weights.y;
	end.weights.y = start.weights.x;
	end.indices = nextTri;
	
	glm::vec3 startn = to_world_triangle_normal(start);
	glm::vec3 endn = to_world_triangle_normal(end);
	rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); //identity quat (wxyz init order)
	rotation = glm::rotation(startn, endn);

	// angle if needed
	//float angle = glm::angle(rotation);

	return true;
}


WalkMeshes::WalkMeshes(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);

	std::vector< glm::vec3 > vertices;
	read_chunk(file, "p...", &vertices);

	std::vector< glm::vec3 > normals;
	read_chunk(file, "n...", &normals);

	std::vector< glm::uvec3 > triangles;
	read_chunk(file, "tri0", &triangles);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct IndexEntry {
		uint32_t name_begin, name_end;
		uint32_t vertex_begin, vertex_end;
		uint32_t triangle_begin, triangle_end;
	};

	std::vector< IndexEntry > index;
	read_chunk(file, "idxA", &index);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in walkmesh file '" << filename << "'" << std::endl;
	}

	//-----------------

	if (vertices.size() != normals.size()) {
		throw std::runtime_error("Mis-matched position and normal sizes in '" + filename + "'");
	}

	for (auto const &e : index) {
		if (!(e.name_begin <= e.name_end && e.name_end <= names.size())) {
			throw std::runtime_error("Invalid name indices in index of '" + filename + "'");
		}
		if (!(e.vertex_begin <= e.vertex_end && e.vertex_end <= vertices.size())) {
			throw std::runtime_error("Invalid vertex indices in index of '" + filename + "'");
		}
		if (!(e.triangle_begin <= e.triangle_end && e.triangle_end <= triangles.size())) {
			throw std::runtime_error("Invalid triangle indices in index of '" + filename + "'");
		}

		//copy vertices/normals:
		std::vector< glm::vec3 > wm_vertices(vertices.begin() + e.vertex_begin, vertices.begin() + e.vertex_end);
		std::vector< glm::vec3 > wm_normals(normals.begin() + e.vertex_begin, normals.begin() + e.vertex_end);

		//remap triangles:
		std::vector< glm::uvec3 > wm_triangles; wm_triangles.reserve(e.triangle_end - e.triangle_begin);
		for (uint32_t ti = e.triangle_begin; ti != e.triangle_end; ++ti) {
			if (!( (e.vertex_begin <= triangles[ti].x && triangles[ti].x < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].y && triangles[ti].y < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].z && triangles[ti].z < e.vertex_end) )) {
				throw std::runtime_error("Invalid triangle in '" + filename + "'");
			}
			wm_triangles.emplace_back(
				triangles[ti].x - e.vertex_begin,
				triangles[ti].y - e.vertex_begin,
				triangles[ti].z - e.vertex_begin
			);
		}
		
		std::string name(names.begin() + e.name_begin, names.begin() + e.name_end);

		auto ret = meshes.emplace(name, WalkMesh(wm_vertices, wm_normals, wm_triangles));
		if (!ret.second) {
			throw std::runtime_error("WalkMesh with duplicated name '" + name + "' in '" + filename + "'");
		}

	}
}

WalkMesh const &WalkMeshes::lookup(std::string const &name) const {
	auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("WalkMesh with name '" + name + "' not found.");
	}
	return f->second;
}