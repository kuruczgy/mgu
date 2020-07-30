#ifndef MGU_RENDER_H
#define MGU_RENDER_H
#include <GLES2/gl2.h>

/* attribs is termiated using an entry with a NULL name */
GLuint mgu_shader_program(const GLchar *vert, const GLchar *frag);

/* shaders */

/* attribute vec2 pos is the position,
 * uniform mat3 tran is the transform  */
extern const GLchar mgu_shader_vert_simple[];

/* Outputs the vec4 color uniform. */
extern const GLchar mgu_shader_frag_color[];


#endif
