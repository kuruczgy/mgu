#ifndef MGU_WAYLAND_H
#define MGU_WAYLAND_H
#include <stdbool.h>
#include <mgu/input.h>
#include <ds/vec.h>
#include <EGL/egl.h>

#ifdef __EMSCRIPTEN__
#else
#include <wayland-client.h>
#include <wayland-egl.h>

#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#endif

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

struct mgu_seat {
	struct mgu_seat_cb cb;
	double pointer_p[2];
#ifdef __EMSCRIPTEN__
#else
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;

	struct mgu_touch_point {
		int32_t id;
		struct mgu_win_surf *surf;
	};
	struct vec touch_points; // vec<struct mgu_touch_point>
	struct mgu_win_surf *pointer_surf, *keyboard_surf;

	struct mgu_disp *disp;
#endif
};

struct mgu_out {
	int32_t size_mm[2];
	int32_t res_px[2];
	int32_t scale;
	double ppmm;
#ifdef __EMSCRIPTEN__
	double devicePixelRatio;
#else
	struct wl_output *out;
	bool configured;
#endif
};

struct mgu_disp {
	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	EGLContext egl_ctx;

	struct mgu_render_cb render_cb;
	struct vec surfaces; // vec<struct mgu_win_surf *>

	struct mgu_seat seat;
#ifdef __EMSCRIPTEN__
	struct mgu_out out;
#else
	struct wl_display *disp;
	struct wl_registry *reg;
	struct wl_compositor *comp;
	struct vec outputs; // vec<struct mgu_out>
	struct xdg_wm_base *wm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct mgu_global_cb {
		void *env;
		void (*f)(void *env, struct wl_registry *reg, uint32_t id,
			const char *i, uint32_t version);
	} global_cb;

	double diagonal; /* physical diagonal of display (mm) */

	bool req_stop;

#endif
};

struct mgu_win_surf {
	EGLSurface egl_surf;

	struct mgu_disp *disp;
	uint32_t size[2];

	enum mgu_win_type {
#ifdef __EMSCRIPTEN__
		MGU_WIN_CANVAS,
#else
		MGU_WIN_XDG,
		MGU_WIN_LAYER,
#endif
	} type;
	union {
#ifdef __EMSCRIPTEN__
		struct {

		} canvas;
#else
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
#endif
	};
#ifdef __EMSCRIPTEN__
#else
	struct wl_egl_window *native;
	struct wl_surface *surf;
	bool wait_for_configure, req_close;
	bool dirty;
	struct wl_callback *frame_cb;
#endif
};

/* All init functions MUST be given a zeroed memory area. */
int mgu_disp_init(struct mgu_disp *disp);
void mgu_disp_finish(struct mgu_disp *disp);
void mgu_disp_run(struct mgu_disp *disp);
struct mgu_win_surf *mgu_disp_add_surf_default(struct mgu_disp *disp,
	const char *title);
struct mgu_out *mgu_disp_get_default_output(struct mgu_disp *disp);
void mgu_win_surf_mark_dirty(struct mgu_win_surf *surf);
#ifdef __EMSCRIPTEN__
struct mgu_win_surf *mgu_disp_add_surf_canvas(struct mgu_disp *disp);
#else

int mgu_disp_init_custom(struct mgu_disp *disp, struct mgu_global_cb global_cb);
int mgu_disp_get_fd(struct mgu_disp *disp);
int mgu_disp_dispatch(struct mgu_disp *disp);
void mgu_disp_remove_surf(struct mgu_disp *disp, struct mgu_win_surf *surf);

void mgu_disp_mark_all_surfs_dirty(struct mgu_disp *disp);

struct mgu_win_surf *mgu_disp_add_surf_xdg(struct mgu_disp *disp,
	const char *title);
struct mgu_win_surf *mgu_disp_add_surf_layer_bottom_panel(struct mgu_disp *disp,
	uint32_t size);
struct mgu_win_surf *mgu_disp_add_surf_layer_overlay_for_each_output(
	struct mgu_disp *disp);

void mgu_disp_force_redraw(struct mgu_disp *disp);

/* globals */
extern const struct wl_registry_listener mgu_wl_registry_listener_dump;
#endif

#endif
