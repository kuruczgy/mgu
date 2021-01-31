#ifndef MGU_GL_H
#define MGU_GL_H
#include <GLES2/gl2.h>

struct mgu_pixel { GLubyte r, g, b, a; };
_Static_assert(sizeof(struct mgu_pixel) == 4, "");

struct mgu_texture {
	GLuint tex;
	uint32_t s[2];
};

struct mgu_texture mgu_tex_farbfeld(const char *filename);
struct mgu_texture mgu_texture_create_from_mem(struct mgu_pixel *data,
	uint32_t s[static 2]);
void mgu_texture_destroy(struct mgu_texture *texture);

/* attribs is termiated using an entry with a NULL name */
GLuint mgu_shader_program(const GLchar *vert, const GLchar *frag);

/* shaders */

struct mgu_shader {
	GLuint prog;

	GLuint a_pos; /* vec2 */
	GLuint a_tex; /* vec2 */

	GLuint u_mat; /* mat4 */
	union {
		struct {
			GLuint u_color; /* vec4 */
		} color;
		struct {
			GLuint u_tex; /* texture2D */
		} tex;
	};
};

void mgu_shader_init_color(struct mgu_shader *s);

/* attribute vec2 pos is the position,
 * uniform mat3 tran is the transform
 * attribute vec2 tex is the texture coordinate */
extern const GLchar mgu_shader_vert_simple[];

/* Outputs the vec4 color uniform. */
extern const GLchar mgu_shader_frag_color[];

extern const GLchar mgu_shader_frag_tex[];


#endif
