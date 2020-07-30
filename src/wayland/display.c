#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-egl.h>
#include "wayland.h"

static void
capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps)
{
	struct mgu_seat *seat = data;
	if (caps & WL_SEAT_CAPABILITY_TOUCH) {
		if (!seat->touch) {
			seat->touch = wl_seat_get_touch(wl_seat);
		}
	} else if (seat->touch) {
		wl_touch_release(seat->touch), seat->touch = NULL;
	}

	if (caps & WL_SEAT_CAPABILITY_POINTER) {
		if (!seat->pointer) {
			seat->pointer = wl_seat_get_pointer(wl_seat);
		}
	} else if (seat->pointer) {
		wl_pointer_release(seat->pointer), seat->pointer = NULL;
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
	}
}
static void
global_dump(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version)
{
	fprintf(stderr, "%s\n", interface);
}
static void
global_remove(void *data, struct wl_registry *registry,
        uint32_t name)
{
}
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

	res = wl_display_roundtrip(disp->disp);
	if (res == -1) {
		goto cleanup_disp;
	}

	if (!(disp->comp && disp->seat.seat && disp->wm)) {
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

