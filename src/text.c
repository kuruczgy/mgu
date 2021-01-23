#include <mgu/gl.h>
#include <mgu/text.h>
#include <string.h>
#include <stdlib.h>
#include <platform_utils/main.h>
#include <platform_utils/log.h>

// #define DEBUG_TEXT_BG

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#elif defined(__ANDROID__)
#include <jni.h>
#else
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
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

#if defined(__ANDROID__)
#include <assert.h>
#include <math.h>
#include <ds/iter.h>
#include <ds/vec.h>
struct layout {
	struct vec lines; /* vec<struct str_slice> */
	float width;
};
void init_layout(struct layout *lay, struct mgu_text_opts opts, void *env,
		float (*measure_slice)(void *env, struct str_slice slice)) {
	lay->lines = vec_new_empty(sizeof(struct str_slice));

	float max_width = opts.s[0];

	struct str_slice slice = { .d = opts.str, .len = strlen(opts.str) };
	struct str_gen gen_pars = str_gen_split(slice, '\n');

	struct str_slice par;
	while (str_gen_next(&gen_pars, &par)) {
		struct str_slice current_line = { .d = NULL };

		struct str_gen gen_words = str_gen_split(par, ' ');
		struct str_slice word;
		while (str_gen_next(&gen_words, &word)) {
			if (!current_line.d) {
				current_line = word;
				continue;
			}

			struct str_slice candidate = current_line;
			candidate.len += word.len + 1;

			float width = measure_slice(env, candidate);
			if (max_width < 0.f || width < max_width) {
				current_line = candidate;
			} else {
				vec_append(&lay->lines, &current_line);
				current_line = word;
			}
		}

		assert(current_line.d != NULL);
		vec_append(&lay->lines, &current_line);
	}

	float bb_w = 0.f;
	for (int i = 0; i < lay->lines.len; ++i) {
		struct str_slice line =
			*(struct str_slice *)vec_get(&lay->lines, i);
		float width = measure_slice(env, line);
		bb_w = fmaxf(bb_w, width);
	}

	lay->width = bb_w;
}
#endif

#if defined(__EMSCRIPTEN__)
EM_JS(void, mgu_internal_measure_text, (
		int *s,
		const char *str,
		int sx,
		int sy,
		int size_px,
		bool align_center), {
	const ctx = document.createElement("canvas").getContext("2d");
	ctx.font = size_px + "px monospace";
	let metrics = ctx.measureText(UTF8ToString(str));
	HEAP32[(s >> 2) + 0] = metrics.width;
	HEAP32[(s >> 2) + 1] = size_px;
});
EM_JS(void, mgu_internal_render_text, (
		int *si,
		const char *str,
		int sx,
		int sy,
		int size_px,
		bool align_center), {
	function set_params(ctx) {
		ctx.font = size_px + "px monospace";
		ctx.textAlign = align_center ? "center" : "left";
		ctx.textBaseline = "top";
		ctx.fillStyle = "white";
	}
	// https://stackoverflow.com/a/16599668
	function create_layout(ctx, text) {
		let max_width = sx;

		function get_lines(text) {
			let words = text.split(" ");
			let lines = [];
			let currentLine = words[0];

			for (let i = 1; i < words.length; i++) {
				let word = words[i];
				let width = ctx.measureText(currentLine + " " + word).width;
				if (max_width < 0 || width < max_width) {
					currentLine += " " + word;
				} else {
					lines.push(currentLine);
					currentLine = word;
				}
			}
			lines.push(currentLine);
			return lines;
		}

		let lines = text.split("\n").map(t => get_lines(t)).reduce((a, b) => a.concat(b), []);

		let bb_w = lines.reduce((a, l) =>
			Math.max(a, ctx.measureText(l).width), 0);
		let line_spacing = size_px;
		return {
			lines: lines,
			width: bb_w,
			line_spacing: line_spacing,
			height: line_spacing * lines.length
		};
	}
	function render_lines(ctx, lay) {
		ctx.canvas.width = lay.width;
		ctx.canvas.height = lay.height;
		ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
#ifdef DEBUG_TEXT_BG
		ctx.fillStyle = "#0000FF7F";
		ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
#endif
		set_params(ctx);
		for (let i = 0; i < lay.lines.length; i++) {
			ctx.fillText(
				lay.lines[i],
				align_center ? lay.width / 2 : 0,
				lay.line_spacing * i
			);
		}
	}

	let gl = Module["ctx"];
	const ctx = document.createElement("canvas").getContext("2d");
	set_params(ctx);
	let lay = create_layout(ctx, UTF8ToString(str), sx);
	render_lines(ctx, lay);

	HEAP32[(si >> 2) + 0] = lay.width;
	HEAP32[(si >> 2) + 1] = lay.height;

	// https://stackoverflow.com/a/46225744
	gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);

	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, ctx.canvas);
});
#elif defined(__ANDROID__)

// JNI convenience macros
#define jni_check if ((*env)->ExceptionCheck(env) == JNI_TRUE) goto jni_error
#define jni_find_class(name, path) jclass class_ ## name = \
	(*env)->FindClass(env, path); jni_check
#define jni_find_method(class, name, sig) jmethodID mid_ ## name = \
	(*env)->GetMethodID(env, class_ ## class, #name, sig); jni_check
#define jni_find_static_method(class, name, sig) jmethodID mid_ ## name = \
	(*env)->GetStaticMethodID(env, class_ ## class, #name, sig); jni_check
#define jni_find_ctor(class, sig) jmethodID ctor_ ## class = \
	(*env)->GetMethodID(env, class_ ## class, "<init>", sig); jni_check
#define jni_find_field(class, name, sig) jfieldID fid_ ## name = \
	(*env)->GetFieldID(env, class_ ## class, #name, sig); jni_check
#define jni_find_static_field(class, name, sig) jfieldID fid_ ## name = \
	(*env)->GetStaticFieldID(env, class_ ## class, #name, sig); jni_check

// see:
// https://web.archive.org/web/20120625232734/http://java.sun.com/docs/books/jni/html/other.html
jobject JNU_NewStringNative(JNIEnv *env, struct str_slice slice) {
	jclass Class_java_lang_String = (*env)->FindClass(env, "java/lang/String");
	if (Class_java_lang_String == NULL) return NULL;

	jmethodID MID_String_init = (*env)->GetMethodID(env, Class_java_lang_String, "<init>", "([B)V");
	if (MID_String_init == NULL) return NULL;

	int len = slice.len;
	jbyteArray bytes = (*env)->NewByteArray(env, len);
	if (bytes != NULL) {
		(*env)->SetByteArrayRegion(env, bytes, 0, len, (jbyte *)slice.d);
		jobject result = (*env)->NewObject(env, Class_java_lang_String, MID_String_init, bytes);
		(*env)->DeleteLocalRef(env, bytes);
		return result;
	} /* else fall through */
	return NULL;
}
#else
static PangoLayout *create_pango_layout(const struct mgu_text *text, struct mgu_text_opts opts) {
	PangoLayout *lay = pango_cairo_create_layout(text->ctx);
	pango_layout_set_text(lay, opts.str, -1);
	pango_layout_set_ellipsize(lay, PANGO_ELLIPSIZE_END);

	if (opts.s[0] >= 0) {
		pango_layout_set_width(lay, opts.s[0] * PANGO_SCALE);
	}
	if (opts.s[1] >= 0) {
		pango_layout_set_height(lay, opts.s[1] * PANGO_SCALE);
	}

	if (opts.align_center) {
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

struct mgu_text *mgu_text_create(struct platform *plat) {
	struct mgu_text *text = malloc(sizeof(struct mgu_text));
	*text = (struct mgu_text){ .plat = plat };
#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
	JavaVM *vm = text->plat->app->activity->vm;
	(*vm)->AttachCurrentThread(vm, &text->jni_env, NULL);
#else
	cairo_surface_t *temp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 0, 0);
	text->ctx = cairo_create(temp);
	cairo_surface_destroy(temp);
#endif
	return text;
}
void mgu_text_destroy(struct mgu_text *text) {
#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
	JavaVM *vm = text->plat->app->activity->vm;
	(*vm)->DetachCurrentThread(vm);
#else
	cairo_destroy(text->ctx);
#endif
	free(text);
}

#if defined(__ANDROID__)
struct measure_slice_jni_env {
	JNIEnv *env;
	jobject obj_paint;
	jmethodID mid_measureText;
};
float measure_slice_jni(void *_env, struct str_slice slice) {
	struct measure_slice_jni_env *menv = _env;
	JNIEnv *env = menv->env;

	if ((*env)->PushLocalFrame(env, 8) != 0) goto jni_error;

	jobject obj_str = JNU_NewStringNative(env, slice);
	if (!obj_str) goto jni_error;

	jfloat measuredTextWidth =
		(*env)->CallFloatMethod(env, menv->obj_paint,
		menv->mid_measureText, obj_str); jni_check;

	(*env)->PopLocalFrame(env, NULL);

	return measuredTextWidth;
jni_error:
	return 0;
}
#endif

void mgu_text_measure(const struct mgu_text *text, struct mgu_text_opts opts,
		int s[static 2]) {
#if defined(__EMSCRIPTEN__)
	int si[2];
	mgu_internal_measure_text(
		si,
		opts.str,
		opts.s[0],
		opts.s[1],
		opts.size_px,
		opts.align_center
	);
	s[0] = si[0], s[1] = si[1];
#elif defined(__ANDROID__)
	// TODO
#else
	PangoLayout *lay = create_pango_layout(text, opts);
	int w, h;
	pango_layout_get_pixel_size(lay, &w, &h);
	s[0] = w, s[1] = h;
	g_object_unref(lay);
#endif
}

GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts,
		int s[static 2]) {
#if defined(__EMSCRIPTEN__)
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	int si[2];
	mgu_internal_render_text(
		si,
		opts.str,
		opts.s[0],
		opts.s[1],
		opts.size_px,
		opts.align_center
	);
	s[0] = si[0], s[1] = si[1];
	return tex;
#elif defined(__ANDROID__)
	// see:
	// https://arm-software.github.io/opengl-es-sdk-for-android/high_quality_text.html#highQualityTextIntroduction

	JNIEnv *env = text->jni_env;

	jni_check;

	if ((*env)->PushLocalFrame(env, 32) != 0) goto jni_error;

	jni_find_class(Paint, "android/graphics/Paint");
	jni_find_class(Bitmap, "android/graphics/Bitmap");
	jni_find_class(Bitmap_Config, "android/graphics/Bitmap$Config");
	jni_find_class(Canvas, "android/graphics/Canvas");
	jni_find_class(GLUtils, "android/opengl/GLUtils");

	jni_find_ctor(Paint, "()V");
	jni_find_ctor(Canvas, "(Landroid/graphics/Bitmap;)V");

	jni_find_static_method(GLUtils, texImage2D, "(IIILandroid/graphics/Bitmap;I)V");
	jni_find_static_method(Bitmap, createBitmap, "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	jni_find_method(Paint, setColor, "(I)V");
	jni_find_method(Paint, setTextSize, "(F)V");
	jni_find_method(Paint, measureText, "(Ljava/lang/String;)F");
	jni_find_method(Paint, ascent, "()F");
	jni_find_method(Paint, descent, "()F");
	jni_find_method(Bitmap, eraseColor, "(I)V");
	jni_find_method(Canvas, drawText, "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");

	jni_find_static_field(Bitmap_Config, ARGB_8888, "Landroid/graphics/Bitmap$Config;");

	jobject obj_argb_8888 =
		(*env)->GetStaticObjectField(env, class_Bitmap_Config, fid_ARGB_8888); jni_check;
	jobject obj_paint = (*env)->NewObject(env, class_Paint, ctor_Paint); jni_check;

	(*env)->CallVoidMethod(env, obj_paint, mid_setColor, (jint)0xFFFFFFFF); jni_check;

	jfloat aFontSize = opts.size_px;
	(*env)->CallVoidMethod(env, obj_paint, mid_setTextSize, aFontSize); jni_check;

	jfloat ascent = (*env)->CallFloatMethod(env, obj_paint, mid_ascent); jni_check;
	jfloat descent = (*env)->CallFloatMethod(env, obj_paint, mid_descent); jni_check;

	struct measure_slice_jni_env ms_env = {
		.env = env,
		.obj_paint = obj_paint,
		.mid_measureText = mid_measureText,
	};
	struct layout lay;
	init_layout(&lay, opts, &ms_env, measure_slice_jni);

	float line_spacing = descent - ascent;
	float height = lay.lines.len * line_spacing;

	jint bitmap_w = (int)(lay.width + .5f);
	jint bitmap_h = (int)(height + .5f);
	jobject obj_bitmap = (*env)->CallStaticObjectMethod(env, class_Bitmap,
		mid_createBitmap, bitmap_w, bitmap_h, obj_argb_8888); jni_check;

#ifdef DEBUG_TEXT_BG
	jint color = 0x7F0000FF;
#else
	jint color = 0x00000000;
#endif
	(*env)->CallVoidMethod(env, obj_bitmap, mid_eraseColor, color); jni_check;

	jobject obj_canvas = (*env)->NewObject(env, class_Canvas, ctor_Canvas, obj_bitmap); jni_check;

	for (int i = 0; i < lay.lines.len; ++i) {
		struct str_slice line =
			*(struct str_slice *)vec_get(&lay.lines, i);
		jobject obj_str = JNU_NewStringNative(env, line);
		if (!obj_str) goto jni_error;
		(*env)->CallVoidMethod(env, obj_canvas, mid_drawText,
			obj_str, 0.f, i * line_spacing - ascent,
			obj_paint); jni_check;
		(*env)->DeleteLocalRef(env, obj_str);
	}

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	(*env)->CallStaticVoidMethod(env, class_GLUtils, mid_texImage2D,
		(jint)GL_TEXTURE_2D, (jint)0, (jint)GL_RGBA, obj_bitmap, (jint)0); jni_check;

	// const char *test_res = (*env)->GetStringUTFChars(env, obj_str, NULL); jni_check;
	// pu_log_info("%s test_res: %s\n", __func__, test_res);
	// (*env)->ReleaseStringUTFChars(env, obj_str, test_res);

	(*env)->PopLocalFrame(env, NULL);

	s[0] = bitmap_w, s[1] = bitmap_h;
	return tex;
jni_error:
	return 0;
#else
	PangoLayout *lay = create_pango_layout(text, opts);

	PangoRectangle logical_rect;
	pango_layout_get_pixel_extents(lay, NULL, &logical_rect);

	s[0] = logical_rect.width;
	s[1] = logical_rect.height;

	struct mgu_pixel *buf =
		calloc(s[0] * s[1], sizeof(struct mgu_pixel));
	cairo_surface_t *surf = cairo_image_surface_create_for_data(
		(unsigned char *)buf, CAIRO_FORMAT_ARGB32, s[0], s[1], 4*s[0]);
	cairo_t *cr = cairo_create(surf);

	cairo_translate(cr, -logical_rect.x, -logical_rect.y);

#ifdef DEBUG_TEXT_BG
	cairo_set_source_rgba(cr, 1, 0, 0, 0.5);
	cairo_paint(cr);
#endif
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	pango_cairo_show_layout(cr, lay);

	g_object_unref(lay);
	cairo_destroy(cr);
	cairo_surface_destroy(surf);

	GLuint tex = mgu_tex_mem((struct mgu_pixel *)buf,
		(uint32_t[]){ s[0], s[1] });
	free(buf);

	return tex;
#endif
}
