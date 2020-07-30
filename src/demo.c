#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libtouch.h>
#include <linux/input-event-codes.h>
#include "linalg.h"
#include "render.h"
#include "wayland.h"

struct app {
	struct mgu_disp disp;
	struct mgu_win win;
	GLuint program, attrib_pos, uni_color, uni_tran;

	struct libtouch_surface *touch;
	struct libtouch_area *touch_area;
	float pointer_pos[2];
	bool pointer_pressed;
};

static void
wl_touch_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time,
		struct wl_surface *surface,
		int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	struct app *app = data;
	libtouch_surface_down(app->touch, time, id,
		(float[]){ wl_fixed_to_double(x), wl_fixed_to_double(y) });
}

static void
wl_touch_up(void *data, struct wl_touch *wl_touch,
	   uint32_t serial, uint32_t time, int32_t id)
{
	struct app *app = data;
	libtouch_surface_up(app->touch, time, id);
}

static void
wl_touch_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	struct app *app = data;
	libtouch_surface_motion(app->touch, time, id,
		(float[]){ wl_fixed_to_double(x), wl_fixed_to_double(y) });
}

static void
wl_touch_frame(void *data, struct wl_touch *wl_touch)
{
}

static void
wl_touch_cancel(void *data, struct wl_touch *wl_touch)
{
}

static void
wl_touch_shape(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t major, wl_fixed_t minor)
{
}

static void
wl_touch_orientation(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t orientation)
{
}

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
		wl_fixed_t fx, wl_fixed_t fy) {
	struct app *app = data;
	app->pointer_pos[0] = wl_fixed_to_double(fx);
	app->pointer_pos[1] = wl_fixed_to_double(fy);
}
static void pointer_leave(void *data, struct wl_pointer *pointer,
		uint32_t serial, struct wl_surface *surface) {
}
static void pointer_motion(void *data, struct wl_pointer *pointer,
		uint32_t time, wl_fixed_t fx, wl_fixed_t fy) {
	struct app *app = data;
	app->pointer_pos[0] = wl_fixed_to_double(fx);
	app->pointer_pos[1] = wl_fixed_to_double(fy);
	if (app->pointer_pressed) {
		libtouch_surface_motion(app->touch, time, 1, app->pointer_pos);
		struct libtouch_rt rt = libtouch_area_get_transform(app->touch_area);
		fprintf(stderr, "libtouch_rt: t[%f %f] s[%f] r[%f]\n",
			rt.t1, rt.t2, rt.s, rt.r);
	}
}
static void pointer_button(void *data, struct wl_pointer *pointer,
		uint32_t serial, uint32_t time,
		uint32_t button, uint32_t state) {
	struct app *app = data;
	if (button & BTN_LEFT) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			if (!app->pointer_pressed) {
				libtouch_surface_down(app->touch,
					time, 1, app->pointer_pos);
			}
			app->pointer_pressed = true;
		} else {
			if (app->pointer_pressed) {
				libtouch_surface_up(app->touch, time, 1);
			}
			app->pointer_pressed = false;
		}
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

void render(void *cl, float t)
{
	struct app *app = cl;

	glUseProgram(app->program);
	glViewport(0, 0, app->win.size[0], app->win.size[1]);

	float T[9];
	mat3_ident(T);

	mat3_scale(T, (float[]){ 100, 100 });

	struct libtouch_rt rt = libtouch_area_get_transform(app->touch_area);
	mat3_tran(T, rt.t);

	mat3_proj(T, app->win.size);
	mat3_t(T);
	glUniformMatrix3fv(app->uni_tran, 1, GL_FALSE, T);

	static const GLfloat a_pos[] = { 0, 0, 0, 1, 1, 0, 1, 1 };
	glVertexAttribPointer(app->attrib_pos, 2, GL_FLOAT, GL_FALSE, 0, a_pos);
	glEnableVertexAttribArray(app->attrib_pos);

	glUniform4f(app->uni_color, 1, 0, 0, 1);

	glClearColor(1.0, 1.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glFlush();
}

int main()
{
	int res;

	struct app app;
	memset(&app, 0, sizeof(struct app));
	if (mgu_disp_init(&app.disp) != 0) {
		res = 1;
		goto cleanup_none;
	}

	if (mgu_win_init(&app.win, &app.disp, &app, &render) != 0) {
		res = 1;
		goto cleanup_disp;
	}

	if (app.disp.seat.touch) {
		fprintf(stderr, "adding touch listener\n");
		wl_touch_add_listener(app.disp.seat.touch, &touch_lis, &app);
	}
	else if (app.disp.seat.pointer) {
		fprintf(stderr, "adding pointer listener\n");
		wl_pointer_add_listener(app.disp.seat.pointer,
			&pointer_lis, &app);
	}
	
	app.touch = libtouch_surface_create();
	app.touch_area = libtouch_surface_add_area(app.touch,
		(struct aabb) { .x = 0, .y = 0, .w = 10000, .h = 10000 });

	app.program = mgu_shader_program(mgu_shader_vert_simple,
		mgu_shader_frag_color);
	if (!app.program) {
		res = 1;
		goto cleanup_win;
	}
	app.attrib_pos = glGetAttribLocation(app.program, "pos");
	app.uni_color = glGetUniformLocation(app.program, "color");
	app.uni_tran = glGetUniformLocation(app.program, "tran");

	mgu_win_run(&app.win);

	res = 0;

cleanup_win:
	mgu_win_finish(&app.win);
cleanup_disp:
	mgu_disp_finish(&app.disp);
cleanup_none:
	return res;
}
