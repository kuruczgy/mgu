#ifndef MGU_WAYLAND_H
#define MGU_WAYLAND_H
#include <EGL/egl.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell-client-protocol.h"

struct mgu_seat {
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
};

struct mgu_disp {
	struct wl_display *disp;
	struct wl_registry *reg;
	struct wl_compositor *comp;
	struct xdg_wm_base *wm;
	struct mgu_seat seat;

	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	EGLContext egl_ctx;
};

typedef void(*mgu_fn_render)(void *cl, float t);

struct mgu_win {
	struct mgu_disp *disp;
	struct wl_surface *surf;
	struct xdg_surface *xdg_surf;
	struct xdg_toplevel *toplevel;
	bool wait_for_configure, req_close;
	struct wl_callback *frame_cb;
	void *render_cl;
	mgu_fn_render render_fn;

	int size[2];

	/* egl stuff */
	struct wl_egl_window *native;
	EGLSurface egl_surf;
};

/* All init functions MUST be given a zeroed memory area. */

int mgu_disp_init(struct mgu_disp *disp);
void mgu_disp_finish(struct mgu_disp *disp);

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp,
	void *cl, mgu_fn_render render);
void mgu_win_finish(struct mgu_win *win);

void mgu_win_run(struct mgu_win *win);

/* globals */
extern const struct wl_registry_listener mgu_wl_registry_listener_dump;

#endif
