#define _POSIX_C_SOURCE 199309L
#include <mgu/win.h>
#include <EGL/egl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <platform_utils/log.h>
#include <time.h>
#include <mgu/input-event-codes.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#elif defined(__ANDROID__)
#include "jni_helper.h"
#else
#include <EGL/eglext.h>
#include <wayland-egl.h>
#include <sys/mman.h>
#include <poll.h>
#endif

// https://stackoverflow.com/a/49236965
#define CASE_STR(value) case value: return #value;
const char* eglGetErrorString(EGLint error)
{
	switch(error)
	{
	CASE_STR(EGL_SUCCESS)
	CASE_STR(EGL_NOT_INITIALIZED)
	CASE_STR(EGL_BAD_ACCESS)
	CASE_STR(EGL_BAD_ALLOC)
	CASE_STR(EGL_BAD_ATTRIBUTE)
	CASE_STR(EGL_BAD_CONTEXT)
	CASE_STR(EGL_BAD_CONFIG)
	CASE_STR(EGL_BAD_CURRENT_SURFACE)
	CASE_STR(EGL_BAD_DISPLAY)
	CASE_STR(EGL_BAD_SURFACE)
	CASE_STR(EGL_BAD_MATCH)
	CASE_STR(EGL_BAD_PARAMETER)
	CASE_STR(EGL_BAD_NATIVE_PIXMAP)
	CASE_STR(EGL_BAD_NATIVE_WINDOW)
	CASE_STR(EGL_CONTEXT_LOST)
	default: return "Unknown";
	}
}
#undef CASE_STR

#define trace_egl_error() pu_log_trace("EGL error: %s\n", \
	eglGetErrorString(eglGetError()))

#if defined(__ANDROID__)
static void fill_display_metrics(struct mgu_disp *disp) {
	JavaVM *vm = disp->plat->app->activity->vm;
	JNIEnv *env;
	(*vm)->AttachCurrentThread(vm, &env, NULL);

	jni_find_class(NativeActivity, "android/app/NativeActivity");
	jni_find_class(WindowManager, "android/view/WindowManager");
	jni_find_class(Display, "android/view/Display");
	jni_find_class(DisplayMetrics, "android/util/DisplayMetrics");
	jni_find_method(NativeActivity, getWindowManager, "()Landroid/view/WindowManager;");
	jni_find_method(WindowManager, getDefaultDisplay, "()Landroid/view/Display;");
	jni_find_method(Display, getMetrics, "(Landroid/util/DisplayMetrics;)V");
	jni_find_ctor(DisplayMetrics, "()V");
	jni_find_field(DisplayMetrics, widthPixels, "I");
	jni_find_field(DisplayMetrics, heightPixels, "I");
	jni_find_field(DisplayMetrics, xdpi, "F");
	jni_find_field(DisplayMetrics, ydpi, "F");

	jobject obj_wm = (*env)->CallObjectMethod(env,
		disp->plat->app->activity->clazz, mid_getWindowManager);
	jni_check;

	jobject obj_display = (*env)->CallObjectMethod(env, obj_wm, mid_getDefaultDisplay);
	jni_check;

	jobject obj_metrics = (*env)->NewObject(env, class_DisplayMetrics, ctor_DisplayMetrics);
	jni_check;

	(*env)->CallVoidMethod(env, obj_display, mid_getMetrics, obj_metrics);
	jni_check;

	int width = (*env)->GetIntField(env, obj_metrics, fid_widthPixels);
	jni_check;

	int height = (*env)->GetIntField(env, obj_metrics, fid_heightPixels);
	jni_check;

	float xdpi = (*env)->GetFloatField(env, obj_metrics, fid_xdpi);
	jni_check;

	float ydpi = (*env)->GetFloatField(env, obj_metrics, fid_ydpi);
	jni_check;

	disp->out.res_px[0] = width;
	disp->out.res_px[1] = height;

	const static float mm_per_inch = 25.4;
	disp->out.size_mm[0] = (width / xdpi) * mm_per_inch;
	disp->out.size_mm[1] = (height / ydpi) * mm_per_inch;
jni_error:
	;
}
#endif

static void calculate_display_metrics(struct mgu_out *out) {
	double p = hypot(out->res_px[0], out->res_px[1]);
	double mm = hypot(out->size_mm[0], out->size_mm[1]);

	out->ppmm = p / mm;
	out->ppmm /= out->scale;

	static const double guessed_screen_visual_deg =
#if defined(__ANDROID__)
		15.0
#else
		22.0
#endif
	;
	out->ppvd = fmax(out->res_px[0], out->res_px[1]) /
		guessed_screen_visual_deg;
	out->ppvd /= out->scale;

	pu_log_info("[win/display metrics] res_px: { %d, %d }, scale: %d, "
		"size_mm: { %f, %f }, ppmm: %f, ppvd: %f\n",
		out->res_px[0], out->res_px[1],
		out->scale,
		out->size_mm[0], out->size_mm[1],
		out->ppmm,
		out->ppvd
	);
}

#if defined(__EMSCRIPTEN__)
EM_JS(double, mgu_win_internal_init_resize_listener, (), {
	document.addEventListener("gesturestart", function (e) { e.preventDefault(); });
	let canvas = document.getElementById("canvas");
	let scale = window.devicePixelRatio;
	let fn = () => {
		canvas.width = canvas.clientWidth * scale;
		canvas.height = canvas.clientHeight * scale;
	};
	window.addEventListener("resize", fn);
	fn();
	return scale;
});
#endif // defined(__EMSCRIPTEN__)

static void redraw_common(struct mgu_win_surf *surf) {
	if (!surf->dirty) return;
	surf->dirty = false;

	// assert(surf->egl_inited);
	EGLBoolean ret = eglMakeCurrent(surf->disp->egl_dpy, surf->egl_surf,
		surf->egl_surf, surf->disp->egl_ctx);
	if (ret != EGL_TRUE) {
		trace_egl_error();
		return;
	}

	EGLint w, h;
	eglQuerySurface(surf->disp->egl_dpy, surf->egl_surf, EGL_WIDTH, &w);
	eglQuerySurface(surf->disp->egl_dpy, surf->egl_surf, EGL_HEIGHT, &h);
	surf->size[0] = w, surf->size[1] = h;

	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	uint64_t msec = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;

	if (surf->disp->render_cb.f
		&& surf->disp->render_cb.f(
			surf->disp->render_cb.env, surf, msec)
		) {
		ret = eglSwapBuffers(surf->disp->egl_dpy, surf->egl_surf);
		if (ret != EGL_TRUE) {
			trace_egl_error();
		}

#if DEBUG_FRAME_RATE
		if (tp.tv_sec > surf->frame_counter_since) {
			surf->frame_counter_since = tp.tv_sec;
			pu_log_info("[win] surf %p FPS: %d\n",
				surf, surf->frame_counter_n);
			surf->frame_counter_n = 0;
		}
		++surf->frame_counter_n;
#endif
	}
}

static bool disp_init_egl_ctx(struct mgu_disp *disp) {
	if (disp->have_egl_ctx) return true;

	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	disp->egl_ctx = eglCreateContext(disp->egl_dpy, disp->egl_conf,
		EGL_NO_CONTEXT, context_attribs);
	if (!disp->egl_ctx) {
		return false;
	}

	// this needs surfaceless context!
	EGLBoolean ret = eglMakeCurrent(disp->egl_dpy, EGL_NO_SURFACE,
		EGL_NO_SURFACE, disp->egl_ctx);
	if (ret != EGL_TRUE) {
		trace_egl_error();
		return false;
	}

	disp->have_egl_ctx = true;
	if (disp->context_cb.f) {
		disp->context_cb.f(disp->context_cb.env, true);
	}

	return true;
}
static void disp_finish_egl_ctx(struct mgu_disp *disp) {
	if (!disp->have_egl_ctx) return;

	if (disp->context_cb.f) {
		disp->context_cb.f(disp->context_cb.env, false);
	}
	disp->have_egl_ctx = false;

	eglDestroyContext(disp->egl_dpy, disp->egl_ctx);
}

static bool disp_init_egl_dpy(struct mgu_disp *disp)
{
	bool res;
	EGLBoolean ret;
	EGLint n;
	EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
	disp->egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#else
	disp->egl_dpy = eglGetPlatformDisplay(
		EGL_PLATFORM_WAYLAND_KHR, disp->disp, NULL);
#endif
	if (!disp->egl_dpy) {
		res = false;
		goto return_res;
	}

	ret = eglInitialize(disp->egl_dpy, NULL, NULL);
	if (ret != EGL_TRUE) {
		res = false;
		goto cleanup_dpy;
	}

	ret = eglChooseConfig(disp->egl_dpy, attribs, &disp->egl_conf, 1, &n);
	if (ret != EGL_TRUE || n <= 0) {
		res = false;
		goto cleanup_dpy;
	}

	res = true;
	goto return_res;
cleanup_dpy:
	eglTerminate(disp->egl_dpy);
	eglReleaseThread();
return_res:
	return res;
}
static void disp_finish_egl_dpy(struct mgu_disp *disp) {
	eglTerminate(disp->egl_dpy);
	eglReleaseThread();
}

static int surf_init_egl(struct mgu_win_surf *surf) {
	assert(!surf->egl_inited);

	if (!disp_init_egl_ctx(surf->disp)) return -1;

	int res;
#if defined(__EMSCRIPTEN__)
	surf->egl_surf = eglCreateWindowSurface(surf->disp->egl_dpy,
		surf->disp->egl_conf, 0, NULL);
#elif defined(__ANDROID__)
	surf->egl_surf = eglCreateWindowSurface(surf->disp->egl_dpy,
		surf->disp->egl_conf, surf->disp->plat->app->window, NULL);
#else
	surf->native = wl_egl_window_create(surf->surf, 1, 1); // dummy size
	if (!surf->native) {
		res = -1;
		goto cleanup_none;
	}
	surf->egl_surf = eglCreatePlatformWindowSurface(surf->disp->egl_dpy,
		surf->disp->egl_conf, surf->native, NULL);
#endif
	if (surf->egl_surf == EGL_NO_SURFACE) {
		trace_egl_error();
		res = -1;
		goto cleanup_native;
	}

	EGLBoolean ret = eglMakeCurrent(surf->disp->egl_dpy, surf->egl_surf,
		surf->egl_surf, surf->disp->egl_ctx);
	if (ret != EGL_TRUE) {
		res = -1;
		goto cleanup_surf;
	}

	surf->egl_inited = true;
	res = 0;
	goto cleanup_none;
cleanup_surf:
	eglDestroySurface(surf->disp->egl_dpy, surf->egl_surf);
cleanup_native:
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	wl_egl_window_destroy(surf->native);
#endif
cleanup_none:
	return res;
}
static void surf_finish_egl(struct mgu_win_surf *surf) {
	assert(surf->egl_inited);

	eglDestroySurface(surf->disp->egl_dpy, surf->egl_surf);
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	wl_egl_window_destroy(surf->native);
#endif
	surf->egl_inited = false;
}

static void surf_destroy(struct mgu_win_surf *surf) {
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	if (surf->frame_cb) {
		wl_callback_destroy(surf->frame_cb);
		surf->frame_cb = NULL;
	}
#endif

	if (surf->egl_inited) {
		surf_finish_egl(surf);
	}

	switch (surf->type) {
#if defined(__EMSCRIPTEN__)
	case MGU_WIN_CANVAS:
		break;
#elif defined(__ANDROID__)
	case MGU_WIN_NATIVE_ACTIVITY:
		break;
#else
	case MGU_WIN_XDG:
		xdg_toplevel_destroy(surf->xdg.toplevel);
		xdg_surface_destroy(surf->xdg.surf);
		break;
	case MGU_WIN_LAYER:
		zwlr_layer_surface_v1_destroy(surf->layer.surf);
		wl_surface_destroy(surf->surf);
		break;
#endif
	}

	free(surf);
}

static void do_req_close(struct mgu_win_surf *surf) {
	surf->req_close = true;

	/* only stop event loop if ALL surfaces want to be closed */
	struct mgu_disp *disp = surf->disp;
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf_i =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		if (!surf_i->req_close) return;
	}

	event_loop_stop(surf->disp->el);
}

#if defined(__EMSCRIPTEN__)
static void redraw(struct mgu_win_surf *surf) {
	redraw_common(surf);
}
static EM_BOOL redraw_cb(double t, void *env)
{
	struct mgu_win_surf *surf = env;
	redraw(surf);
	return EM_TRUE;
}

struct { uint32_t code; const char *key; } key_lut[] = {
	{ KEY_0, "0" },
	{ KEY_1, "1" },
	{ KEY_2, "2" },
	{ KEY_3, "3" },
	{ KEY_3, "3" },
	{ KEY_4, "4" },
	{ KEY_5, "5" },
	{ KEY_6, "6" },
	{ KEY_7, "7" },
	{ KEY_8, "8" },
	{ KEY_9, "9" },
	{ KEY_A, "a" },
	{ KEY_B, "b" },
	{ KEY_C, "c" },
	{ KEY_D, "d" },
	{ KEY_E, "e" },
	{ KEY_F, "f" },
	{ KEY_G, "g" },
	{ KEY_H, "h" },
	{ KEY_I, "i" },
	{ KEY_J, "j" },
	{ KEY_K, "k" },
	{ KEY_L, "l" },
	{ KEY_M, "m" },
	{ KEY_N, "n" },
	{ KEY_O, "o" },
	{ KEY_P, "p" },
	{ KEY_Q, "q" },
	{ KEY_R, "r" },
	{ KEY_S, "s" },
	{ KEY_T, "t" },
	{ KEY_U, "u" },
	{ KEY_V, "v" },
	{ KEY_W, "w" },
	{ KEY_X, "x" },
	{ KEY_Y, "y" },
	{ KEY_Z, "z" },
};

static void input_event(struct mgu_win_surf *surf,
		struct mgu_input_event_args ev, uint32_t time) {
	ev.time = time;
	struct mgu_seat_cb *cb = &surf->disp->seat.cb;
	if (cb->f) {
		cb->f(cb->env, surf, ev);
	}
}
static EM_BOOL key_callback(int t, const EmscriptenKeyboardEvent *e, void *env) {
	struct mgu_win_surf *surf = env;
	for (int i = 0; i < sizeof(key_lut) / sizeof(key_lut[0]); ++i) {
		if (strcmp(e->key, key_lut[i].key) == 0) {
			struct mgu_input_event_args ev = {
				.t = MGU_KEYBOARD | MGU_DOWN,
				.keyboard.down.key = key_lut[i].code
			};
			input_event(surf, ev, 0);
			return EM_TRUE;
		}
	}
	return EM_FALSE;
}
static EM_BOOL touch_callback(int t, const EmscriptenTouchEvent *e, void *env) {
	struct mgu_win_surf *surf = env;
	double devicePixelRatio = surf->disp->out.devicePixelRatio;
	enum mgu_input_ev et = 0;
	switch (t) {
	case EMSCRIPTEN_EVENT_TOUCHSTART: et = MGU_TOUCH | MGU_DOWN; break;
	case EMSCRIPTEN_EVENT_TOUCHCANCEL:
	case EMSCRIPTEN_EVENT_TOUCHEND: et = MGU_TOUCH | MGU_UP; break;
	case EMSCRIPTEN_EVENT_TOUCHMOVE: et = MGU_TOUCH | MGU_MOVE; break;
	}
	for (int i = 0; i < e->numTouches; ++i) {
		const EmscriptenTouchPoint *p = &e->touches[i];
		if (!p->isChanged) continue;
		struct mgu_input_event_args ev = {
			.t = et,
			.touch = {
				.id = p->identifier,
				.down_or_move.p = {
					p->clientX * devicePixelRatio,
					p->clientY * devicePixelRatio
				}
			}
		};
		input_event(surf, ev, 0);
	}
	return EM_TRUE;
}

static int init_surf_canvas(struct mgu_win_surf *surf, struct mgu_disp *disp) {
	int res;
	surf->type = MGU_WIN_CANVAS;
	surf->disp = disp;

	if (surf_init_egl(surf) != 0) {
		res = -1;
		goto cleanup_none;
	}

	res = 0;
	goto cleanup_none;
cleanup_none:
	return res;
}
struct mgu_win_surf *mgu_disp_add_surf_canvas(struct mgu_disp *disp) {
	struct mgu_win_surf *surf = malloc(sizeof(struct mgu_win_surf));
	*surf = (struct mgu_win_surf){ 0 };
	int res = init_surf_canvas(surf, disp);
	if (res == 0) {
		vec_append(&disp->surfaces, &surf);
		return surf;
	}
	return NULL;
}
#elif defined(__ANDROID__)
/* uint32_t key_lut[] = {
	[AKEYCODE_1] = KEY_1,
	[AKEYCODE_2] = KEY_2,
	[AKEYCODE_3] = KEY_3,
	[AKEYCODE_4] = KEY_4,
	[AKEYCODE_5] = KEY_5,
	[AKEYCODE_6] = KEY_6,
	[AKEYCODE_7] = KEY_7,
	[AKEYCODE_8] = KEY_8,
	[AKEYCODE_9] = KEY_9,
	[AKEYCODE_0] = KEY_0,

	[AKEYCODE_Q] = KEY_Q,
	[AKEYCODE_W] = KEY_W,
	[AKEYCODE_E] = KEY_E,
	[AKEYCODE_R] = KEY_R,
	[AKEYCODE_T] = KEY_T,
	[AKEYCODE_Y] = KEY_Y,
	[AKEYCODE_U] = KEY_U,
	[AKEYCODE_I] = KEY_I,
	[AKEYCODE_O] = KEY_O,
	[AKEYCODE_P] = KEY_P,
	[AKEYCODE_A] = KEY_A,
	[AKEYCODE_S] = KEY_S,
	[AKEYCODE_D] = KEY_D,
	[AKEYCODE_F] = KEY_F,
	[AKEYCODE_G] = KEY_G,
	[AKEYCODE_H] = KEY_H,
	[AKEYCODE_J] = KEY_J,
	[AKEYCODE_K] = KEY_K,
	[AKEYCODE_L] = KEY_L,
	[AKEYCODE_Z] = KEY_Z,
	[AKEYCODE_X] = KEY_X,
	[AKEYCODE_C] = KEY_C,
	[AKEYCODE_V] = KEY_V,
	[AKEYCODE_B] = KEY_B,
	[AKEYCODE_N] = KEY_N,
	[AKEYCODE_M] = KEY_M,
}; */
static void redraw(struct mgu_win_surf *surf) {
	redraw_common(surf);
}
static void idle_fn(void *env) {
	struct mgu_disp *disp = env;

	// we dont support multiple surfaces yet
	if (disp->surfaces.len == 0) return;
	struct mgu_win_surf *surf =
		*(struct mgu_win_surf **)vec_get(&disp->surfaces, 0);

	redraw(surf);
}
static void input_event(struct mgu_win_surf *surf,
		struct mgu_input_event_args ev, uint64_t atime) {
	ev.time = atime / 1000000;
	struct mgu_seat_cb *cb = &surf->disp->seat.cb;
	if (cb->f) {
		cb->f(cb->env, surf, ev);
	}
}
static int32_t engine_handle_input(struct android_app* app,
		AInputEvent* event) {
	struct mgu_disp *disp = app->userData;

	// we dont support multiple surfaces
	if (disp->surfaces.len == 0) return 0;
	struct mgu_win_surf *surf =
		*(struct mgu_win_surf **)vec_get(&disp->surfaces, 0);

	int32_t asource = AInputEvent_getSource(event);
	int32_t aclass = asource & AINPUT_SOURCE_CLASS_MASK;

	switch (AInputEvent_getType(event)) {
	// TODO: this is untestable, because ANativeActivity_showSoftInput does
	// not seem to work
	/* case AINPUT_EVENT_TYPE_KEY: {
		int32_t acode = AKeyEvent_getKeyCode(event);
		int64_t atime = AKeyEvent_getEventTime(event);
		int32_t action = AKeyEvent_getAction(event);
		uint32_t code = key_lut[acode];
		struct mgu_input_event_args ev = { .t = MGU_KEYBOARD };
		if (action == AKEY_EVENT_ACTION_DOWN) {
			ev.t |= MGU_DOWN;
			ev.keyboard.down.key = code;
		} else {
			return 0;
		}
		if ((acode >= AKEYCODE_0 && acode <= AKEYCODE_9) ||
				(acode >= AKEYCODE_A && acode <= AKEYCODE_Z)) {
			input_event(surf, ev, atime);
			return 1;
		}
		break;
	} */
	case AINPUT_EVENT_TYPE_MOTION: {
		if (aclass != AINPUT_SOURCE_CLASS_POINTER) break;
		int64_t atime = AMotionEvent_getEventTime(event);
		int32_t action = AMotionEvent_getAction(event);
		int32_t index =
			(action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
			>> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
		action = action & AMOTION_EVENT_ACTION_MASK;
		size_t count = AMotionEvent_getPointerCount(event);
		for (size_t i = 0; i < count; ++i) {
			int32_t id = AMotionEvent_getPointerId(event, i);
			float x = AMotionEvent_getX(event, i);
			float y = AMotionEvent_getY(event, i);
			struct mgu_input_event_args ev = {
				.t = MGU_TOUCH,
				.touch.id = id,
				.touch.down_or_move.p = { x, y },
			};
			if (i != index) goto wtf_android;
			switch (action) {
			case AMOTION_EVENT_ACTION_DOWN:
			case AMOTION_EVENT_ACTION_POINTER_DOWN:
				ev.t |= MGU_DOWN;
				input_event(surf, ev, atime);
				break;
			case AMOTION_EVENT_ACTION_UP:
			case AMOTION_EVENT_ACTION_POINTER_UP:
				ev.t |= MGU_UP;
				input_event(surf, ev, atime);
				break;
			case AMOTION_EVENT_ACTION_MOVE:
wtf_android:
				ev.t |= MGU_MOVE;
				input_event(surf, ev, atime);
				break;
			}
		}
		return 1;
	}
	}
	return 0;
}
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	struct mgu_disp *disp = app->userData;

	// we dont support multiple surfaces yet
	if (disp->surfaces.len == 0) return;
	struct mgu_win_surf *surf =
		*(struct mgu_win_surf **)vec_get(&disp->surfaces, 0);

	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		if (disp->plat->app->window != NULL) {
			if (!disp_init_egl_ctx(disp)) {
				pu_log_info("[win/android]"
					" disp_init_egl_ctx failed!\n");
			}
			if (surf_init_egl(surf) != 0) {
				// error
				pu_log_info("[win/android]"
					" surf_init_egl failed!\n");
				break;
			}
			mgu_disp_force_redraw(disp);
			event_loop_set_idle_func(disp->el, disp, idle_fn);
		}
		break;
	case APP_CMD_TERM_WINDOW:
		event_loop_set_idle_func(disp->el, NULL, NULL);
		disp_finish_egl_ctx(disp);
		surf_finish_egl(surf);
		break;
	case APP_CMD_CONFIG_CHANGED:
	case APP_CMD_WINDOW_REDRAW_NEEDED:
		mgu_win_surf_mark_dirty(surf);
		break;
	default:
		break;
	}
}
static int init_surf_native_activity(struct mgu_win_surf *surf,
		struct mgu_disp *disp) {
	int res;
	surf->type = MGU_WIN_NATIVE_ACTIVITY;
	surf->disp = disp;

	res = 0;
	goto cleanup_none;
cleanup_none:
	return res;
}
struct mgu_win_surf *mgu_disp_add_surf_native_activity(struct mgu_disp *disp) {
	struct mgu_win_surf *surf = malloc(sizeof(struct mgu_win_surf));
	*surf = (struct mgu_win_surf){ 0 };
	int res = init_surf_native_activity(surf, disp);
	if (res == 0) {
		vec_append(&disp->surfaces, &surf);
		return surf;
	}
	return NULL;
}
#else
static struct mgu_out *find_out(struct mgu_disp *disp,
		struct wl_output *wl_output) {
	for (int i = 0; i < disp->outputs.len; ++i) {
		struct mgu_out *out = vec_get(&disp->outputs, i);
		if (out->out == wl_output) return out;
	}
	return NULL;
}
static struct mgu_win_surf *find_surf(struct mgu_disp *disp,
		struct wl_surface *wl_surface) {
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		if (surf->surf == wl_surface) return surf;
	}
	return NULL;
}

static void wl_point(wl_fixed_t x, wl_fixed_t y, double p[static 2]) {
	p[0] = wl_fixed_to_double(x);
	p[1] = wl_fixed_to_double(y);
}
static void input_event(struct mgu_seat *seat, struct mgu_input_event_args ev,
		uint32_t time, struct mgu_win_surf *surf) {
	ev.time = time;
	if (seat->cb.f) {
		seat->cb.f(seat->cb.env, surf, ev);
	}
}
static void wl_touch_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time,
		struct wl_surface *surface,
		int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_seat *seat = data;
	struct mgu_touch_point tp = {
		.id = id, .surf = find_surf(seat->disp, surface) };
	vec_append(&seat->touch_points, &tp);

	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_DOWN };
	wl_point(x, y, ev.touch.down_or_move.p);
	ev.touch.id = id;
	input_event(seat, ev, time, tp.surf);
}
static void wl_touch_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id) {
	struct mgu_seat *seat = data;
	struct mgu_touch_point tp = { 0 };
	for (int i = 0; i < seat->touch_points.len; ++i) {
		struct mgu_touch_point *tpi = vec_get(&seat->touch_points, i);
		if (tpi->id == id) {
			tp = *tpi;
			vec_remove(&seat->touch_points, i);
			break;
		}
	}

	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_UP };
	ev.touch.id = id;
	input_event(seat, ev, time, tp.surf);
}
static void wl_touch_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_seat *seat = data;
	struct mgu_touch_point tp = { 0 };
	for (int i = 0; i < seat->touch_points.len; ++i) {
		struct mgu_touch_point *tpi = vec_get(&seat->touch_points, i);
		if (tpi->id == id) {
			tp = *tpi;
			break;
		}
	}

	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_MOVE };
	wl_point(x, y, ev.touch.down_or_move.p);
	ev.touch.id = id;
	input_event(seat, ev, time, tp.surf);
}
static void wl_touch_frame(void *data, struct wl_touch *wl_touch) { }
static void wl_touch_cancel(void *data, struct wl_touch *wl_touch) { }
static void wl_touch_shape(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t major, wl_fixed_t minor) { }
static void wl_touch_orientation(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t orientation) { }
static const struct wl_touch_listener touch_lis = {
	.down = wl_touch_down,
	.up = wl_touch_up,
	.motion = wl_touch_motion,
	.frame = wl_touch_frame,
	.cancel = wl_touch_cancel,
	.shape = wl_touch_shape,
	.orientation = wl_touch_orientation,
};

static void pointer_enter(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t x, wl_fixed_t y) {
	struct mgu_seat *seat = data;
	wl_point(x, y, seat->pointer_p);
	seat->pointer_surf = find_surf(seat->disp, surface);
}
static void pointer_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface) {
	((struct mgu_seat *)data)->pointer_surf = NULL;
}
static void pointer_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_seat *seat = data;
	wl_point(x, y, seat->pointer_p);

	struct mgu_input_event_args ev = { .t = MGU_POINTER | MGU_MOVE };
	memcpy(ev.pointer.move.p, seat->pointer_p, sizeof(double) * 2);
	input_event(seat, ev, time, seat->pointer_surf);
}
static void pointer_button(void *data, struct wl_pointer *pointer,
		uint32_t serial, uint32_t time,
		uint32_t button, uint32_t state) {
	struct mgu_seat *seat = data;
	struct mgu_input_event_args ev = {
		.t = MGU_POINTER | MGU_BTN };
	memcpy(ev.pointer.btn.p, seat->pointer_p, sizeof(double) * 2);
	if (button & BTN_LEFT) {
		ev.t |= (state == 1 ? MGU_DOWN : MGU_UP);
		ev.pointer.btn.state = state;
		input_event(seat, ev, time, seat->pointer_surf);
	}
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) { }
static void pointer_frame(void *data, struct wl_pointer *pointer) { }
static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t source) { }
static void pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) { }
static void pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) { }
const struct wl_pointer_listener pointer_lis = {
	pointer_enter,
	pointer_leave,
	pointer_motion,
	pointer_button,
	pointer_axis,
	pointer_frame,
	pointer_axis_source,
	pointer_axis_stop,
	pointer_axis_discrete
};

// wl_keyboard_listener
static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size) {
	// void *mem = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	// if (mem == MAP_FAILED) {
	// 	return;
	// }
	// fprintf(stderr, "keymap: %.*s\n", size, (char*)mem);
	// munmap(mem, size);
}
static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface,
		struct wl_array *keys) {
	struct mgu_seat *seat = data;
	seat->keyboard_surf = find_surf(seat->disp, surface);
}
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface) {
	((struct mgu_seat *)data)->keyboard_surf = NULL;
}
static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key,
		uint32_t _key_state) {
	struct mgu_seat *seat = data;
	enum wl_keyboard_key_state key_state = _key_state;
	if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		struct mgu_input_event_args ev = {
			.t = MGU_KEYBOARD | MGU_DOWN };
		ev.keyboard.down.key = key;
		input_event(seat, ev, time, seat->keyboard_surf);
	}
}
static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group) {
}
static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay) {
}
static const struct wl_keyboard_listener keyboard_lis = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void
capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps)
{
	struct mgu_seat *seat = data;

	if (caps & WL_SEAT_CAPABILITY_POINTER) {
		if (!seat->pointer) {
			seat->pointer = wl_seat_get_pointer(wl_seat);
			wl_pointer_add_listener(seat->pointer,
				&pointer_lis, seat);
		}
	} else if (seat->pointer) {
		wl_pointer_release(seat->pointer), seat->pointer = NULL;
	}

	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		if (!seat->keyboard) {
			seat->keyboard = wl_seat_get_keyboard(wl_seat);
			wl_keyboard_add_listener(seat->keyboard,
				&keyboard_lis, seat);
		}
	} else if (seat->keyboard) {
		wl_keyboard_release(seat->keyboard), seat->keyboard = NULL;
	}

	if (caps & WL_SEAT_CAPABILITY_TOUCH) {
		if (!seat->touch) {
			seat->touch = wl_seat_get_touch(wl_seat);
			wl_touch_add_listener(seat->touch, &touch_lis, seat);
		}
	} else if (seat->touch) {
		wl_touch_release(seat->touch), seat->touch = NULL;
	}
}

static void name(void *data, struct wl_seat *wl_seat, const char *name) { }

static const struct wl_seat_listener seat_lis = {
	.capabilities = capabilities,
	.name = name,
};

static void
handle_xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}
static const struct xdg_wm_base_listener wm_lis = { handle_xdg_wm_base_ping };

static void output_handle_geometry(void *data, struct wl_output *wl_output,
		int32_t x, int32_t y, int32_t phys_width, int32_t phys_height,
		int32_t subpixel, const char *make, const char *model,
		int32_t transform) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = find_out(disp, wl_output);
	if (out) {
		out->size_mm[0] = phys_width, out->size_mm[1] = phys_height;
	}
}
static void output_handle_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = find_out(disp, wl_output);
	if (out) {
		out->res_px[0] = width;
		out->res_px[1] = height;
	}
}
static void output_handle_done(void* data, struct wl_output *wl_output) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = find_out(disp, wl_output);
	if (out) {
		calculate_display_metrics(out);
		out->configured = true;
	}
}
static void output_handle_scale(void* data, struct wl_output *wl_output,
	int32_t factor) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = find_out(disp, wl_output);
	if (out) {
		out->scale = factor;
	}
}
static const struct wl_output_listener output_lis = {
	.geometry = output_handle_geometry,
	.mode = output_handle_mode,
	.done = output_handle_done,
	.scale = output_handle_scale,
};

static void
global(void *data, struct wl_registry *reg,
		uint32_t id, const char *i, uint32_t version)
{
	struct mgu_disp *disp = data;
	if (strcmp(i, wl_compositor_interface.name) == 0) {
		disp->comp = wl_registry_bind(reg, id, &wl_compositor_interface,
			wl_compositor_interface.version);
	} else if (!disp->seat.seat && strcmp(i, wl_seat_interface.name) == 0) {
		disp->seat.seat = wl_registry_bind(reg, id, &wl_seat_interface,
			wl_seat_interface.version);
		if (disp->seat.seat) {
			wl_seat_add_listener(disp->seat.seat,
				&seat_lis, &disp->seat);
		}
	} else if (strcmp(i, xdg_wm_base_interface.name) == 0) {
		disp->wm = wl_registry_bind(reg, id, &xdg_wm_base_interface,
			version); // TODO: compositor support?
		if (disp->wm) {
			xdg_wm_base_add_listener(disp->wm, &wm_lis, disp);
		}
	} else if (strcmp(i, zwlr_layer_shell_v1_interface.name) == 0) {
		disp->layer_shell = wl_registry_bind(reg, id,
			&zwlr_layer_shell_v1_interface,
			zwlr_layer_shell_v1_interface.version);
	} else if (strcmp(i, wl_output_interface.name) == 0) {
		struct mgu_out out = { .ppmm = -1.0, .scale = 1 };
		out.out = wl_registry_bind(reg, id, &wl_output_interface,
			wl_output_interface.version);
		if (out.out) {
			wl_output_add_listener(out.out, &output_lis, disp);
			vec_append(&disp->outputs, &out);
		}
	} else if (disp->global_cb.f) {
		disp->global_cb.f(disp->global_cb.env, reg, id, i, version);
	}
}
static void
global_dump(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version)
{
	fprintf(stderr, "%s\n", interface);
}
static void
global_remove(void *data, struct wl_registry *registry, uint32_t name) { }
const struct wl_registry_listener reg_lis = { global, global_remove };
const struct wl_registry_listener mgu_wl_registry_listener_dump = {
	global_dump,
	global_remove
};


int mgu_disp_init_custom(struct mgu_disp *disp, struct platform *plat,
		struct mgu_global_cb global_cb) {
	disp->global_cb = global_cb;
	return mgu_disp_init(disp, plat);
}

static void schedule_frame(struct mgu_win_surf *surf);

static void redraw(struct mgu_win_surf *surf)
{
	redraw_common(surf);

	schedule_frame(surf);
}

static void
frame_cb(void *data, struct wl_callback *wl_callback, uint32_t time)
{
	struct mgu_win_surf *surf = data;
	if (surf->frame_cb) {
		wl_callback_destroy(surf->frame_cb);
		surf->frame_cb = NULL;
	}

	redraw(surf);
}
static const struct wl_callback_listener frame_cb_lis = { .done = frame_cb };

static void schedule_frame(struct mgu_win_surf *surf)
{
	if (!surf->frame_cb) {
		surf->frame_cb = wl_surface_frame(surf->surf);
		if (surf->frame_cb) {
			wl_callback_add_listener(surf->frame_cb,
				&frame_cb_lis, surf);
			wl_surface_commit(surf->surf);
		}
	}
}

static void surface_enter(void *data, struct wl_surface *surface,
		struct wl_output *output) {
	struct mgu_win_surf *surf = data;
	surf->out_last_entered = output;
	pu_log_info("[win/wayland] surface enter: %p\n", output);
}
static void surface_leave(void *data, struct wl_surface *surface,
		struct wl_output *output) {
	// I guess just ignore this?
	pu_log_info("[win/wayland] surface leave: %p\n", output);
}

static struct wl_surface_listener surf_lis = {
	.enter = surface_enter,
	.leave = surface_leave,
};

static void configure_common(struct mgu_win_surf *surf,
		int32_t size[static 2]) {
	struct mgu_out *out = mgu_win_surf_get_output(surf);
	int32_t scale = out ? out->scale : 1;
	// surf->size[0] = size[0], surf->size[1] = size[1];
	wl_surface_set_buffer_scale(surf->surf, scale);
	wl_egl_window_resize(surf->native,
		size[0] * scale, size[1] * scale, 0, 0);
	mgu_win_surf_mark_dirty(surf);
}
static void configure_ack_common(struct mgu_win_surf *surf) {
	if (surf->wait_for_configure) {
		eglSwapBuffers(surf->disp->egl_dpy, surf->egl_surf);
		schedule_frame(surf);
		surf->wait_for_configure = false;
	}
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct mgu_win_surf *surf = data;
	configure_common(surf, (int32_t[]){ width, height });
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	configure_ack_common(surf);
}
static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	do_req_close(data);
}
struct zwlr_layer_surface_v1_listener layer_surf_lis = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void handle_xdg_surface_configure(void *data,
		struct xdg_surface *surface, uint32_t serial) {
	xdg_surface_ack_configure(surface, serial);
	configure_ack_common(data);
}
static const struct xdg_surface_listener xdg_surf_lis = {
	handle_xdg_surface_configure
};

static void handle_xdg_toplevel_configure(void *data,
		struct xdg_toplevel *xdg_toplevel,
		int32_t width, int32_t height, struct wl_array *state) {
	if (width == 0 || height == 0) {
		width = height = 500;
	}
	configure_common(data, (int32_t[]){ width, height });
}
static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel
		*xdg_toplevel) {
	do_req_close(data);
}
static const struct xdg_toplevel_listener toplevel_lis = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
};

static int init_surf_common(struct mgu_win_surf *surf, struct mgu_disp *disp) {
	int res;

	surf->disp = disp;
	surf->wait_for_configure = true;

	wl_surface_commit(surf->surf);

	if (surf_init_egl(surf) != 0) {
		res = -1;
		goto cleanup_none;
	}

	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_none;
	}

	res = 0;
	goto cleanup_none;
cleanup_none:
	return res;
}

static int init_surf_xdg(
		struct mgu_win_surf *surf,
		struct mgu_disp *disp,
		const char *title) {
	int res;

	surf->type = MGU_WIN_XDG;
	surf->surf = wl_compositor_create_surface(disp->comp);
	if (!surf->surf) {
		res = -1;
		goto cleanup_none;
	}

	res = wl_surface_add_listener(surf->surf, &surf_lis, surf);
	if (res == -1) {
		goto cleanup_surf;
	}

	surf->xdg.surf = xdg_wm_base_get_xdg_surface(disp->wm, surf->surf);
	if (!surf->xdg.surf) {
		res = -1;
		goto cleanup_surf;
	}

	res = xdg_surface_add_listener(surf->xdg.surf, &xdg_surf_lis, surf);
	if (res == -1) {
		goto cleanup_xdg_surf;
	}

	surf->xdg.toplevel = xdg_surface_get_toplevel(surf->xdg.surf);
	if (!surf->xdg.toplevel) {
		res = -1;
		goto cleanup_xdg_surf;
	}

	res = xdg_toplevel_add_listener(surf->xdg.toplevel,
		&toplevel_lis, surf);
	if (res == -1) {
		goto cleanup_toplevel;
	}

	xdg_toplevel_set_title(surf->xdg.toplevel, title);

	res = init_surf_common(surf, disp);
	if (res == -1) {
		goto cleanup_toplevel;
	}

	res = 0;
	goto cleanup_none;
cleanup_toplevel:
	xdg_toplevel_destroy(surf->xdg.toplevel);
cleanup_xdg_surf:
	xdg_surface_destroy(surf->xdg.surf);
cleanup_surf:
	wl_surface_destroy(surf->surf);
cleanup_none:
	return res;
}

static int init_layer_surf(
		struct mgu_win_surf *surf,
		struct mgu_disp *disp,
		struct wl_output *wl_output,
		uint32_t size[static 2],
		int32_t exclusive_zone,
		enum zwlr_layer_surface_v1_anchor anchor,
		enum zwlr_layer_shell_v1_layer layer) {
	int res;

	surf->type = MGU_WIN_LAYER;
	memcpy(surf->size, size, sizeof(uint32_t) * 2);
	surf->surf = wl_compositor_create_surface(disp->comp);
	if (!surf->surf) {
		res = -1;
		goto cleanup_none;
	}

	surf->layer.surf = zwlr_layer_shell_v1_get_layer_surface(
		disp->layer_shell,
		surf->surf,
		wl_output,
		layer,
		"panel"
	);
	if (!surf->layer.surf) {
		res = -1;
		goto cleanup_surf;
	}

	res = zwlr_layer_surface_v1_add_listener(surf->layer.surf,
		&layer_surf_lis, surf);
	if (res == -1) {
		goto cleanup_layer_surf;
	}

	zwlr_layer_surface_v1_set_size(surf->layer.surf, size[0], size[1]);
	zwlr_layer_surface_v1_set_anchor(surf->layer.surf, anchor);
	zwlr_layer_surface_v1_set_exclusive_zone(surf->layer.surf,
		exclusive_zone);

	res = init_surf_common(surf, disp);
	if (res == -1) {
		goto cleanup_layer_surf;
	}

	res = 0;
	goto cleanup_none;
cleanup_layer_surf:
	zwlr_layer_surface_v1_destroy(surf->layer.surf);
cleanup_surf:
	wl_surface_destroy(surf->surf);
cleanup_none:
	return res;
}

struct mgu_win_surf *mgu_disp_add_surf_xdg(struct mgu_disp *disp,
		const char *title) {
	struct mgu_win_surf *surf = malloc(sizeof(struct mgu_win_surf));
	*surf = (struct mgu_win_surf){ 0 };
	int res = init_surf_xdg(surf, disp, title);
	if (res == 0) {
		vec_append(&disp->surfaces, &surf);
		return surf;
	}
	return NULL;
}
struct mgu_win_surf *mgu_disp_add_surf_layer_bottom_panel(struct mgu_disp *disp,
		uint32_t size) {
	struct mgu_win_surf *surf = malloc(sizeof(struct mgu_win_surf));
	*surf = (struct mgu_win_surf){ 0 };
	int res = init_layer_surf(surf, disp,
		mgu_disp_get_default_output(disp)->out,
		(uint32_t[]){ 0, size },
		size,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP
	);
	if (res == 0) {
		vec_append(&disp->surfaces, &surf);
		return surf;
	}
	return NULL;
}
struct mgu_win_surf *mgu_disp_add_surf_layer_overlay_for_each_output(
		struct mgu_disp *disp) {
	struct mgu_win_surf *last_surf = NULL;
	for (int i = 0; i < disp->outputs.len; ++i) {
		struct mgu_out *out = vec_get(&disp->outputs, i);
		struct mgu_win_surf *surf = malloc(sizeof(struct mgu_win_surf));
		*surf = (struct mgu_win_surf){ 0 };
		int res = init_layer_surf(surf, disp,
			out->out,
			(uint32_t[]){ 0, 0 },
			-1,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
				| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
			ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY
		);
		if (res == 0) {
			vec_append(&disp->surfaces, &surf);
			last_surf = surf;
		} else {
			return NULL;
		}
	}
	return last_surf;
}

void mgu_disp_remove_surf(struct mgu_disp *disp, struct mgu_win_surf *surf) {
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf_i =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		if (surf_i == surf) {
			vec_remove(&disp->surfaces, i);
			surf_destroy(surf_i);
			break;
		}
	}
}


void mgu_disp_mark_all_surfs_dirty(struct mgu_disp *disp) {
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf_i =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		mgu_win_surf_mark_dirty(surf_i);
	}
}
#endif

void mgu_win_surf_mark_dirty(struct mgu_win_surf *surf) {
	surf->dirty = true;

#if defined(__EMSCRIPTEN__)
	// TODO
#elif defined(__ANDROID__)
	// TODO
#else
	schedule_frame(surf);
#endif
}

int mgu_disp_init(struct mgu_disp *disp, struct platform *plat)
{
	int res;
	disp->plat = plat;
	disp->surfaces = vec_new_empty(sizeof(struct mgu_win_surf *));

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	disp->disp = wl_display_connect(NULL);
	if (!disp->disp) {
		res = -1; /* wl_display_connect failed */
		goto cleanup_none;
	}
#endif

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
	// default fake values
	struct mgu_out *out = &disp->out;
	*out = (struct mgu_out){
		.size_mm = { 25, 25 },
		.res_px = { 150, 150 },
		.scale = 1
	};
#endif

	if (!disp_init_egl_dpy(disp)) {
		res = -1;
		goto cleanup_none;
	}

#if defined(__EMSCRIPTEN__)
	double devicePixelRatio = mgu_win_internal_init_resize_listener();
	disp->out.devicePixelRatio = devicePixelRatio;
	disp->out.res_px[0] *= devicePixelRatio;
	disp->out.res_px[1] *= devicePixelRatio;
	pu_log_info("[win/emscripten] devicePixelRatio: %f\n",
		devicePixelRatio);
	calculate_display_metrics(out);
#elif defined(__ANDROID__)
	fill_display_metrics(disp);
	calculate_display_metrics(out);
#endif

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	disp->reg = wl_display_get_registry(disp->disp);
	if (!disp->reg) {
		res = -1; /* TODO: can this even happen? */
		goto cleanup_egl_dpy;
	}

	res = wl_registry_add_listener(disp->reg, &reg_lis, disp);
	if (res == -1) {
		goto cleanup_egl_dpy;
	}

	disp->outputs = vec_new_empty(sizeof(struct mgu_out));
	disp->seat.touch_points = vec_new_empty(sizeof(struct mgu_touch_point));
	disp->seat.disp = disp;

	// TODO: please...
	res = wl_display_roundtrip(disp->disp);
	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_egl_dpy;
	}

	struct mgu_out *out = mgu_disp_get_default_output(disp);
	if (!(disp->comp
			&& disp->seat.seat
			&& disp->wm
			&& disp->layer_shell
			&& out
			&& out->configured)) {
		res = -1;
		goto cleanup_egl_dpy;
	}
#endif

	res = 0;
	goto cleanup_none;
#if !defined(__ANDROID__)
cleanup_egl_dpy:
	disp_init_egl_dpy(disp);
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	wl_display_disconnect(disp->disp);
#endif
#endif
cleanup_none:
	return res;
}

void mgu_disp_finish(struct mgu_disp *disp)
{
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		surf_destroy(surf);
	}
	vec_free(&disp->surfaces);

	disp_finish_egl_ctx(disp);
	disp_finish_egl_dpy(disp);

#if defined(__EMSCRIPTEN__)
	/* TODO */
#elif defined(__ANDROID__)
	/* TODO */
#else
	if (disp->reg) {
		wl_registry_destroy(disp->reg);
	}

	wl_display_disconnect(disp->disp);
#endif
}

struct mgu_win_surf *mgu_disp_add_surf_default(struct mgu_disp *disp,
		const char *title) {
#if defined(__EMSCRIPTEN__)
	return mgu_disp_add_surf_canvas(disp);
#elif defined(__ANDROID__)
	return mgu_disp_add_surf_native_activity(disp);
#else
	return mgu_disp_add_surf_xdg(disp, title);
#endif
}

struct mgu_out *mgu_disp_get_default_output(struct mgu_disp *disp) {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
	return &disp->out;
#else
	if (disp->outputs.len > 0) {
		return vec_get(&disp->outputs, 0);
	}
	return NULL;
#endif
}

struct mgu_out *mgu_win_surf_get_output(struct mgu_win_surf *surf) {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
	return &disp->out;
#else
	struct mgu_out *out = find_out(surf->disp, surf->out_last_entered);
	if (!out) out = mgu_disp_get_default_output(surf->disp);
	return out;
#endif
}

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
static void mgu_disp_dispatch(struct mgu_disp *disp) {
	while (wl_display_prepare_read(disp->disp) != 0)
		wl_display_dispatch_pending(disp->disp);

	wl_display_read_events(disp->disp);
	wl_display_dispatch_pending(disp->disp);
	wl_display_flush(disp->disp);
}
static void disp_dispatch(void *env, struct pollfd pfd) {
	struct mgu_disp *disp = env;
	if (pfd.revents & POLLIN) {
		mgu_disp_dispatch(disp);
		if (disp->req_stop) event_loop_stop(disp->el);
	}
}
#endif

void mgu_disp_add_to_event_loop(struct mgu_disp *disp, struct event_loop *el) {
	disp->el = el;

#if defined(__EMSCRIPTEN__)
	// we dont support multiple surfaces yet
	if (disp->surfaces.len == 0) return;
	struct mgu_win_surf *surf =
		*(struct mgu_win_surf **)vec_get(&disp->surfaces, 0);

	/* keyboard */
	emscripten_set_keypress_callback("body", surf, EM_FALSE, key_callback);

	/* touch */
	emscripten_set_touchstart_callback("body", surf, EM_FALSE, touch_callback);
	emscripten_set_touchend_callback("body", surf, EM_FALSE, touch_callback);
	emscripten_set_touchmove_callback("body", surf, EM_FALSE, touch_callback);
	emscripten_set_touchcancel_callback("body", surf, EM_FALSE, touch_callback);

	emscripten_request_animation_frame_loop(redraw_cb, surf);
#elif defined(__ANDROID__)
	disp->plat->app->userData = disp;
	disp->plat->app->onAppCmd = engine_handle_cmd;
	disp->plat->app->onInputEvent = engine_handle_input;
#else
	event_loop_add_fd(el, wl_display_get_fd(disp->disp),
		POLLIN, disp, disp_dispatch);
#endif
}

void mgu_disp_force_redraw(struct mgu_disp *disp) {
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf_i =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		mgu_win_surf_mark_dirty(surf_i);
		redraw(surf_i);
	}
}

void mgu_disp_set_context_cb(struct mgu_disp *disp, struct mgu_context_cb cb) {
	disp->context_cb = cb;
	if (disp->have_egl_ctx) {
		if (disp->context_cb.f) {
			disp->context_cb.f(disp->context_cb.env, true);
		}
	}
}
