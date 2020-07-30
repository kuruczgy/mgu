#ifndef MGU_INPUT_H
#define MGU_INPUT_H

enum mgu_input_ev {
	MGU_DOWN = 1 << 0,
	MGU_UP = 1 << 1,
	MGU_MOVE = 1 << 2,
	MGU_TOUCH = 1 << 3,
	MGU_POINTER = 1 << 4,
	MGU_BTN = 1 << 5,
};
struct mgu_input_event_args {
	enum mgu_input_ev t;
	uint32_t time;
	union {
		struct {
			int id;
			union {
				struct {
					double p[2];
				} down_or_move;
				struct { } up;
			};
		} touch;
		union {
			struct {
				double p[2];
			} move;
			struct {
				double p[2];
			} btn;
		} pointer;
	};
};

enum mgu_input_obj_type {
	MGU_INPUT_OBJ_TYPE_PRESS = 1 << 0,
	MGU_INPUT_OBJ_TYPE_TRAN = 1 << 1,
};

struct mgu_input_man;

struct mgu_input_obj {
	enum mgu_input_obj_type t;
	void *env;
	float *aabb;
	union {
		struct {
			void (*f)(void *env);
		} press;
		struct {
			void (*start)(void *env);
			void (*move)(void *env, const float d[static 2]);
			void (*end)(void *env);
		} tran;
	};
	struct mgu_input_man *im; /* private */
};

struct mgu_input_man *mgu_input_man_create(float touch_press_threshold);
void mgu_input_man_add(struct mgu_input_man *im, struct mgu_input_obj *in);
void mgu_input_man_remove(struct mgu_input_man *im, struct mgu_input_obj *in);
void mgu_input_man_report(struct mgu_input_man *im,
	struct mgu_input_event_args ev);
void mgu_input_man_destroy(struct mgu_input_man *im);

#endif
