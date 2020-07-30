#include <stdlib.h>
#include <math.h>
#include <libtouch.h>
#include <ds/vec.h>
#include <ds/matrix.h>
#include <mgu/input.h>

struct input_entry {
	struct mgu_input_obj *in;
	struct libtouch_area *area;
};

struct mgu_input_man {
	struct vec ins; /* vec<struct input_entry> */
	struct libtouch_surface *surf;
	float touch_press_threshold;
};

struct mgu_input_man *mgu_input_man_create(float touch_press_threshold) {
	struct mgu_input_man *im = malloc(sizeof(struct mgu_input_man));
	im->ins = vec_new_empty(sizeof(struct mgu_input_obj));
	im->surf = libtouch_surface_create();
	im->touch_press_threshold = touch_press_threshold;
	return im;
}
static void start_cb(void *env) {
	struct mgu_input_obj *in = env;
	if (in->t == MGU_INPUT_OBJ_TYPE_TRAN) {
		if (in->tran.start) {
			in->tran.start(in->env);
		}
	}
}
static void move_cb(void *env, struct libtouch_rt rt) {
	struct mgu_input_obj *in = env;
	if (in->t == MGU_INPUT_OBJ_TYPE_TRAN) {
		if (in->tran.move) {
			in->tran.move(in->env, rt.t);
		}
	}
}
static void end_cb(void *env, struct libtouch_rt rt) {
	struct mgu_input_obj *in = env;
	if (in->t == MGU_INPUT_OBJ_TYPE_TRAN) {
		if (in->tran.move) {
			in->tran.move(in->env, rt.t);
		}
		if (in->tran.end) {
			in->tran.end(in->env);
		}
	}
	if (in->t == MGU_INPUT_OBJ_TYPE_PRESS) {
		if (in->press.f) {
			if (hypotf(rt.t1, rt.t2) <
					in->im->touch_press_threshold) {
				in->press.f(in->env);
			}
		}
	}
}
void mgu_input_man_add(struct mgu_input_man *im, struct mgu_input_obj *in) {
	struct input_entry e = {
		.in = in,
		.area = libtouch_surface_add_area(
			im->surf, in->aabb, LIBTOUCH_T,
			(struct libtouch_area_ops){
				.env = in, .start = start_cb, .move = move_cb,
				.end = end_cb
			}
		),
	};
	in->im = im;
	vec_append(&im->ins, &e);
}
void mgu_input_man_remove(struct mgu_input_man *im, struct mgu_input_obj *in) {
	for (int i = 0; i < im->ins.len; ++i) {
		struct input_entry *e = vec_get(&im->ins, i);
		if (e->in == in) {
			libtouch_surface_remove_area(im->surf, e->area);
			vec_remove(&im->ins, i);
			break;
		}
	}
}
void mgu_input_man_report(struct mgu_input_man *im,
		struct mgu_input_event_args ev) {
	if (ev.t & MGU_TOUCH) {
		double *p = ev.touch.down_or_move.p;
		if (ev.t & MGU_DOWN) {
			libtouch_surface_down(im->surf, ev.time,
				ev.touch.id, (float[]){ p[0], p[1] });
		} else if (ev.t & MGU_UP) {
			libtouch_surface_up(im->surf, ev.time, ev.touch.id);
		} else if (ev.t & MGU_MOVE) {
			libtouch_surface_motion(im->surf, ev.time,
				ev.touch.id, (float[]){ p[0], p[1] });
		}
	}
	if (ev.t & MGU_POINTER && ev.t & MGU_BTN && ev.t & MGU_DOWN) {
		double *p = ev.pointer.btn.p;
		for (int i = 0; i < im->ins.len; ++i) {
			struct input_entry *e = vec_get(&im->ins, i);
			if (e->in->t != MGU_INPUT_OBJ_TYPE_PRESS) continue;
			if (!e->in->press.f) continue;
			if (aabb_contains(e->in->aabb,
					(float[]){ p[0], p[1]} )) {
				e->in->press.f(e->in->env);
			}
		}
	}
}
void mgu_input_man_destroy(struct mgu_input_man *im) {
	vec_free(&im->ins);
	free(im);
}
