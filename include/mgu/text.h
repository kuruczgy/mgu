#ifndef MGU_TEXT_H
#define MGU_TEXT_H
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <platform_utils/main.h>
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#include <cairo/cairo.h>
#endif

struct mgu_text {
	struct platform *plat;
#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
	JNIEnv *jni_env;
#else
	cairo_t *ctx;
#endif
};

struct mgu_text_opts {
	const char *str;
	int s[2];
	int size_px;
	bool ch, cv;
};

void mgu_text_init(struct mgu_text *text, struct platform *plat);
void mgu_text_finish(struct mgu_text *text);
void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]);
GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]);


#endif
