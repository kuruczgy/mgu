#ifndef MGU_TEXT_H
#define MGU_TEXT_H
#include <GLES2/gl2.h>
#include <stdbool.h>

struct platform;

struct mgu_text_opts {
	const char *str;
	int s[2];
	int size_px;
	bool align_center;
};

struct mgu_text *mgu_text_create(struct platform *plat);
void mgu_text_destroy(struct mgu_text *text);
void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts,
	int s[static 2]);
GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts,
	int s[static 2]);


#endif
