#ifndef MGU_WAYLAND_H
#define MGU_WAYLAND_H
#include <stdbool.h>
#include <mgu/input.h>

struct mgu_win_surf;

struct mgu_seat_cb {
	void *env;
	void (*f)(void *env, struct mgu_win_surf *surf,
		struct mgu_input_event_args ev);
};
struct mgu_render_cb {
	void *env;
	bool (*f)(void *env, struct mgu_win_surf *surf, float t);
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

int mgu_win_init(struct mgu_win *win, struct mgu_disp *disp, const char *title);
void mgu_win_finish(struct mgu_win *win);

void mgu_win_run(struct mgu_win *win);

#else

#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <ds/vec.h>
#include <ds/hashmap.h>

struct mgu_touch_point {
	int32_t id;
	struct mgu_win_surf *surf;
};
struct mgu_seat {
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;
	struct mgu_seat_cb cb;

	struct vec touch_points; // vec<struct mgu_touch_point>
	struct mgu_win_surf *pointer_surf, *keyboard_surf;
	double pointer_p[2];

	struct mgu_disp *disp;
};
struct mgu_out {
	struct wl_output *out;
	int32_t size_mm[2];
	int32_t res_px[2];
	int32_t scale;
	double ppmm;
	bool configured;
};

struct mgu_global_cb {
	void *env;
	void (*f)(void *env, struct wl_registry *reg, uint32_t id, const char *i, uint32_t version);
};
struct mgu_disp {
	struct wl_display *disp;
	struct wl_registry *reg;
	struct wl_compositor *comp;
	struct vec outputs; // vec<struct mgu_out>
	struct xdg_wm_base *wm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct mgu_seat seat;
	struct mgu_global_cb global_cb;

	double diagonal; /* physical diagonal of display (mm) */

	struct mgu_render_cb render_cb;
	struct vec surfaces; // vec<struct mgu_win_surf *>

	bool req_stop;

	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	EGLContext egl_ctx;
};

enum mgu_win_type {
	MGU_WIN_XDG,
	MGU_WIN_LAYER
};
struct mgu_win_surf {
	enum mgu_win_type type;
	struct wl_surface *surf;
	union {
		struct {
			struct xdg_surface *surf;
			struct xdg_toplevel *toplevel;
		} xdg;
		struct {
			struct zwlr_layer_surface_v1 *surf;
			int32_t exclusive_zone;
			enum zwlr_layer_surface_v1_anchor anchor;
			enum zwlr_layer_shell_v1_layer layer;
		} layer;
	};
	uint32_t size[2];

	bool wait_for_configure, req_close;
	struct wl_callback *frame_cb;

	/* egl stuff */
	struct wl_egl_window *native;
	EGLSurface egl_surf;

	struct mgu_disp *disp;
};

/* All init functions MUST be given a zeroed memory area. */

int mgu_disp_init(struct mgu_disp *disp);
int mgu_disp_init_custom(struct mgu_disp *disp, struct mgu_global_cb global_cb);
void mgu_disp_finish(struct mgu_disp *disp);
int mgu_disp_get_fd(struct mgu_disp *disp);
int mgu_disp_dispatch(struct mgu_disp *disp);
struct mgu_out *mgu_disp_get_default_output(struct mgu_disp *disp);
void mgu_disp_remove_surf(struct mgu_disp *disp, struct mgu_win_surf *surf);

struct mgu_win_surf *mgu_disp_add_surf_default(struct mgu_disp *disp,
	const char *title);
struct mgu_win_surf *mgu_disp_add_surf_xdg(struct mgu_disp *disp,
	const char *title);
struct mgu_win_surf *mgu_disp_add_surf_layer_bottom_panel(struct mgu_disp *disp,
	uint32_t size);
struct mgu_win_surf *mgu_disp_add_surf_layer_overlay_for_each_output(
	struct mgu_disp *disp);

void mgu_disp_run(struct mgu_disp *disp);

void mgu_disp_force_redraw(struct mgu_disp *disp);

/* globals */
extern const struct wl_registry_listener mgu_wl_registry_listener_dump;
#endif

#endif
