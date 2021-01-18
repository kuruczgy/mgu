#include <mgu/gl.h>
#include <mgu/text.h>
#include <string.h>
#include <platform_utils/log.h>

#if defined(__EMSCRIPTEN__)
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
		ctx.fillStyle = "white";
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
		ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
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

	// https://stackoverflow.com/a/46225744
	gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);

	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, ctx.canvas);
});
#elif defined(__ANDROID__)
// see:
// https://web.archive.org/web/20120625232734/http://java.sun.com/docs/books/jni/html/other.html
jobject JNU_NewStringNative(JNIEnv *env, const char *str) {
	jclass Class_java_lang_String = (*env)->FindClass(env, "java/lang/String");
	if (Class_java_lang_String == NULL) return NULL;

	jmethodID MID_String_init = (*env)->GetMethodID(env, Class_java_lang_String, "<init>", "([B)V");
	if (MID_String_init == NULL) return NULL;

	int len = strlen(str);
	jbyteArray bytes = (*env)->NewByteArray(env, len);
	if (bytes != NULL) {
		(*env)->SetByteArrayRegion(env, bytes, 0, len, (jbyte *)str);
		jobject result = (*env)->NewObject(env, Class_java_lang_String, MID_String_init, bytes);
		(*env)->DeleteLocalRef(env, bytes);
		return result;
	} /* else fall through */
	return NULL;
}
#include <jni.h>
#else
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#endif

void mgu_text_init(struct mgu_text *text, struct platform *plat) {
	text->plat = plat;
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
}
void mgu_text_finish(struct mgu_text *text) {
#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
	JavaVM *vm = text->plat->app->activity->vm;
	(*vm)->DetachCurrentThread(vm);
#else
	cairo_destroy(text->ctx);
#endif
}

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
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
#if defined(__EMSCRIPTEN__)
	mgu_internal_measure_text(
		s,
		opts.str,
		opts.s[0],
		opts.s[1],
		opts.size_px,
		opts.ch,
		opts.cv
	);
#elif defined(__ANDROID__)
	// TODO
#else
	PangoLayout *lay = create_layout(text, opts);
	pango_layout_get_pixel_size(lay, &s[0], &s[1]);
	g_object_unref(lay);
#endif
}

GLuint mgu_tex_text(const struct mgu_text *text, struct mgu_text_opts opts, int s[static 2]) {
#if defined(__EMSCRIPTEN__)
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
#elif defined(__ANDROID__)
	// see:
	// https://arm-software.github.io/opengl-es-sdk-for-android/high_quality_text.html#highQualityTextIntroduction
#define check if ((*env)->ExceptionCheck(env) == JNI_TRUE) goto err
	JNIEnv *env = text->jni_env;

	check;

	if ((*env)->PushLocalFrame(env, 32) != 0) goto err;

	jobject obj_str = JNU_NewStringNative(env, opts.str);
	if (!obj_str) goto err;

	jclass class_Paint = (*env)->FindClass(env, "android/graphics/Paint"); check;
	jclass class_Bitmap = (*env)->FindClass(env, "android/graphics/Bitmap"); check;
	jclass class_Bitmap_Config = (*env)->FindClass(env, "android/graphics/Bitmap$Config"); check;
	jclass class_Canvas = (*env)->FindClass(env, "android/graphics/Canvas"); check;
	jclass class_GLUtils = (*env)->FindClass(env, "android/opengl/GLUtils"); check;

	jmethodID ctor_Paint = (*env)->GetMethodID(env, class_Paint, "<init>", "()V"); check;
	jmethodID mid_setARGB = (*env)->GetMethodID(env, class_Paint, "setARGB", "(IIII)V"); check;
	jmethodID mid_setTextSize = (*env)->GetMethodID(env, class_Paint, "setTextSize", "(F)V"); check;
	jmethodID mid_measureText = (*env)->GetMethodID(env, class_Paint, "measureText",
		"(Ljava/lang/String;)F"); check;
	jmethodID mid_createBitmap = (*env)->GetStaticMethodID(env, class_Bitmap,
		"createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"); check;
	jmethodID mid_eraseColor = (*env)->GetMethodID(env, class_Bitmap, "eraseColor", "(I)V"); check;
	jmethodID ctor_Canvas = (*env)->GetMethodID(env, class_Canvas,
		"<init>", "(Landroid/graphics/Bitmap;)V"); check;
	jmethodID mid_drawText = (*env)->GetMethodID(env, class_Canvas,
		"drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V"); check;
	jmethodID mid_texImage2D = (*env)->GetStaticMethodID(env, class_GLUtils,
		"texImage2D", "(IIILandroid/graphics/Bitmap;I)V"); check;

	jfieldID fid_ARGB_8888 = (*env)->GetStaticFieldID(env, class_Bitmap_Config,
		"ARGB_8888", "Landroid/graphics/Bitmap$Config;"); check;

	jobject obj_argb_8888 = (*env)->GetStaticObjectField(env, class_Bitmap_Config, fid_ARGB_8888); check;
	jobject obj_paint = (*env)->NewObject(env, class_Paint, ctor_Paint); check;

	(*env)->CallVoidMethod(env, obj_paint, mid_setARGB, (jint)255, (jint)255, (jint)255, (jint)255); check;

	jfloat aFontSize = opts.size_px;
	(*env)->CallVoidMethod(env, obj_paint, mid_setTextSize, aFontSize); check;

	jfloat realTextWidth = (*env)->CallFloatMethod(env, obj_paint, mid_measureText, obj_str); check;

	jint bitmap_w = (jint)(realTextWidth + 2.0f), bitmap_h = (jint)(aFontSize + 2.0f);
	jobject obj_bitmap = (*env)->CallStaticObjectMethod(env, class_Bitmap,
		mid_createBitmap, bitmap_w, bitmap_h, obj_argb_8888); check;

	jint color = 0x000000FF;
	(*env)->CallVoidMethod(env, obj_bitmap, mid_eraseColor, color); check;

	jobject obj_canvas = (*env)->NewObject(env, class_Canvas, ctor_Canvas, obj_bitmap); check;

	jfloat pos_x = 1.0f, pos_y = 1.0f + aFontSize * 0.75f;
	(*env)->CallVoidMethod(env, obj_canvas, mid_drawText, obj_str, pos_x, pos_y, obj_paint); check;

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	(*env)->CallStaticVoidMethod(env, class_GLUtils, mid_texImage2D,
		(jint)GL_TEXTURE_2D, (jint)0, (jint)GL_RGBA, obj_bitmap, (jint)0); check;

	// const char *test_res = (*env)->GetStringUTFChars(env, obj_str, NULL); check;
	// pu_log_info("%s test_res: %s\n", __func__, test_res);
	// (*env)->ReleaseStringUTFChars(env, obj_str, test_res);

	(*env)->PopLocalFrame(env, NULL);

	s[0] = bitmap_w, s[1] = bitmap_h;
	return tex;
err:
	return 0;
#undef check
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
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
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
