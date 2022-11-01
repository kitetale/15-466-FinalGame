#include "make_vao_for_program.hpp"

#include <cassert>
#include <set>
#include <stdexcept>

void Attrib::VertexAttribPointer(GLint location) const {
	//store old buffer binding:
	GLint old_binding = 0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_binding);

	//bind Attrib's buffer:
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	if (interpretation == AsFloat) {
		glVertexAttribPointer(location, size, type, GL_FALSE, stride, (GLbyte *)0 + offset);
	} else if (interpretation == AsFloatFromFixedPoint) {
		glVertexAttribPointer(location, size, type, GL_TRUE, stride, (GLbyte *)0 + offset);
	} else if (interpretation == AsInteger) {
		glVertexAttribIPointer(location, size, type, stride, (GLbyte *)0 + offset);
	} else {
		assert(0 && "Invalid interpretation.");
	}

	//restore buffer binding:
	glBindBuffer(GL_ARRAY_BUFFER, old_binding);
}


GLuint make_vao_for_program(std::map< std::string, Attrib const * > const &attribs, GLuint program) {
	//don't mess up the current vao binding:
	GLint old_vao = 0;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);

	//create a new vertex array object:
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//Try to bind all attributes:
	std::set< GLuint > bound;
	for (auto &name_attrib : attribs) {
		std::string const &name = name_attrib.first;
		assert(name_attrib.second && "It is an error to pass a null pointer to 'attribs'.");
		Attrib const &attrib = *name_attrib.second;

		if (attrib.buffer == 0) continue; //don't bind attribs referencing no buffer
		if (attrib.size == 0) continue; //don't bind empty attribs

		GLint location = glGetAttribLocation(program, name.c_str());
		if (location == -1) continue; //can't bind missing attribs

		//bind the attribute to the location:

		//call glVertexAttribPointer (or integer variant):
		attrib.VertexAttribPointer(location);
		glEnableVertexAttribArray(location);

		//remember that it was bound:
		bound.insert(location);
	}

	//restore old vertex array binding:
	glBindVertexArray(old_vao);

	//Check that all active attributes were bound:
	GLint active = 0;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &active);
	assert(active >= 0 && "Doesn't makes sense to have negative active attributes.");
	for (GLuint i = 0; i < GLuint(active); ++i) {
		GLchar name[100];
		GLint size = 0;
		GLenum type = 0;
		glGetActiveAttrib(program, i, 100, NULL, &size, &type, name);
		name[99] = '\0';
		GLint location = glGetAttribLocation(program, name);
		if (!bound.count(GLuint(location))) {
			throw std::runtime_error("ERROR: active attribute '" + std::string(name) + "' in program is not bound.");
		}
	}

	return vao;

}