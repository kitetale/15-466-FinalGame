#pragma once
#include "Mode.hpp"
#include "Scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <deque>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <iostream>
#include <map>
#include <algorithm>

#include "TextTextureProgram.hpp"


class TextRenderer {
    public: 
        uint32_t font_size;

        glm::uvec2 drawable_size;

        float margin_percent = 0.04f; // 4% margin
        float space_between_lines = 5.0f;

        FT_Library ft_library;
        FT_Face ft_face;
        FT_Error error;

        FT_GlyphSlot  slot;
        FT_Matrix     matrix;                 /* transformation matrix */
        FT_Vector     pen;                    /* untransformed origin  */

        hb_font_t *hb_font;
        hb_buffer_t *hb_buffer;

        hb_glyph_info_t *info;
        hb_glyph_position_t *pos;

        /// Holds all state information relevant to a character as loaded using FreeType
        struct Character {
            unsigned int TextureID; // ID handle of the glyph texture
            glm::ivec2   Size;      // Size of glyph
            glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
            unsigned int Advance;   // Horizontal offset to advance to next glyph
        };

        std::map<hb_codepoint_t, Character> Characters;
        unsigned int VAO, VBO;

        // constructor and destructor
        TextRenderer(std::string font_file, uint32_t font_size);
        ~TextRenderer();

        // config functions
        void set_drawable_size(const glm::uvec2 &drawable_size);
        void set_margin(float margin_percent);
        void set_space_between_lines(float space_between_lines);

        glm::vec2 get_screen_pos(const glm::vec2 &rel_pos);

        // render a single line
        void renderLine(std::string &line, float x, float y, float scale, glm::vec3 color);

        // render multiple lines separated by newline
        void renderText(std::string text, float x, float y, float scale, glm::vec3 color);
        // render multiple lines stored as text vector
        void renderText(std::vector<std::string> &lines, float x, float y, float scale, glm::vec3 color);
        // add newlines at appropriate positions of a line based on glyph width and screen width
        std::string shapeAndWrapLine(std::string text, float scale);
        void shapeAndWrapLineVector(std::vector<std::string> &text, float scale);

        // render arbitrary text with both manual newlines and automatic wrapping
        void renderWrappedText(std::string text, float y, float scale, glm::vec3 color, bool top_origin=false);
        

};



