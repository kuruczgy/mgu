#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <mgu/gl.h>
#include <mgu/text.h>

void mgu_text_init(struct mgu_text *text) {
	cairo_surface_t *temp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 0, 0);
	text->ctx = cairo_create(temp);
	cairo_surface_destroy(temp);
}
void mgu_text_finish(struct mgu_text *text) {
	cairo_destroy(text->ctx);
}

static void get_text_size(PangoLayout *lay, int s[static 2]) {
	pango_layout_get_size(lay, &s[0], &s[1]);
	s[0] /= PANGO_SCALE;
	s[1] /= PANGO_SCALE;
}

GLuint mgu_tex_text(const struct mgu_text *text, const char *str,
		int s[static 2]) {
	PangoLayout *lay = pango_cairo_create_layout(text->ctx);
	pango_layout_set_text(lay, str, -1);

	PangoFontDescription *desc = pango_font_description_from_string(
		"DejaVu Sans Mono 32");
	pango_layout_set_font_description(lay, desc);
	pango_font_description_free(desc);

	get_text_size(lay, s);
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
}
