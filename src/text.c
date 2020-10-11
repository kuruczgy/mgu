#include <mgu/gl.h>
#include <mgu/text.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, mgu_internal_render_text, (const char *str, int *s), {
	function createTextCanvas(text, params) {
		let fontSize = params.fontSize || 32;

		const ctx = document.createElement("canvas").getContext("2d");

		ctx.font = fontSize + "px monospace";
		const textMetrics = ctx.measureText(text);
		let width = textMetrics.width;
		let height = fontSize;
		params.width = ctx.canvas.width = width;
		params.height = ctx.canvas.height = height;

		ctx.font = fontSize + "px monospace";
		ctx.textAlign = params.align || "center" ;
		ctx.textBaseline = params.baseline || "middle";
		ctx.fillStyle = params.color || "black";
		ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
		ctx.fillText(text, width / 2, height / 2);
		return ctx.canvas;
	}

	let gl = Module["ctx"];

	params = {};
	let textCanvas = createTextCanvas(UTF8ToString(str), params);
	HEAP32[s >> 2] = params.width;
	HEAP32[(s >> 2) + 1] = params.height;
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, textCanvas);
});
#else
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#endif

void mgu_text_init(struct mgu_text *text) {
#ifdef __EMSCRIPTEN__
#else
	cairo_surface_t *temp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 0, 0);
	text->ctx = cairo_create(temp);
	cairo_surface_destroy(temp);
#endif
}
void mgu_text_finish(struct mgu_text *text) {
#ifdef __EMSCRIPTEN__
#else
	cairo_destroy(text->ctx);
#endif
}

GLuint mgu_tex_text(const struct mgu_text *text, const char *str,
		int s[static 2]) {
#ifdef __EMSCRIPTEN__
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	mgu_internal_render_text(str, s);
	return tex;
#else
	PangoLayout *lay = pango_cairo_create_layout(text->ctx);
	pango_layout_set_text(lay, str, -1);

	PangoFontDescription *desc = pango_font_description_from_string(
		"DejaVu Sans Mono 32");
	pango_layout_set_font_description(lay, desc);
	pango_font_description_free(desc);

	pango_layout_get_pixel_size(lay, &s[0], &s[1]);
	struct mgu_pixel *buffer =
		calloc(s[0] * s[1], sizeof(struct mgu_pixel));
	cairo_surface_t *surf = cairo_image_surface_create_for_data(
		(unsigned char *)buffer,CAIRO_FORMAT_ARGB32,s[0],s[1],4*s[0]);
	cairo_t *cr = cairo_create(surf);

	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	pango_cairo_show_layout(cr, lay);

	g_object_unref(lay);
	cairo_destroy(cr);
	cairo_surface_destroy(surf);

	GLuint tex = mgu_tex_mem((struct mgu_pixel *)buffer,
		(uint32_t[]){ s[0], s[1] });
	return tex;
#endif
}
