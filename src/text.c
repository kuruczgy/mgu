#include <mgu/gl.h>
#include <mgu/text.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, mgu_internal_measure_text, (
		int *s,
		const char *str,
		int sx,
		int sy,
		int size_px,
		bool ch,
		bool cv), {
	const ctx = document.createElement("canvas").getContext("2d");
	ctx.font = size_px + "px monospace";
	let metrics = ctx.measureText(UTF8ToString(str));
	HEAP32[s >> 2] = metrics.width;
	HEAP32[(s >> 2) + 1] = size_px;
});
EM_JS(void, mgu_internal_render_text, (
		int *s,
		const char *str,
		int sx,
		int sy,
		int size_px,
		bool ch,
		bool cv), {
	function set_params(ctx) {
		ctx.font = size_px + "px monospace";
		ctx.textAlign = ch ? "center" : "left";
		ctx.textBaseline = "top";
		ctx.fillStyle = "black";
	}
	// https://stackoverflow.com/a/16599668
	function get_lines(ctx, text, max_width) {
		let words = text.split(" ");
		let lines = [];
		let currentLine = words[0];

		for (let i = 1; i < words.length; i++) {
			let word = words[i];
			let width = ctx.measureText(currentLine + " " + word).width;
			if (width < max_width) {
				currentLine += " " + word;
			} else {
				lines.push(currentLine);
				currentLine = word;
			}
		}
		lines.push(currentLine);
		return lines;
	}
	function render_lines(ctx, lines) {
		let line_spacing = size_px;
		ctx.canvas.width = sx;
		ctx.canvas.height = sy;
		let bb_h = line_spacing * lines.length;
		ctx.fillStyle = "#00000000";
		ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
		set_params(ctx);
		for (let i = 0; i < lines.length; i++) {
			ctx.fillText(
				lines[i],
				ch ? sx / 2 : 0,
				line_spacing * i + (cv ? sy / 2 - bb_h / 2 : 0)
			);
		}
	}

	let gl = Module["ctx"];
	const ctx = document.createElement("canvas").getContext("2d");
	let lines = get_lines(ctx, UTF8ToString(str), sx);
	render_lines(ctx, lines);
	HEAP32[s >> 2] = ctx.canvas.width;
	HEAP32[(s >> 2) + 1] = ctx.canvas.height;
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, ctx.canvas);
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

#ifdef __EMSCRIPTEN__
#else
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
#endif

void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]) {
#ifdef __EMSCRIPTEN__
	mgu_internal_measure_text(
		s,
		opts.str,
		opts.s[0],
		opts.s[1],
		opts.size_px,
		opts.ch,
		opts.cv
	);
#else
	PangoLayout *lay = create_layout(text, opts);
	pango_layout_get_pixel_size(lay, &s[0], &s[1]);
	g_object_unref(lay);
#endif
}

GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]) {
#ifdef __EMSCRIPTEN__
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	mgu_internal_render_text(
		s,
		opts.str,
		opts.s[0],
		opts.s[1],
		opts.size_px,
		opts.ch,
		opts.cv
	);
	return tex;
#else
	PangoLayout *lay = create_layout(text, opts);

	PangoRectangle logical_rect;
	pango_layout_get_pixel_extents(lay, NULL, &logical_rect);
	s[0] = logical_rect.x + logical_rect.width;
	if (!opts.cv) {
		s[1] = logical_rect.y + logical_rect.height;
	} else {
		s[1] = logical_rect.y / 2 + opts.s[1] / 2 + logical_rect.height / 2;
	}

	struct mgu_pixel *buffer =
		calloc(s[0] * s[1], sizeof(struct mgu_pixel));
	cairo_surface_t *surf = cairo_image_surface_create_for_data(
		(unsigned char *)buffer,CAIRO_FORMAT_ARGB32,s[0],s[1],4*s[0]);
	cairo_t *cr = cairo_create(surf);

	if (opts.cv) {
		cairo_translate(cr, 0, opts.s[1] / 2 - logical_rect.height / 2 - logical_rect.y);
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
	free(buffer);

	return tex;
#endif
}
