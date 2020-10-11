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

static EM_BOOL redraw(double t, void *env)
{
	struct mgu_win *win = env;
	win->render_cb.f(win->render_cb.cl, t / 1000.0);
	eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
	return EM_TRUE;
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

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp) {
	int res;
	win->disp = disp;

	win->size[0] = win->size[1] = 500;
	disp->out = (struct mgu_out){
		.size_mm = { 25, 25 },
		.res_px = { 150, 150 },
		.scale = 1
	};

	struct mgu_out *out = &disp->out;
	double p = hypot(out->res_px[0], out->res_px[1]);
	double mm = hypot(out->size_mm[0], out->size_mm[1]);
	out->ppmm = p / mm;
	out->ppmm /= out->scale;

	emscripten_set_canvas_element_size("#canvas", win->size[0], win->size[1]);

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

void mgu_win_run(struct mgu_win *win)
{
	emscripten_request_animation_frame_loop(redraw, win);
	emscripten_unwind_to_js_event_loop();
}
