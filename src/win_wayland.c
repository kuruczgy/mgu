#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-egl.h>
#include <mgu/win.h>
#include <sys/mman.h>
#include <poll.h>

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
		double p = hypot(out->res_px[0], out->res_px[1]);
		double mm = hypot(out->size_mm[0], out->size_mm[1]);
		out->ppmm = p / mm;
		out->ppmm /= out->scale;
		fprintf(stderr, "ppmm: %f\n", out->ppmm);

		out->configured = true;
	}
}
static void output_handle_scale(void* data, struct wl_output *wl_output,
	int32_t factor) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = find_out(disp, wl_output);
	if (out) {
		fprintf(stderr, "scale: %d\n", factor);
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

	disp->egl_dpy = eglGetPlatformDisplay(
		EGL_PLATFORM_WAYLAND_KHR, disp->disp, NULL);
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

int mgu_disp_init_custom(struct mgu_disp *disp,
		struct mgu_global_cb global_cb) {
	disp->global_cb = global_cb;
	return mgu_disp_init(disp);
}
int mgu_disp_init(struct mgu_disp *disp)
{
	int res;

	disp->disp = wl_display_connect(NULL);
	if (!disp->disp) {
		res = -1; /* wl_display_connect failed */
		goto cleanup_none;
	}

	if (disp_init_egl(disp) != 0) {
		res = -1;
		goto cleanup_disp;
	}

	disp->reg = wl_display_get_registry(disp->disp);
	if (!disp->reg) {
		res = -1; /* TODO: can this even happen? */
		goto cleanup_disp;
	}

	res = wl_registry_add_listener(disp->reg, &reg_lis, disp);
	if (res == -1) {
		goto cleanup_disp;
	}

	disp->outputs = vec_new_empty(sizeof(struct mgu_out));
	disp->surfaces = vec_new_empty(sizeof(struct mgu_win_surf *));
	disp->seat.touch_points = vec_new_empty(sizeof(struct mgu_touch_point));
	disp->seat.disp = disp;

	// TODO: please...
	res = wl_display_roundtrip(disp->disp);
	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_disp;
	}

	struct mgu_out *out = mgu_disp_get_default_output(disp);
	if (!(disp->comp
			&& disp->seat.seat
			&& disp->wm
			&& disp->layer_shell
			&& out
			&& out->configured)) {
		res = -1;
		goto cleanup_disp;
	}

	res = 0;
	goto cleanup_none;
cleanup_disp:
	wl_display_disconnect(disp->disp);
cleanup_none:
	return res;
}

static void surf_destroy(struct mgu_win_surf *surf) {
	if (surf->frame_cb) {
		wl_callback_destroy(surf->frame_cb);
		surf->frame_cb = NULL;
	}

	eglDestroySurface(surf->disp->egl_dpy, surf->egl_surf);
	wl_egl_window_destroy(surf->native);

	if (surf->type == MGU_WIN_LAYER) {
		zwlr_layer_surface_v1_destroy(surf->layer.surf);
		wl_surface_destroy(surf->surf);
	} else {
		/* TODO */
	}

	free(surf);
}
void mgu_disp_finish(struct mgu_disp *disp)
{
	if (disp->reg) {
		wl_registry_destroy(disp->reg);
	}

	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		surf_destroy(surf);
	}
	vec_free(&disp->surfaces);

	/* TODO */
}

int mgu_disp_get_fd(struct mgu_disp *disp) {
	return wl_display_get_fd(disp->disp);
}
int mgu_disp_dispatch(struct mgu_disp *disp) {
	while (wl_display_prepare_read(disp->disp) != 0)
		wl_display_dispatch_pending(disp->disp);

	wl_display_read_events(disp->disp);
	wl_display_dispatch_pending(disp->disp);
	wl_display_flush(disp->disp);

	return 0;
}
struct mgu_out *mgu_disp_get_default_output(struct mgu_disp *disp) {
	if (disp->outputs.len > 0) {
		return vec_get(&disp->outputs, 0);
	}
	return NULL;
}

static void schedule_frame(struct mgu_win_surf *surf);

static void redraw(struct mgu_win_surf *surf, float t)
{
	EGLBoolean ret = eglMakeCurrent(surf->disp->egl_dpy, surf->egl_surf,
		surf->egl_surf, surf->disp->egl_ctx);
	if (ret != EGL_TRUE) {
		return; // TODO: error
	}
	if (surf->disp->render_cb.f
		&& surf->disp->render_cb.f(
			surf->disp->render_cb.env, surf, t)
		) {
		eglSwapBuffers(surf->disp->egl_dpy, surf->egl_surf);
	}
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

	redraw(surf, time / 1000.0f);
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

static void configure_common(struct mgu_win_surf *surf,
		int32_t size[static 2]) {
	struct mgu_out *out = mgu_disp_get_default_output(surf->disp); // TODO: obviously not correct...
	int32_t scale = out->scale;
	surf->size[0] = size[0], surf->size[1] = size[1];
	wl_surface_set_buffer_scale(surf->surf, scale);
	wl_egl_window_resize(surf->native,
		size[0] * scale, size[1] * scale, 0, 0);
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
	((struct mgu_win_surf *)data)->req_close = true;
}
struct zwlr_layer_surface_v1_listener layer_surf_lis = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void handle_xdg_surface_configure(void *data,
		struct xdg_surface *surface, uint32_t serial) {
	fprintf(stderr, "configure!\n");
	xdg_surface_ack_configure(surface, serial);
	configure_ack_common(data);
}
static const struct xdg_surface_listener xdg_surf_lis = {
	handle_xdg_surface_configure
};

static void handle_xdg_toplevel_configure(void *data,
		struct xdg_toplevel *xdg_toplevel,
		int32_t width, int32_t height, struct wl_array *state) {
	configure_common(data, (int32_t[]){ width, height });
}
static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel
		*xdg_toplevel) {
	((struct mgu_win_surf *)data)->req_close = true;
}
static const struct xdg_toplevel_listener toplevel_lis = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
};

static int surf_init_egl(struct mgu_win_surf *surf, struct mgu_disp *disp)
{
	int res;

	surf->native = wl_egl_window_create(surf->surf, 1, 1); // dummy size
	if (!surf->native) {
		res = -1;
		goto cleanup_none;
	}

	surf->egl_surf = eglCreatePlatformWindowSurface(
		disp->egl_dpy, disp->egl_conf, surf->native, NULL);
	if (!surf->egl_surf) {
		res = -1;
		goto cleanup_native;
	}

	EGLBoolean ret = eglMakeCurrent(disp->egl_dpy, surf->egl_surf,
		surf->egl_surf, disp->egl_ctx);
	if (ret != EGL_TRUE) {
		// TODO: error
	}

	res = 0;
	goto cleanup_none;
cleanup_native:
	wl_egl_window_destroy(surf->native);
cleanup_none:
	return res;
}

static int init_surf_common(struct mgu_win_surf *surf, struct mgu_disp *disp) {
	int res;

	surf->disp = disp;
	surf->wait_for_configure = true;

	wl_surface_commit(surf->surf);

	if (surf_init_egl(surf, disp) != 0) {
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

	surf->xdg.surf = xdg_wm_base_get_xdg_surface(disp->wm, surf->surf);
	if (!surf->xdg.surf) {
		res = -1;
		goto cleanup_none;
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

struct mgu_win_surf *mgu_disp_add_surf_default(struct mgu_disp *disp, const char *title) {
	return mgu_disp_add_surf_xdg(disp, title);
}
struct mgu_win_surf *mgu_disp_add_surf_xdg(struct mgu_disp *disp, const char *title) {
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

void mgu_disp_run(struct mgu_disp *disp) {
	while (!disp->req_stop) {
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

void mgu_disp_force_redraw(struct mgu_disp *disp) {
	for (int i = 0; i < disp->surfaces.len; ++i) {
		struct mgu_win_surf *surf_i =
			*(struct mgu_win_surf **)vec_get(&disp->surfaces, i);
		redraw(surf_i, 0.0f);
	}
}
