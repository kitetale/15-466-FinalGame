#pragma once

/*
 * Helper that sets up a vertex array object for a given set of attributes
 * and a provided program. Will throw if program references attributes not
 * included in the passed attribs map.
 *
 */

#include "GL.hpp"
#include <map>
#include <string>

//These 'Attrib' structures describe the location of various attributes within the buffer (in exactly format wanted by glVertexAttribPointer):
struct Attrib {
	GLuint buffer = 0;
	GLint size = 0;
	GLenum type = 0;
	//which flavor of glVertexAttribPointer to use:
	enum Interpretation : uint8_t {
		AsFloat, //glVertexAttribPointer with normalized as GL_FALSE
		AsFloatFromFixedPoint, //glVertexAttribPointer with normalized as GL_TRUE
		AsInteger //glVertexAttribIPointer
	} interpretation = AsFloat;
	GLsizei stride = 0;
	GLsizei offset = 0;

	//constructors:
	Attrib() = default;
	Attrib(GLuint buffer_, GLint size_, GLenum type_, Interpretation interpretation_, GLsizei stride_, GLsizei offset_)
	: buffer(buffer_), size(size_), type(type_), interpretation(interpretation_), stride(stride_), offset(offset_) { }

	//This helper calls the proper "glVertexAttribPointer" variant:
	// n.b. it is an error to call this is buffer is zero
	void VertexAttribPointer(GLint location) const;
	//it is named with capital letters to mimic the OpenGL call it invokes.
	// ...I definitely thought about this a fair amount and I'm not really happy
	//   with it, but I also don't like the alternatives.
};

//Given a list of attribs, set proper locations for a shader program.
// note: it is an error to pass a 'null' pointer in attribs.
GLuint make_vao_for_program(std::map< std::string, Attrib const * > const &attribs, GLuint program);