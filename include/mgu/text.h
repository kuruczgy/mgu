#ifndef MGU_TEXT_H
#define MGU_TEXT_H
#include <GLES2/gl2.h>

#ifdef __EMSCRIPTEN__
struct mgu_text { };
#else
#include <cairo/cairo.h>
struct mgu_text {
	cairo_t *ctx;
};
#endif

void mgu_text_init(struct mgu_text *text);
void mgu_text_finish(struct mgu_text *text);
GLuint mgu_tex_text(const struct mgu_text *text, const char *str,
	int s[static 2]);


#endif
