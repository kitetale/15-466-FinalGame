#include "RectangleMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>


GLuint proto_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > proto_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("proto.pnct"));
	proto_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > proto_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("proto.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = proto_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = proto_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > proto_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("proto.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

RectangleMode::RectangleMode() : scene(*rectangle_scene) {
    // MESH & WALKMESH SETUP ---------------------------------------------------
    {
        //create a player transform:
        scene.transforms.emplace_back();
        player.transform = &scene.transforms.back();

        //get mesh of rectangle character
        for (auto &transform : scene.transforms) {
            if (transform.name == "rectangle_character") character.character_transform = &transform;
        }

        //create a player camera attached to a child of the player transform:
        scene.transforms.emplace_back();
        scene.cameras.emplace_back(&scene.transforms.back());
        character.camera = &scene.cameras.back();
        character.camera->fovy = glm::radians(90.0f);
        character.camera->near = 0.01f;
        character.camera->transform->position = glm::vec3(0.0f, -4.0f, 7.0f);
        
        player.transform->position = glm::vec3(0.0f,0.0f,0.0f);

        //rotate camera facing direction (-z) to player facing direction (+y):
        character.camera->transform->rotation = glm::angleAxis(glm::radians(50.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        //start player walking at nearest walk point:
        player.at = walkmesh->nearest_walk_point(player.transform->position);
    }

}

glm::mat4 rotateRectangle(glm::vec2 dir) {
    return glm::mat4(0.f);
}

RectangleMode::~RectangleMode() {
}

bool::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	return false;
}



void RectangleMode::update(float elapsed) {
	
    // //slowly rotates through [0,1):
	// wobble += elapsed / 10.0f;
	// wobble -= std::floor(wobble);

	// rotate rectangle
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 dir = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) dir.x =-1.0f;
		if (!left.pressed && right.pressed) dir.x = 1.0f;
		if (down.pressed && !up.pressed) dir.y =-1.0f;
		if (!down.pressed && up.pressed) dir.y = 1.0f;

		// //make it so that moving diagonally doesn't go faster:
		// if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

        glm::mat4 rotateRect = rotateRectangle(glm)

		//get move in world coordinate system:
		glm::vec3 remain =  player.transform->make_local_to_world() * glm::vec4(move.x, move.y, 0.0f, 0.0f);

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

		// update character mesh's position to respect walking
		character.character_transform->position = walkmesh->to_world_point(player.at);
		// update bounding box info based on current new pos
		// we'll disregard orientation since it's a small diff and our box is 'nice'
		minBound.x = character.character_transform->position.x - halfDim.x;
		maxBound.x = character.character_transform->position.x + halfDim.x;
		minBound.y = character.character_transform->position.y - halfDim.y;
		maxBound.y = character.character_transform->position.y + halfDim.y;
		minBound.z = character.character_transform->position.z - halfDim.z;
		maxBound.z = character.character_transform->position.z + halfDim.z;


		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);

			// update characte mesh's rotation 
			character.character_transform->rotation = glm::normalize(adjust * character.character_transform->rotation);
		}

		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void RectangleMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	character.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

    //set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*character.camera);

	GL_ERRORS();
}