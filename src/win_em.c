#include <EGL/egl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <mgu/win.h>

static int disp_init_egl(struct mgu_disp *disp)
{
	int res;
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
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	disp->egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!disp->egl_dpy) {
		res = -1;
		goto return_res;
	}

	ret = eglInitialize(disp->egl_dpy, NULL, NULL);
	if (ret != EGL_TRUE) {
		res = -1;
		goto cleanup_dpy;
	}

	ret = eglChooseConfig(disp->egl_dpy, attribs, &disp->egl_conf, 1, &n);
	if (ret != EGL_TRUE || n <= 0) {
		res = -1;
		goto cleanup_dpy;
	}

	disp->egl_ctx = eglCreateContext(disp->egl_dpy, disp->egl_conf,
		EGL_NO_CONTEXT, context_attribs);
	if (!disp->egl_ctx) {
		res = -1;
		goto cleanup_dpy;
	}

	res = 0;
	goto return_res;
cleanup_dpy:
	eglTerminate(disp->egl_dpy);
	eglReleaseThread();
return_res:
	return res;
}

int mgu_disp_init(struct mgu_disp *disp)
{
	int res;

	if (disp_init_egl(disp) != 0) {
		res = -1;
		goto cleanup_disp;
	}

	res = 0;
	goto cleanup_none;
cleanup_disp:
cleanup_none:
	return res;
}

void mgu_disp_finish(struct mgu_disp *disp)
{
}

static int win_init_egl(struct mgu_win *win)
{
	int res;
	EGLBoolean ret;
	struct mgu_disp *disp = win->disp;

	win->egl_surf = eglCreateWindowSurface(
		disp->egl_dpy, disp->egl_conf, 0, NULL);
	if (!win->egl_surf) {
		res = -1;
		goto cleanup_none;
	}

	ret = eglMakeCurrent(disp->egl_dpy, win->egl_surf,
		win->egl_surf, disp->egl_ctx);
	if (ret != EGL_TRUE) {
		res = -1;
		goto cleanup_surf;
	}

	res = 0;
	goto cleanup_none;
cleanup_surf:
	eglDestroySurface(disp->egl_dpy, win->egl_surf);
cleanup_none:
	return res;
}

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

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp) {
	int res;
	win->disp = disp;

	disp->out = (struct mgu_out){
		.size_mm = { 25, 25 },
		.res_px = { 150, 150 },
		.scale = 1
	};

	double devicePixelRatio = mgu_win_internal_init_resize_listener();
	disp->out.devicePixelRatio = devicePixelRatio;
	fprintf(stderr, "%s devicePixelRatio: %f\n", __func__, devicePixelRatio);

	struct mgu_out *out = &disp->out;
	double p = hypot(out->res_px[0], out->res_px[1]);
	double mm = hypot(out->size_mm[0], out->size_mm[1]);
	out->ppmm = p / mm;
	out->ppmm /= out->scale;

	if (win_init_egl(win) != 0) {
		res = -1;
		goto cleanup_none;
	}

	res = 0;
	goto cleanup_none;
cleanup_none:
	return res;
}

void mgu_win_finish(struct mgu_win *win)
{
	/* TODO */
}

static EM_BOOL redraw(double t, void *env)
{
	struct mgu_win *win = env;

	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle = emscripten_webgl_get_current_context();
	emscripten_webgl_get_drawing_buffer_size(handle, &win->size[0], &win->size[1]);

	bool rendered = win->render_cb.f(win->render_cb.cl, t / 1000.0);
	if (rendered) eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
	return EM_TRUE;
}

#define KEY_1			2
#define KEY_2			3
#define KEY_3			4
#define KEY_4			5
#define KEY_5			6
#define KEY_6			7
#define KEY_7			8
#define KEY_8			9
#define KEY_9			10
#define KEY_0			11

#define KEY_Q			16
#define KEY_W			17
#define KEY_E			18
#define KEY_R			19
#define KEY_T			20
#define KEY_Y			21
#define KEY_U			22
#define KEY_I			23
#define KEY_O			24
#define KEY_P			25
#define KEY_A			30
#define KEY_S			31
#define KEY_D			32
#define KEY_F			33
#define KEY_G			34
#define KEY_H			35
#define KEY_J			36
#define KEY_K			37
#define KEY_L			38
#define KEY_Z			44
#define KEY_X			45
#define KEY_C			46
#define KEY_V			47
#define KEY_B			48
#define KEY_N			49
#define KEY_M			50

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

static void input_event(struct mgu_seat *seat, struct mgu_input_event_args ev,
		uint32_t time) {
	ev.time = time;
	if (seat->cb.f) {
		seat->cb.f(seat->cb.cl, ev);
	}
}
static EM_BOOL key_callback(int t, const EmscriptenKeyboardEvent *e, void *env) {
	struct mgu_disp *disp = env;
	for (int i = 0; i < sizeof(key_lut) / sizeof(key_lut[0]); ++i) {
		if (strcmp(e->key, key_lut[i].key) == 0) {
			struct mgu_input_event_args ev = {
				.t = MGU_KEYBOARD | MGU_DOWN,
				.keyboard.down.key = key_lut[i].code
			};
			input_event(&disp->seat, ev, 0);
			return EM_TRUE;
		}
	}
	return EM_FALSE;
}
static EM_BOOL touch_callback(int t, const EmscriptenTouchEvent *e, void *env) {
	struct mgu_disp *disp = env;
	double devicePixelRatio = disp->out.devicePixelRatio;
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
		input_event(&disp->seat, ev, 0);
	}
	return EM_TRUE;
}

void mgu_win_run(struct mgu_win *win)
{
	struct mgu_disp *disp = win->disp;

	/* keyboard */
	emscripten_set_keypress_callback("body", disp, EM_FALSE, key_callback);

	/* touch */
	emscripten_set_touchstart_callback("body", disp, EM_FALSE, touch_callback);
	emscripten_set_touchend_callback("body", disp, EM_FALSE, touch_callback);
	emscripten_set_touchmove_callback("body", disp, EM_FALSE, touch_callback);
	emscripten_set_touchcancel_callback("body", disp, EM_FALSE, touch_callback);

	emscripten_request_animation_frame_loop(redraw, win);
	emscripten_unwind_to_js_event_loop();
}
