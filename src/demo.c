#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libtouch.h>
#include <ds/matrix.h>
#include <mgu/gl.h>
#include <mgu/sr.h>
#include <mgu/text.h>
#include <mgu/win.h>
#include <platform_utils/main.h>
#include <platform_utils/event_loop.h>
#include <platform_utils/log.h>

struct app {
	struct mgu_disp disp;
	GLuint program, attrib_pos, attrib_tex, uni_tex, uni_tran;

	GLuint tex;
	int tex_size[2];

	struct sr *sr;

	struct libtouch_surface *touch;
	struct libtouch_area *touch_area;
};

bool render(void *env, struct mgu_win_surf *surf, float t)
{
	struct app *app = env;

	struct mgu_out *out = mgu_disp_get_default_output(&app->disp);
	int32_t scale = out->scale;
	glViewport(0, 0, surf->size[0] * scale, surf->size[1] * scale);

	/* set blending */
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(179/255.0f, 182/255.0f, 183/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	float proj[9];
	mat3_ident(proj);
	mat3_proj(proj, (int[]){ surf->size[0], surf->size[1] });

	t /= 4.0f;
	int off = (t - (int)t) * 100;

	sr_put(app->sr, (struct sr_spec){
		.t = SR_RECT,
		.p = { off, 0, 300, surf->size[1] / 4 },
		.argb = 0xFF2ECC71
	});

	uint32_t corner_color[] = {
		0xFF3498DB,
		0xFF9B59B6,
		0xFFF1C40F,
		0xFF000000,
	};
	float rect_s = 100;
	for (int i = 0; i < 4; ++i) {
		float px = i % 2 == 0 ? 0 : surf->size[0] - rect_s;
		float py = i/2 % 2 == 0 ? 0 : surf->size[1] - rect_s;
		sr_put(app->sr, (struct sr_spec){
			.t = SR_RECT,
			.p = { px, py, rect_s, rect_s },
			.argb = corner_color[i]
		});
	}

	for (int i = 0; i < 2; ++i) {
		float line_w = 1;
		struct sr_spec spec = {
			.t = SR_RECT,
			.p = { 0, 0, line_w, line_w },
			.argb = 0xFFFFFFFF
		};
		spec.p[i] = surf->size[i] / 2.0f;
		spec.p[(i+1)%2+2] = surf->size[(i+1)%2];
		sr_put(app->sr, spec);
	}

	char str_buf[16];
	snprintf(str_buf, 16, "%d x %d", surf->size[0], surf->size[1]);
	sr_put(app->sr, (struct sr_spec){
		.t = SR_TEXT,
		.p = { 0, 0, surf->size[0], surf->size[1] },
		.argb = 0xFF00FFFF,
		.text = { .s = str_buf, .px = 100, .o = SR_CENTER }
	});

	snprintf(str_buf, 16, "pos_x = %dpx", off);
	sr_put(app->sr, (struct sr_spec){
		.t = SR_TEXT,
		.p = { off, 0, 300, surf->size[1] / 4 },
		.argb = 0xFFFF00FF,
		.text = { .s = str_buf, .px = 30, .o = SR_CENTER }
	});

	sr_present(app->sr, proj);

	glUseProgram(app->program);

	float T[9];
	mat3_ident(T);

	// double ppmm = app->disp.out.ppmm, ss = ppmm / scale;

	mat3_scale(T, (float[]){ app->tex_size[0], app->tex_size[1] });
	mat3_tran(T, (float[]){ surf->size[0] - app->tex_size[0],
		surf->size[1] - app->tex_size[1] });

	struct libtouch_rt rt = libtouch_area_get_transform(app->touch_area);
	// float rt_scale = libtouch_rt_scaling(&rt);
	// fprintf(stderr, "libtouch_rt: t[%f %f] s[%f] r[%f] scale: %f\n",
	// 	rt.t1, rt.t2, rt.s, rt.r, rt_scale);
	float touch_T[] = {
		rt.s, -rt.r, rt.t1,
		rt.r, rt.s, rt.t2,
		0, 0, 1
	};

	// mat3_scale(T, (float[]){ ss, ss });
	mat3_mul_l(T, touch_T);
	mat3_proj(T, (int[]){ surf->size[0], surf->size[1] });
	mat3_t(T);

	glUniformMatrix3fv(app->uni_tran, 1, GL_FALSE, T);

	static const GLfloat a_pos[] = { 0, 0, 0, 1, 1, 0, 1, 1 };
	GLuint buf;
	glGenBuffers(1, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, a_pos, GL_STATIC_DRAW);

	glVertexAttribPointer(app->attrib_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(app->attrib_tex, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(app->attrib_pos);
	glEnableVertexAttribArray(app->attrib_tex);

	/* bind texture */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glUniform1i(app->uni_tex, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDeleteBuffers(1, &buf);

	mgu_win_surf_mark_dirty(surf);
	return true;
}

const GLchar shader_frag_mandlebrot[] =
"precision highp float;\n"
"varying vec2 v_tex;\n"
"const int iter = 4;\n"
"void main() {\n"
"	vec2 z, c;\n"
"	z = c = (v_tex - 0.5) * 4.0;\n"
"	int k = 0;\n"
"	for (int i = 0; i < iter; ++i) {\n"
"		vec2 zn = vec2(z.x * z.x - z.y * z.y, z.x * z.y + z.y * z.x) + c;\n"
"		if (dot(zn, zn) > 4.0) break;\n"
"		z = zn;\n"
"		++k;\n"
"	}\n"
"	gl_FragColor = vec4(k == iter ? 0.0 : float(k) / float(iter), 0, 0, 1);\n"
"}\n";

const GLchar shader_frag_msdf[] =
"precision highp float;\n"
"varying vec2 v_tex;\n"
"uniform sampler2D tex;\n"
"uniform float scale;\n"
"float median(float a, float b, float c) {\n"
"	return max(min(a, b), min(max(a, b), c));\n"
"}\n"
"void main() {\n"
"	vec3 s = texture2D(tex, v_tex).rgb;\n"
"	float d = (median(s.r, s.g, s.b) - 0.5) * 2.0;\n"
"	float w = clamp(d * scale, -0.5, 0.5);\n"
"	float f = d > -0.999 ? w + 0.5 : 0.0;\n"
"	gl_FragColor = mix(vec4(1, 1, 1, 1), vec4(0, 0, 0, 1), f);\n"
"}\n";

void seat_cb(void *cl, struct mgu_win_surf *surf,
		struct mgu_input_event_args ev) {
	struct app *app = cl;
	if (ev.t & MGU_TOUCH) {
		const double *p = ev.touch.down_or_move.p;
		if (ev.t & MGU_DOWN) {
			libtouch_surface_down(app->touch, ev.time, ev.touch.id,
				(float[]){ p[0], p[1] });
		} else if (ev.t & MGU_MOVE) {
			libtouch_surface_motion(app->touch, ev.time, ev.touch.id,
				(float[]){ p[0], p[1] });
		} else if (ev.t & MGU_UP) {
			libtouch_surface_up(app->touch, ev.time, ev.touch.id);
		}
	}
	if (ev.t & MGU_KEYBOARD) {
		if (ev.t & MGU_DOWN) {
			printf("key down: %u\n", ev.keyboard.down.key);
		}
	}
}

void platform_main(struct platform *plat)
{
	pu_log_info("%s start\n", __func__);
	int res;

	struct app app;
	memset(&app, 0, sizeof(struct app));
	if (mgu_disp_init(&app.disp, plat) != 0) {
		res = 1;
		goto cleanup_none;
	}

	if (!mgu_disp_add_surf_default(&app.disp, "")) {
		res = 1;
		goto cleanup_disp;
	}
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && 0
	if (!mgu_disp_add_surf_layer_bottom_panel(&app.disp, 300)) {
		res = 1;
		goto cleanup_disp;
	}
#endif
	pu_log_info("%s added surfaces\n", __func__);

	app.disp.seat.cb = (struct mgu_seat_cb){ .env = &app, .f = seat_cb };
	app.disp.render_cb = (struct mgu_render_cb){ .env = &app, .f = render };

	app.touch = libtouch_surface_create();
	float area[] = { 0, 0, 10000, 10000 };
	app.touch_area = libtouch_surface_add_area(app.touch, area,
		(struct libtouch_area_opts){ .g = LIBTOUCH_TSR });

	app.program = mgu_shader_program(mgu_shader_vert_simple,
		mgu_shader_frag_tex);
	if (!app.program) {
		res = 1;
		goto cleanup_disp;
	}
	app.attrib_pos = glGetAttribLocation(app.program, "pos");
	app.attrib_tex = glGetAttribLocation(app.program, "tex");
	app.uni_tran = glGetUniformLocation(app.program, "mat");
	app.uni_tex = glGetUniformLocation(app.program, "texture");

	struct mgu_text *mgu_text = mgu_text_create(plat);
	app.tex = mgu_tex_text(mgu_text, (struct mgu_text_opts){
		.str = "line one\nline two\nline three",
		.s = { -1, -1 },
		.size_px = 30,
	}, app.tex_size);
	mgu_text_destroy(mgu_text);

	app.sr = sr_create_opengl(plat);

	struct event_loop *el = event_loop_create(plat);
	mgu_disp_add_to_event_loop(&app.disp, el);
	event_loop_run(el);
	event_loop_destroy(el);

	res = 0;

	sr_destroy(app.sr);
cleanup_disp:
	mgu_disp_finish(&app.disp);
cleanup_none:
	(void)res;
}
