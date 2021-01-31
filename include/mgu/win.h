#ifndef MGU_WIN_H
#define MGU_WIN_H
#include <stdbool.h>
#include <mgu/input.h>
#include <ds/vec.h>
#include <EGL/egl.h>
#include <platform_utils/event_loop.h>

#define DEBUG_FRAME_RATE 0

#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
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
	bool (*f)(void *env, struct mgu_win_surf *surf, uint64_t msec);
};
struct mgu_context_cb {
	void *env;
	void (*f)(void *env, bool have_ctx);
};

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
struct mgu_touch_point {
	int32_t id;
	struct mgu_win_surf *surf;
};
#endif

struct mgu_seat {
	struct mgu_seat_cb cb;
	double pointer_p[2];
#if defined(__EMSCRIPTEN__)
#elif defined(__ANDROID__)
#else
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;

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
#if defined(__EMSCRIPTEN__)
	double devicePixelRatio;
#elif defined(__ANDROID__)
#else
	struct wl_output *out;
	bool configured;
#endif
};

struct mgu_disp {
	/* egl stuff */
	EGLDisplay egl_dpy;
	EGLConfig egl_conf;
	bool have_egl_ctx;
	EGLContext egl_ctx;

	struct mgu_context_cb context_cb;
	struct mgu_render_cb render_cb;
	struct vec surfaces; // vec<struct mgu_win_surf *>

	struct mgu_seat seat;

	struct platform *plat;
	struct event_loop *el;
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
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
	bool egl_inited;

#if DEBUG_FRAME_RATE
	float frame_rate;
	long long int frame_counter_since;
	int frame_counter_n;
#endif

	struct mgu_disp *disp;
	uint32_t size[2];

	enum mgu_win_type {
#if defined(__EMSCRIPTEN__)
		MGU_WIN_CANVAS,
#elif defined(__ANDROID__)
		MGU_WIN_NATIVE_ACTIVITY,
#else
		MGU_WIN_XDG,
		MGU_WIN_LAYER,
#endif
	} type;
	union {
#if defined(__EMSCRIPTEN__)
		// struct { } canvas;
#elif defined(__ANDROID__)
		// struct { } native_activity;
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
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
	struct wl_egl_window *native;
	struct wl_surface *surf;
	bool wait_for_configure, req_close;
	bool dirty;
	struct wl_callback *frame_cb;
#endif
};

/* All init functions MUST be given a zeroed memory area. */
int mgu_disp_init(struct mgu_disp *disp, struct platform *plat);
void mgu_disp_finish(struct mgu_disp *disp);
struct mgu_win_surf *mgu_disp_add_surf_default(struct mgu_disp *disp,
	const char *title);
struct mgu_out *mgu_disp_get_default_output(struct mgu_disp *disp);
void mgu_win_surf_mark_dirty(struct mgu_win_surf *surf);
void mgu_disp_add_to_event_loop(struct mgu_disp *disp, struct event_loop *el);
void mgu_disp_force_redraw(struct mgu_disp *disp);
void mgu_disp_set_context_cb(struct mgu_disp *disp, struct mgu_context_cb cb);
#if defined(__EMSCRIPTEN__)
struct mgu_win_surf *mgu_disp_add_surf_canvas(struct mgu_disp *disp);
#elif defined(__ANDROID__)
struct mgu_win_surf *mgu_disp_add_surf_native_activity(struct mgu_disp *disp);
#else

int mgu_disp_init_custom(struct mgu_disp *disp, struct platform *plat,
	struct mgu_global_cb global_cb);
void mgu_disp_remove_surf(struct mgu_disp *disp, struct mgu_win_surf *surf);

void mgu_disp_mark_all_surfs_dirty(struct mgu_disp *disp);

struct mgu_win_surf *mgu_disp_add_surf_xdg(struct mgu_disp *disp,
	const char *title);
struct mgu_win_surf *mgu_disp_add_surf_layer_bottom_panel(struct mgu_disp *disp,
	uint32_t size);
struct mgu_win_surf *mgu_disp_add_surf_layer_overlay_for_each_output(
	struct mgu_disp *disp);

/* globals */
extern const struct wl_registry_listener mgu_wl_registry_listener_dump;
#endif

#endif
