#include <mgu/gl.h>
#include <mgu/text.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, mgu_internal_render_text, (const char *str, int *s), {
	function createTextCanvas(text, params) {
		let fontSize = params.fontSize || 8;

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

static PangoLayout *create_layout(const struct mgu_text *text, struct mgu_text_opts opts) {
	PangoLayout *lay = pango_cairo_create_layout(text->ctx);
	pango_layout_set_text(lay, opts.str, -1);
	pango_layout_set_ellipsize(lay, PANGO_ELLIPSIZE_END);

	if (opts.s[0] >= 0) {
		pango_layout_set_width(lay, opts.s[0] * PANGO_SCALE);
	}
	if (opts.s[1] >= 0) {
		pango_layout_set_height(lay, opts.s[1] * PANGO_SCALE);
	}

	if (opts.ch) {
		pango_layout_set_alignment(lay, PANGO_ALIGN_CENTER);
	}

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family_static(desc, "monospace");
	pango_font_description_set_absolute_size(desc, opts.size_px * PANGO_SCALE);
	pango_layout_set_font_description(lay, desc);
	pango_font_description_free(desc);

	return lay;
}

void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]) {
	PangoLayout *lay = create_layout(text, opts);
	pango_layout_get_pixel_size(lay, &s[0], &s[1]);
	g_object_unref(lay);
}

GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]) {
#ifdef __EMSCRIPTEN__
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	mgu_internal_render_text(str, s);
	return tex;
#else
	PangoLayout *lay = create_layout(text, opts);

	PangoRectangle ink_rect;
	pango_layout_get_pixel_extents(lay, &ink_rect, NULL);
	s[0] = ink_rect.x + ink_rect.width;
	if (!opts.cv) {
		s[1] = ink_rect.y + ink_rect.height;
	} else {
		s[1] = ink_rect.y / 2 + opts.s[1] / 2 + ink_rect.height / 2;
	}

	struct mgu_pixel *buffer =
		calloc(s[0] * s[1], sizeof(struct mgu_pixel));
	cairo_surface_t *surf = cairo_image_surface_create_for_data(
		(unsigned char *)buffer,CAIRO_FORMAT_ARGB32,s[0],s[1],4*s[0]);
	cairo_t *cr = cairo_create(surf);

	if (opts.cv) {
		cairo_translate(cr, 0, opts.s[1] / 2 - ink_rect.height / 2 - ink_rect.y);
	}

	// cairo_set_source_rgba(cr, 1, 0, 0, 1);
	// cairo_paint(cr);
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
