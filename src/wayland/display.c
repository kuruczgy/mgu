#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-egl.h>
#include <mgu/win.h>

static void wl_point(wl_fixed_t x, wl_fixed_t y, double p[static 2]) {
	p[0] = wl_fixed_to_double(x);
	p[1] = wl_fixed_to_double(y);
}
static void input_event(struct mgu_seat *seat, struct mgu_input_event_args ev,
		uint32_t time) {
	ev.time = time;
	if (seat->cb.f) {
		seat->cb.f(seat->cb.env, ev);
	}
}
static void wl_touch_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time,
		struct wl_surface *surface,
		int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_DOWN };
	wl_point(x, y, ev.touch.down_or_move.p);
	ev.touch.id = id;
	input_event(data, ev, time);
}
static void wl_touch_up(void *data, struct wl_touch *wl_touch,
	   uint32_t serial, uint32_t time, int32_t id)
{
	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_UP };
	ev.touch.id = id;
	input_event(data, ev, time);
}
static void wl_touch_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_input_event_args ev = { .t = MGU_TOUCH | MGU_MOVE };
	wl_point(x, y, ev.touch.down_or_move.p);
	ev.touch.id = id;
	input_event(data, ev, time);
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
}
static void pointer_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface) { }
static void pointer_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t x, wl_fixed_t y) {
	struct mgu_seat *seat = data;
	wl_point(x, y, seat->pointer_p);

	struct mgu_input_event_args ev = { .t = MGU_POINTER | MGU_MOVE };
	memcpy(ev.pointer.move.p, seat->pointer_p, sizeof(double) * 2);
	input_event(data, ev, time);
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
		input_event(data, ev, time);
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
}
static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
}
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface) {
}
static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t _key_state) {
	enum wl_keyboard_key_state key_state = _key_state;
	if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		struct mgu_input_event_args ev = { .t = MGU_KEYBOARD | MGU_DOWN };
		ev.keyboard.down.key = key;
		input_event(data, ev, time);
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
	struct mgu_out *out = &disp->out;
	if (wl_output == out->out) {
		out->size_mm[0] = phys_width, out->size_mm[1] = phys_height;
	}
}
static void output_handle_mode(void *data, struct wl_output *wl_output,
		uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = &disp->out;
	if (wl_output == out->out) {
		out->res_px[0] = width;
		out->res_px[1] = height;
	}
}
static void output_handle_done(void* data, struct wl_output *wl_output) {
	struct mgu_disp *disp = data;
	struct mgu_out *out = &disp->out;
	if (wl_output == out->out) {
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
	struct mgu_out *out = &disp->out;
	if (wl_output == out->out) {
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
	} else if (!disp->out.out && strcmp(i, wl_output_interface.name) == 0) {
		disp->out.out = wl_registry_bind(reg, id, &wl_output_interface,
			wl_output_interface.version);
		if (disp->out.out) {
			wl_output_add_listener(disp->out.out,&output_lis,disp);
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

int mgu_disp_init_custom(struct mgu_disp *disp, struct mgu_global_cb global_cb) {
	disp->global_cb = global_cb;
	mgu_disp_init(disp);
}
int mgu_disp_init(struct mgu_disp *disp)
{
	int res;

	disp->out.ppmm = -1.0;
	disp->out.scale = 1;

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

	// TODO: please...
	res = wl_display_roundtrip(disp->disp);
	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_disp;
	}

	if (!(disp->comp
			&& disp->seat.seat
			&& disp->wm
			&& disp->layer_shell
			&& disp->out.configured)) {
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

void mgu_disp_finish(struct mgu_disp *disp)
{
	if (disp->reg) {
		wl_registry_destroy(disp->reg);
	}
	/* TODO */
}

