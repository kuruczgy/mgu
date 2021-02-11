#ifndef MGU_SR_H
#define MGU_SR_H
#include <platform_utils/main.h>
#include <mgu/gl.h>

struct sr;

enum sr_type {
	SR_RECT,
	SR_TEX,
	SR_TEXT,
	SR_TYPE_N
};

enum sr_align_opts {
	SR_CENTER_V = 1 << 0,
	SR_CENTER_H = 1 << 1,
	SR_STRETCH = 1 << 2,
	SR_TEX_PASS_OWNERSHIP = 1 << 3,
	SR_CENTER = SR_CENTER_V | SR_CENTER_H,
};

struct sr_spec {
	enum sr_type t;
	float p[4];
	uint32_t argb;
	enum sr_align_opts o;
	union {
		struct mgu_texture tex;
		struct {
			const char *s;
			int px;
		} text;
	};
};

struct sr *sr_create_opengl(struct platform *plat);
void sr_destroy(struct sr *sr);
void sr_put(struct sr *sr, struct sr_spec spec);
void sr_measure(struct sr *sr, float p[static 2], struct sr_spec spec);
void sr_clip_push(struct sr *sr, const float aabb[static 4]);
void sr_clip_pop(struct sr *sr);
void sr_present(struct sr *sr, const uint32_t win_size[static 2]);

/*
Example usage:

sr_put(sr, (struct sr_spec){
	.t = SR_RECT,
	.p = { 0, 0, 100, 100 },
	.argb = 0xFF000000,
});

sr_put(sr, (struct sr_spec){
	.t = SR_TEXT,
	.p = { 0, 0, 100, 100 },
	.argb = 0xFF000000,
	.text = {
		.s = "asdfg",
		.w = 16,
		.o = SR_CENTER
	},
});
*/

#endif
