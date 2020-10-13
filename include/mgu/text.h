#ifndef MGU_TEXT_H
#define MGU_TEXT_H
#include <GLES2/gl2.h>
#include <stdbool.h>

#ifdef __EMSCRIPTEN__
struct mgu_text { };
#else
#include <cairo/cairo.h>
struct mgu_text {
	cairo_t *ctx;
};
#endif

struct mgu_text_opts {
	const char *str;
	int s[2];
	int size_px;
	bool ch, cv;
};

void mgu_text_init(struct mgu_text *text);
void mgu_text_finish(struct mgu_text *text);
void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]);
GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]);


#endif
