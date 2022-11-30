#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include "GL.hpp"
#include <glm/glm.hpp>
#include <map>
#include <algorithm>

using namespace std;
//This file exists to check that programs that use freetype / harfbuzz link properly in this base code.
//You probably shouldn't be looking here to learn to use either library.

#define FONT_SIZE 36

#define WIDTH   160
#define HEIGHT  120

/* origin is the upper left corner */
unsigned char image[HEIGHT][WIDTH];


void
draw_bitmap( FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y)
{
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;

  /* for simplicity, we assume that `bitmap->pixel_mode' */
  /* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */

  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
      if ( i < 0      || j < 0       ||
           i >= WIDTH || j >= HEIGHT )
        continue;

      image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}


void
show_image( void )
{
  int  i, j;
  for ( i = 0; i < HEIGHT; i++ )
  {
    for ( j = 0; j < WIDTH; j++ )
      putchar( image[i][j] == 0 ? ' '
                                : image[i][j] < 128 ? '+'
                                                    : '*' );
    putchar( '\n' );
  }
}

int main(int argc, char **argv) {

	if (argc < 3)
	{
		cout << "usage: hello-harfbuzz font-file.ttf text" << std::endl;
		return 1;
	}

	const char *fontfile;
  	const char *text;

	fontfile = argv[1];
	text = argv[2];

	/* Initialize FreeType and create FreeType font face. */
	FT_Library ft_library;
	FT_Face ft_face;

	FT_Init_FreeType( &ft_library );
	FT_New_Face (ft_library, fontfile, 0, &ft_face);
	FT_Set_Char_Size(ft_face, FONT_SIZE * 64, FONT_SIZE * 64, 0, 0);

	/* Create hb-ft font. */
	hb_font_t *hb_font;
	hb_font = hb_ft_font_create (ft_face, NULL);


	hb_buffer_t *hb_buffer = hb_buffer_create();
	hb_buffer_add_utf8 (hb_buffer, text, -1, 0, -1);
  	hb_buffer_guess_segment_properties (hb_buffer);	

	/* Shape it! */
  	hb_shape (hb_font, hb_buffer, NULL, 0);


	/* Get glyph information and positions out of the buffer. */
	unsigned int len = hb_buffer_get_length (hb_buffer);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);

	/* Print them out as is. */
	cout << "Raw buffer contents:" << endl;
	for (unsigned int i = 0; i < len; i++)
	{
		hb_codepoint_t gid   = info[i].codepoint;
		unsigned int cluster = info[i].cluster;
		double x_advance = pos[i].x_advance / 64.;
		double y_advance = pos[i].y_advance / 64.;
		double x_offset  = pos[i].x_offset / 64.;
		double y_offset  = pos[i].y_offset / 64.;

		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf  ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
	}

	printf ("Converted to absolute positions:\n");
	/* And converted to absolute positions. */
	{
		double current_x = 0;
		double current_y = 0;
		for (unsigned int i = 0; i < len; i++)
		{
		hb_codepoint_t gid   = info[i].codepoint;
		unsigned int cluster = info[i].cluster;
		double x_position = current_x + pos[i].x_offset / 64.;
		double y_position = current_y + pos[i].y_offset / 64.;


		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf ("glyph='%s'	cluster=%d	position=(%g,%g)\n",
			glyphname, cluster, x_position, y_position);

		current_x += pos[i].x_advance / 64.;
		current_y += pos[i].y_advance / 64.;
		}
	}
	FT_Error error;
	
	FT_GlyphSlot  slot;
	FT_Matrix     matrix;                 /* transformation matrix */
	FT_Vector     pen;                    /* untransformed origin  */
	
	double        angle;
	int           target_height;
	int           n;
	size_t num_chars;

	num_chars     = strlen( text );
	angle         = 0;
	target_height = HEIGHT;

	slot = ft_face->glyph;

	  /* set up matrix */
	matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L );
	matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
	matrix.yx = (FT_Fixed)( sin( angle ) * 0x10000L );
	matrix.yy = (FT_Fixed)( cos( angle ) * 0x10000L );

	pen.x = 0;
  	pen.y = 0;

	for ( n = 0; n < num_chars; n++ )
	{
		/* set transformation */
		FT_Set_Transform( ft_face, &matrix, &pen );

		/* load glyph image into the slot (erase previous one) */
		error = FT_Load_Glyph( ft_face, info[n].codepoint, FT_LOAD_RENDER );
		if ( error )
		continue;                 /* ignore errors */

		/* now, draw to our target surface (convert position) */
		draw_bitmap( &slot->bitmap,
					slot->bitmap_left,
					target_height - slot->bitmap_top );

		/* increment pen position */
		pen.x += pos[n].x_advance;
		pen.y += pos[n].y_advance;
	}

	show_image();

	FT_Done_Face    ( ft_face );
	FT_Done_FreeType( ft_library );



	hb_buffer_destroy(hb_buffer);

	printf("It worked?\n");
}