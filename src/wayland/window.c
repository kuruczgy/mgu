#include <poll.h>
#include <stdio.h>
#include "wayland.h"

static void schedule_frame(struct mgu_win *win);

static void redraw(struct mgu_win *win, float t)
{
	win->render_fn(win->render_cl, t);
	eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
	schedule_frame(win);
}

static void
frame_cb(void *data, struct wl_callback *wl_callback, uint32_t time)
{
	struct mgu_win *win = data;
	if (win->frame_cb) {
		wl_callback_destroy(win->frame_cb);
		win->frame_cb = NULL;
	}

	redraw(win, time / 1000.0f);
}
static const struct wl_callback_listener frame_cb_lis = { .done = frame_cb };

static void schedule_frame(struct mgu_win *win)
{
	if (!win->frame_cb) {
		win->frame_cb = wl_surface_frame(win->surf);
		if (win->frame_cb) {
			wl_callback_add_listener(win->frame_cb,
				&frame_cb_lis, win);
			wl_surface_commit(win->surf);
		}
	}
}

static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface,
	uint32_t serial)
{
	fprintf(stderr, "configure!\n");
	struct mgu_win *win = data;
	xdg_surface_ack_configure(surface, serial);
	if (win->wait_for_configure) {
		eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
		schedule_frame(win);
		win->wait_for_configure = false;
	}
}
static void
handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
	int32_t width, int32_t height, struct wl_array *state)
{
	struct mgu_win *win = data;
	win->size[0] = width;
	win->size[1] = height;
	wl_egl_window_resize(win->native, win->size[0], win->size[1], 0, 0);
}
static void
handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	struct mgu_win *win = data;
	win->req_close = true;
}
static const struct xdg_surface_listener xdg_surf_lis = {
	handle_xdg_surface_configure
};
static const struct xdg_toplevel_listener toplevel_lis = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
};

static int win_init_egl(struct mgu_win *win)
{
	int res;
	EGLBoolean ret;
	struct mgu_disp *disp = win->disp;

	win->native = wl_egl_window_create(win->surf,win->size[0],win->size[1]);
	if (!win->native) {
		res = -1;
		goto cleanup_none;
	}

	win->egl_surf = eglCreatePlatformWindowSurface(
		disp->egl_dpy, disp->egl_conf, win->native, NULL);
	if (!win->egl_surf) {
		res = -1;
		goto cleanup_native;
	}

	res = 0;
	goto cleanup_none;
cleanup_surf:
	eglDestroySurface(disp->egl_dpy, win->egl_surf);
cleanup_native:
	wl_egl_window_destroy(win->native);
cleanup_none:
	return res;
}

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp,
	void *cl, mgu_fn_render render)
{
	int res;

	win->disp = disp;
	win->size[0] = win->size[1] = 800; /* default dummy size */

	win->render_cl = cl;
	win->render_fn = render;

	win->surf = wl_compositor_create_surface(disp->comp);
	if (!win->surf) {
		res = -1;
		goto cleanup_none;
	}

	if (win_init_egl(win) != 0) {
		res = -1;
		goto cleanup_surf;
	}

	win->xdg_surf = xdg_wm_base_get_xdg_surface(disp->wm, win->surf);
	if (!win->xdg_surf) {
		res = -1;
		goto cleanup_surf;
	}

	res = xdg_surface_add_listener(win->xdg_surf, &xdg_surf_lis, win);
	if (res == -1) {
		goto cleanup_xdg_surf;
	}

	win->toplevel = xdg_surface_get_toplevel(win->xdg_surf);
	if (!win->toplevel) {
		res = -1;
		goto cleanup_xdg_surf;
	}

	res = xdg_toplevel_add_listener(win->toplevel, &toplevel_lis, win);
	if (res == -1) {
		goto cleanup_toplevel;
	}

	win->wait_for_configure = true;
	wl_surface_commit(win->surf);

	/* TODO: who cleans up the egl stuff? */
	EGLBoolean ret = eglMakeCurrent(disp->egl_dpy, win->egl_surf,
		win->egl_surf, disp->egl_ctx);
	if (ret != EGL_TRUE) {
		res = -1;
		goto cleanup_toplevel;
	}

	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_toplevel;
	}

	res = 0;
	goto cleanup_none;
cleanup_toplevel:
	xdg_toplevel_destroy(win->toplevel);
cleanup_xdg_surf:
	xdg_surface_destroy(win->xdg_surf);
cleanup_surf:
	wl_surface_destroy(win->surf);
cleanup_none:
	return res;
}
void mgu_win_finish(struct mgu_win *win)
{
	/* TODO */
}

void mgu_win_run(struct mgu_win *win)
{
	struct mgu_disp *disp = win->disp;

	while (!win->req_close) {
		wl_display_flush(disp->disp);

		struct pollfd fds[1];
		fds[0].fd = wl_display_get_fd(disp->disp);
		fds[0].events = POLLIN | POLLERR | POLLHUP;
		if (poll(fds, 1, -1) == -1) {
			break;
		}

		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
		if (fds[0].revents & POLLIN) {
			if (wl_display_dispatch(disp->disp) == -1) {
				break;
			}
		}
	}
}
