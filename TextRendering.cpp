#include "TextRendering.hpp"

#include <sstream>

TextRenderer::TextRenderer(std::string font_file, uint32_t font_size) {
    this->font_size = font_size;
    // initialize freetype
    // based on https://www.freetype.org/freetype2/docs/tutorial/step1.html
    error = FT_Init_FreeType( &ft_library );
    if (error) {
        throw std::runtime_error("FT_Init_FreeType failed");
    }
    error = FT_New_Face (ft_library, font_file.c_str(), 0, &ft_face);
    if (error) {
        throw std::runtime_error("FT_New_Face failed");
    }
    error = FT_Set_Char_Size (ft_face, font_size * 64, font_size * 64, 0, 0);
    if (error) {
        throw std::runtime_error("FT_Set_Char_Size failed");
    }
    // create hb-ft font
    hb_font = hb_ft_font_create(ft_face, NULL);
}

TextRenderer::~TextRenderer() {
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
}

// configure text renderer

// set drawable size
void TextRenderer::set_drawable_size(const glm::uvec2 &size) {
    this->drawable_size = size;
}
// set margin, used for auto wrapping
void TextRenderer::set_margin(float margin) {
    this->margin_percent = margin;
}

// set space between lines
void TextRenderer::set_space_between_lines(float space) {
    this->space_between_lines = space;
}

// convert relative position to screen position based on drawable size
glm::vec2 TextRenderer::get_screen_pos(const glm::vec2 &rel_pos) {
    glm::vec2 screen_pos;
    screen_pos.x = rel_pos.x * drawable_size.x;
    screen_pos.y = rel_pos.y * drawable_size.y;
    return screen_pos;
}



// render one line of text using OpenGL
// heavily based on following tutorials with some modifications to use glyphs instead of chars:
// https://learnopengl.com/In-Practice/Text-Rendering
// https://www.freetype.org/freetype2/docs/tutorial/step1.html
// https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
void TextRenderer::renderLine(std::string &line, float x, float y, float scale, glm::vec3 color) {
    // OpenGL state
	// ------------
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST); 
	// disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // start of hb tutorial code 

    // parse text using Harfbuzz
    // create hb buffer
    hb_buffer = hb_buffer_create();

    hb_buffer_add_utf8(hb_buffer, line.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(hb_buffer);
    hb_shape(hb_font, hb_buffer, NULL, 0);

    /* Get glyph information and positions out of the buffer. */
	info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
	pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);
    // end of hb tutorial code

    // start of OpenGL text rendering tutorial code

    // set up shader
    glUseProgram(text_texture_program->program);
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(drawable_size.x), 0.0f, static_cast<float>(drawable_size.y));
    glUniformMatrix4fv(text_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(projection));
    
    // set size to load glyphs as
    FT_Set_Pixel_Sizes(ft_face, 0, font_size);

    // add glyphs to character map, if not present
    uint32_t glyph_len = hb_buffer_get_length (hb_buffer);
    for ( uint32_t i = 0; i < glyph_len; i++)
    {
        // get glyph
        hb_codepoint_t glyph_cp = info[i].codepoint;
        // find glyph in characters map, if not found, load it
        if (Characters.find(glyph_cp) == Characters.end()) {
            // load glyph
            if (FT_Load_Glyph(ft_face, glyph_cp, FT_LOAD_RENDER)) {
                throw std::runtime_error("FREETYTPE: Failed to load Glyph");
            }
            
            // generate texture and dynamically add to character map
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                ft_face->glyph->bitmap.width,
                ft_face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                ft_face->glyph->bitmap.buffer
            );
            // set texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // now store character for later use
            Character character = {
                texture,
                glm::ivec2(ft_face->glyph->bitmap.width, ft_face->glyph->bitmap.rows),
                glm::ivec2(ft_face->glyph->bitmap_left, ft_face->glyph->bitmap_top),
                static_cast<unsigned int>(ft_face->glyph->advance.x)
            };
            Characters.insert(std::pair<hb_codepoint_t, Character>(glyph_cp, character));
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // configure VAO/VBO for texture quads
    // -----------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // -------------------------

    glUniform3f(text_texture_program->TextColor_vec3, color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    for ( uint32_t i = 0; i < glyph_len; i++)
    {
        // get glyph and character buffer entry
        hb_codepoint_t glyph_cp = info[i].codepoint;
        Character ch = Characters[glyph_cp];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    // end of openGL text rendering tutorial code

    hb_buffer_destroy(hb_buffer);
}

// render multiline text, separated at newlines
void TextRenderer::renderText(std::string text, float x, float y, float scale, glm::vec3 color) {
    // set spacing according to font size
    float spacing = font_size * scale + space_between_lines;

    // split text into lines
    std::vector<std::string> lines;
    std::string line;
    std::istringstream iss(text);
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    // render each line
    for (uint32_t i = 0; i < lines.size(); i++) {
        renderLine(lines[i], x, y + (lines.size() - 1 - i) * spacing, scale, color);
    }
}

void TextRenderer::renderText(std::vector<std::string> &lines, float x, float y, float scale, glm::vec3 color) {
    // set spacing according to font size
    float spacing = font_size * scale + space_between_lines;
    // render each line in text vector
    for (uint32_t i = 0; i < lines.size(); i++) {
        renderLine(lines[i], x, y + (lines.size() - 1 - i) * spacing, scale, color);
    }
}

std::string TextRenderer::shapeAndWrapLine(std::string text, float scale) {
    // return empty string if text is empty
    if (text.empty()) {
        return "";
    }

    // calculate start and end positions of text
    float x_start = drawable_size.x * margin_percent;
    float x_end = drawable_size.x * (1.0f - margin_percent);

    // shape text 
    hb_buffer_t* temp_hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(temp_hb_buffer, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(temp_hb_buffer);
    hb_shape(hb_font, temp_hb_buffer, NULL, 0);

    /* Get glyph positions out of the buffer. */
	hb_glyph_position_t *temp_pos = hb_buffer_get_glyph_positions (temp_hb_buffer, NULL);

    // find length of string that can fit on one line
    float current_pos = x_start;
    size_t last_fitting_space_pos = 0;
    for (size_t i = 0; i < hb_buffer_get_length(temp_hb_buffer); i++) {
        current_pos += temp_pos[i].x_advance / 64.0f * scale;
        // leave loop if over the margin
        if (current_pos > x_end) {
            hb_buffer_destroy(temp_hb_buffer);
            // insert new line after last fitting space, and wrap rest of text
            return text.substr(0, last_fitting_space_pos) + "\n" + shapeAndWrapLine(text.substr(last_fitting_space_pos + 1), scale);
        }
        // save position of last fitting space
        if (text.at(i) == ' ') {
            last_fitting_space_pos = i;
        }
    } 

    // getting here means everything fits in one line
    hb_buffer_destroy(temp_hb_buffer);
    return text;

}

void TextRenderer::shapeAndWrapLineVector(std::vector<std::string> &text, float scale) {
    // return empty string if text is empty
    if (text.empty()) {
        return;
    }

    // calculate start and end positions of text
    float x_start = drawable_size.x * margin_percent;
    float x_end = drawable_size.x * (1.0f - margin_percent);
    
    // get last element of vector
    std::string line = text.back();

    // shape text 
    hb_buffer_t* temp_hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(temp_hb_buffer, line.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(temp_hb_buffer);
    hb_shape(hb_font, temp_hb_buffer, NULL, 0);

    /* Get glyph positions out of the buffer. */
	hb_glyph_position_t *temp_pos = hb_buffer_get_glyph_positions (temp_hb_buffer, NULL);

    // find length of string that can fit on one line
    float current_pos = x_start;
    size_t last_fitting_space_pos = 0;
    for (size_t i = 0; i < hb_buffer_get_length(temp_hb_buffer); i++) {
        current_pos += temp_pos[i].x_advance / 64.0f * scale;
        // leave loop if over the margin
        if (current_pos > x_end) {
            hb_buffer_destroy(temp_hb_buffer);

            // getting here means we have to separate the current line

            // remove current line from vector
            text.pop_back();

            // separate wrapped and unwrapped text into two string, putting the unwrapped text into the back of the vector
            std::string wrapped = line.substr(0, last_fitting_space_pos);
            text.push_back(wrapped);
            std::string rest = line.substr(last_fitting_space_pos + 1);
            text.push_back(rest);

            // recursively call function to wrap last element, will return when done
            shapeAndWrapLineVector(text, scale);
            return;
        }
        // save position of last fitting space
        if (line.at(i) == ' ') {
            last_fitting_space_pos = i;
        }
    } 

    // getting here means everything fits in one line
    hb_buffer_destroy(temp_hb_buffer);
    // leave function, since last element is wrapped
    return;
}

void TextRenderer::renderWrappedText(std::string text, float y, float scale, glm::vec3 color, bool top_origin) {

    // prepare vector
    std::vector<std::string> wrapped_lines;

    // split text into lines
    std::string line;

    std::istringstream iss(text);
    while (std::getline(iss, line)) {
        // create a vector for line to feed to text wrap function
        std::vector<std::string> line_vector;
        line_vector.push_back(line);
        shapeAndWrapLineVector(line_vector, scale);
        // add wrapped lines to total vector
        for (uint32_t i = 0; i < line_vector.size(); i++) {
            wrapped_lines.push_back(line_vector[i]);
        }
    }
    
    // find offset for top origin
    if (top_origin) {
        size_t num_lines = wrapped_lines.size();
        y = drawable_size.y - y - (num_lines * (font_size * scale + space_between_lines));
        // bounds protection
        if (y < 0) {
            y = 0;
        }
        if (y > drawable_size.y) {
            y = static_cast<float>(drawable_size.y);
        }
    }

    float x_start = drawable_size.x * margin_percent;

    // render text
    renderText(wrapped_lines, x_start, y, scale, color);
}