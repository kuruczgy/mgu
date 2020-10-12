#ifndef MGU_WAYLAND_H
#define MGU_WAYLAND_H
#include <stdbool.h>
#include <mgu/input.h>

struct mgu_seat_cb {
	void (*f)(void *cl, struct mgu_input_event_args ev);
	void *cl;
};
struct mgu_render_cb {
	bool (*f)(void *cl, float t);
	void *cl;
};

#ifdef __EMSCRIPTEN__
#include <EGL/egl.h>

struct mgu_seat {
	struct mgu_seat_cb cb;
	double pointer_p[2];
};
struct mgu_out {
	int32_t size_mm[2];
	int32_t res_px[2];
	int32_t scale;
	double ppmm;
	double devicePixelRatio;
};
struct mgu_disp {
	struct mgu_out out;
	struct mgu_seat seat;
	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	EGLContext egl_ctx;
};

struct mgu_win {
	struct mgu_disp *disp;
	struct mgu_render_cb render_cb;
	int size[2];

	/* egl stuff */
	struct wl_egl_window *native;
	EGLSurface egl_surf;
};

/* All init functions MUST be given a zeroed memory area. */

int mgu_disp_init(struct mgu_disp *disp);
void mgu_disp_finish(struct mgu_disp *disp);

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp);
void mgu_win_finish(struct mgu_win *win);

void mgu_win_run(struct mgu_win *win);

#else

#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
struct mgu_seat {
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;
	struct mgu_seat_cb cb;

	double pointer_p[2];
};
struct mgu_out {
	struct wl_output *out;
	int32_t size_mm[2];
	int32_t res_px[2];
	int32_t scale;
	double ppmm;
	bool configured;
};

struct mgu_disp {
	struct wl_display *disp;
	struct wl_registry *reg;
	struct wl_compositor *comp;
	struct mgu_out out; // TODO: multiple screens
	struct xdg_wm_base *wm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct mgu_seat seat;

	double diagonal; /* physical diagonal of display (mm) */

	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	EGLContext egl_ctx;
};

enum mgu_win_type {
	MGU_WIN_XDG,
	MGU_WIN_LAYER
};
struct mgu_win {
	enum mgu_win_type type;
	struct mgu_disp *disp;
	struct wl_surface *surf;
	union {
		struct {
			struct xdg_surface *surf;
			struct xdg_toplevel *toplevel;
		} xdg;
		struct {
			struct zwlr_layer_surface_v1 *surf;
			uint32_t size[2];
			int32_t exclusive_zone;
			enum zwlr_layer_surface_v1_anchor anchor;
		} layer;
	};
	bool wait_for_configure, req_close;
	struct wl_callback *frame_cb;
	struct mgu_render_cb render_cb;

	int size[2];

	/* egl stuff */
	struct wl_egl_window *native;
	EGLSurface egl_surf;
};

/* All init functions MUST be given a zeroed memory area. */

int mgu_disp_init(struct mgu_disp *disp);
void mgu_disp_finish(struct mgu_disp *disp);

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp);
int mgu_win_init_xdg(struct mgu_win *win, struct mgu_disp *disp);
int mgu_win_init_layer_bottom_panel(struct mgu_win *win, struct mgu_disp *disp,
	uint32_t size);
void mgu_win_finish(struct mgu_win *win);

void mgu_win_run(struct mgu_win *win);

/* globals */
extern const struct wl_registry_listener mgu_wl_registry_listener_dump;
#endif

#endif
