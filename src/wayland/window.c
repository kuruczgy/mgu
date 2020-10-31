#include <poll.h>
#include <stdio.h>
#include <mgu/win.h>

static void schedule_frame(struct mgu_win *win);

static void redraw(struct mgu_win *win, float t)
{
	if (win->render_cb.f(win->render_cb.env, t)) {
		eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
	}
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

// static void surface_enter(void *data, struct wl_surface *surface,
// 		struct wl_output *output) {
// 	struct mgu_win *win = data;
// 	if (win->disp->output == output) {
// 
// 	}
// }
// static void surface_leave(void *data, struct wl_surface *surface,
// 		struct wl_output *output) {
// }
// 
// static struct wl_surface_listener surf_lis = {
// 	.enter = surface_enter,
// 	.leave = surface_leave,
// };

static void configure_common(struct mgu_win *win, int32_t size[static 2]) {
	int32_t scale = win->disp->out.scale;
	win->size[0] = size[0], win->size[1] = size[1];
	wl_surface_set_buffer_scale(win->surf, scale);
	wl_egl_window_resize(win->native,
		size[0] * scale, size[1] * scale, 0, 0);
}
static void configure_ack_common(struct mgu_win *win) {
	if (win->wait_for_configure) {
		eglSwapBuffers(win->disp->egl_dpy, win->egl_surf);
		schedule_frame(win);
		win->wait_for_configure = false;
	}
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct mgu_win *win = data;
	configure_common(win, (int32_t[]){ width, height });
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	configure_ack_common(win);
}
static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct mgu_win *win = data;
	win->req_close = true;
}
struct zwlr_layer_surface_v1_listener layer_surf_lis = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void handle_xdg_surface_configure(void *data, struct xdg_surface
		*surface, uint32_t serial) {
	fprintf(stderr, "configure!\n");
	xdg_surface_ack_configure(surface, serial);
	configure_ack_common(data);
}
static void handle_xdg_toplevel_configure(void *data, struct xdg_toplevel
		*xdg_toplevel, int32_t width, int32_t height, struct wl_array
		*state) {
	configure_common(data, (int32_t[]){ width, height });
}
static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel
		*xdg_toplevel) {
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
cleanup_native:
	wl_egl_window_destroy(win->native);
cleanup_none:
	return res;
}

static int init_xdg(struct mgu_win *win) {
	int res;

	win->xdg.surf = xdg_wm_base_get_xdg_surface(win->disp->wm, win->surf);
	if (!win->xdg.surf) {
		res = -1;
		goto cleanup_none;
	}

	res = xdg_surface_add_listener(win->xdg.surf, &xdg_surf_lis, win);
	if (res == -1) {
		goto cleanup_xdg_surf;
	}

	win->xdg.toplevel = xdg_surface_get_toplevel(win->xdg.surf);
	if (!win->xdg.toplevel) {
		res = -1;
		goto cleanup_xdg_surf;
	}

	res = xdg_toplevel_add_listener(win->xdg.toplevel, &toplevel_lis, win);
	if (res == -1) {
		goto cleanup_toplevel;
	}

	res = 0;
	goto cleanup_none;
cleanup_toplevel:
	xdg_toplevel_destroy(win->xdg.toplevel);
cleanup_xdg_surf:
	xdg_surface_destroy(win->xdg.surf);
cleanup_none:
	return res;
}
static int init_layer(struct mgu_win *win) {
	int res;

	win->layer.surf = zwlr_layer_shell_v1_get_layer_surface(
		win->disp->layer_shell,
		win->surf,
		win->disp->out.out,
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		"panel"
	);
	if (!win->layer.surf) {
		res = -1;
		goto cleanup_none;
	}

	res = zwlr_layer_surface_v1_add_listener(win->layer.surf,
		&layer_surf_lis, win);
	if (res == -1) {
		goto cleanup_layer_surf;
	}

	zwlr_layer_surface_v1_set_size(win->layer.surf,
		win->layer.size[0], win->layer.size[1]);
	zwlr_layer_surface_v1_set_anchor(win->layer.surf, win->layer.anchor);
	zwlr_layer_surface_v1_set_exclusive_zone(win->layer.surf,
		win->layer.exclusive_zone);

	res = 0;
	goto cleanup_none;
cleanup_layer_surf:
	zwlr_layer_surface_v1_destroy(win->layer.surf);
cleanup_none:
	return res;
}

static int init_common(struct mgu_win *win, struct mgu_disp *disp) {
	int res;
	win->disp = disp;
	win->size[0] = win->size[1] = 800; /* default dummy size */

	win->surf = wl_compositor_create_surface(disp->comp);
	if (!win->surf) {
		res = -1;
		goto cleanup_none;
	}

	if (win->type == MGU_WIN_XDG) {
		res = init_xdg(win);
	} else {
		res = init_layer(win);
	}
	if (res != 0) {
		goto cleanup_surf;
	}

	win->wait_for_configure = true;
	wl_surface_commit(win->surf);

	if (win_init_egl(win) != 0) {
		res = -1;
		goto cleanup_surf;
	}

	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_surf;
	}

	res = 0;
	goto cleanup_none;
cleanup_surf:
	wl_surface_destroy(win->surf);
cleanup_none:
	return res;
}

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp) {
	return mgu_win_init_xdg(win, disp);
}
int mgu_win_init_xdg(struct mgu_win *win, struct mgu_disp *disp) {
	win->type = MGU_WIN_XDG;
	return init_common(win, disp);
}
int mgu_win_init_layer_bottom_panel(struct mgu_win *win, struct mgu_disp *disp,
		uint32_t size) {
	win->type = MGU_WIN_LAYER;
	win->layer.size[1] = size;
	win->layer.exclusive_zone = size;
	win->layer.anchor = 
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	return init_common(win, disp);
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
