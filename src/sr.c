#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ds/vec.h>
#include <ds/matrix.h>
#include <mgu/gl.h>
#include <mgu/text.h>
#include <mgu/sr.h>

const GLchar shader_vert[] =
"uniform mat3 mat;\n"
"attribute vec3 pos;\n"
"attribute vec4 val;\n"
"varying vec4 v_val;\n"
"void main() {\n"
"	vec3 ph = mat * vec3(pos.xy, 1);\n"
"	gl_Position = vec4(ph.xy / ph.z, pos.z, 1);\n"
"	v_val = val;\n"
"}\n";

const GLchar shader_frag_color[] =
"precision mediump float;\n"
"varying vec4 v_val;\n"
"void main() { gl_FragColor = v_val; }\n";

const GLchar shader_frag_tex[] =
"precision mediump float;\n"
"uniform vec4 tex_color;\n"
"varying vec4 v_val;\n"
"uniform sampler2D texture;\n"
"void main() {\n"
"	gl_FragColor = tex_color * texture2D(texture, v_val.xy);\n"
"}\n";

struct vertex {
	float pos[2];
	float d;
	float val[4];
};
_Static_assert(sizeof(struct vertex) == 7 * sizeof(float), "");
struct text {
	GLuint tex;
	float p[4];
	int tex_size[2];
	float color[4];
};

static void argb_color(float col[static 4], uint32_t c) {
	for (int i = 0; i < 4; ++i) {
		col[(i + 3) % 4] = ((c >> ((3 - i) * 8)) & 0xFF) / 255.0f;
	}
}

/* vec<struct vertex> */
static void make_quad(struct vec *vec, float p[static 4], uint32_t argb) {
	struct { int x, y; } quad[6] = {
		{ 0, 0 }, { 1, 0 }, { 0, 1 },
		{ 1, 1 }, { 0, 1 }, { 1, 0 },
	};
	int d = vec->len;
	for (int i = 0; i < sizeof(quad) / sizeof(quad[0]); ++i) {
		struct vertex v;

		v.pos[0] = p[0] + quad[i].x * p[2];
		v.pos[1] = p[1] + quad[i].y * p[3];

		v.d = d;

		argb_color(v.val, argb);

		vec_append(vec, &v);
	}
}
static void set_vertices(GLuint prog, GLuint buffer, const struct vertex *d, int n) {
	GLint a_pos = glGetAttribLocation(prog, "pos");
	GLint a_val = glGetAttribLocation(prog, "val");

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(struct vertex) * n, d, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex), (void *)offsetof(struct vertex, pos));
	glEnableVertexAttribArray(a_pos);
	if (a_val != -1) {
		glVertexAttribPointer(a_val, 4, GL_FLOAT, GL_FALSE,
			sizeof(struct vertex),
			(void *)offsetof(struct vertex, val));
		glEnableVertexAttribArray(a_val);
	}
}
static void set_mat(GLuint prog, const float mat[static 9]) {
	GLuint u_mat = glGetUniformLocation(prog, "mat");
	float m[9];
	memcpy(m, mat, sizeof(m));
	mat3_t(m);
	glUniformMatrix3fv(u_mat, 1, GL_FALSE, m);
}

struct sr {
	struct vec tris; /* vec<struct vertex> */
	struct vec texts; /* vec<struct text> */

	GLuint prog_color, prog_tex;
	GLuint vertex_buffer;

	struct mgu_text text;
};
struct sr *sr_create_opengl(struct platform *plat) {
	struct sr *sr = malloc(sizeof(struct sr));
	// asrt(sr, "");

	sr->tris = vec_new_empty(sizeof(struct vertex));
	sr->texts = vec_new_empty(sizeof(struct text));

	sr->prog_color = mgu_shader_program(shader_vert, shader_frag_color);
	sr->prog_tex = mgu_shader_program(shader_vert, shader_frag_tex);

	glGenBuffers(1, &sr->vertex_buffer);

	mgu_text_init(&sr->text, plat);

	return sr;
}
void sr_destroy(struct sr *sr) {
	vec_free(&sr->tris);
	vec_free(&sr->texts);

	glDeleteBuffers(1, &sr->vertex_buffer);

	glDeleteProgram(sr->prog_color);
	glDeleteProgram(sr->prog_tex);

	mgu_text_finish(&sr->text);

	free(sr);
}

static void sr_put_text(struct sr *sr, struct sr_spec spec) {
	struct text t;
	memcpy(t.p, spec.p, sizeof(float) * 4);
	struct mgu_text_opts opts = {
		.str = spec.text.s,
		.s = { (int)spec.p[2], (int)spec.p[3] },
		.ch = spec.text.o & SR_CENTER_H,
		.cv = spec.text.o & SR_CENTER_V,
		.size_px = spec.text.px,
	};
	t.tex = mgu_tex_text(&sr->text, opts, t.tex_size);
	argb_color(t.color, spec.argb);
	vec_append(&sr->texts, &t);
}
void sr_measure_text(struct sr *sr, float p[static 2], struct sr_spec spec) {
	struct mgu_text_opts opts = {
		.str = spec.text.s,
		.s = { (int)spec.p[2], (int)spec.p[3] },
		.ch = spec.text.o & SR_CENTER_H,
		.cv = spec.text.o & SR_CENTER_V,
		.size_px = spec.text.px,
	};
	int s[2];
	mgu_text_measure(&sr->text, opts, s);
	p[0] = s[0], p[1] = s[1];
}
void sr_put(struct sr *sr, struct sr_spec spec) {
	switch (spec.t) {
	case SR_RECT:
		make_quad(&sr->tris, spec.p, spec.argb);
		break;
	case SR_TEXT:
		sr_put_text(sr, spec);
		break;
	default:
		;// asrt(false, "");
	}
}
void sr_measure(struct sr *sr, float p[static 2], struct sr_spec spec) {
	switch (spec.t) {
	case SR_RECT:
		memcpy(p, spec.p + 2, sizeof(float) * 2);
		break;
	case SR_TEXT:
		sr_measure_text(sr, p, spec);
		break;
	default:
		;// asrt(false, "");
	}
}
void sr_present(struct sr *sr, const float mat[static 9]) {

	/* adjust depth values for all tris */
	for (int i = 0; i < sr->tris.len; ++i) {
		struct vertex *v = vec_get(&sr->tris, i);
		v->d /= -sr->tris.len;
	}

	glUseProgram(sr->prog_color);
	set_mat(sr->prog_color, mat);
	set_vertices(sr->prog_color, sr->vertex_buffer, sr->tris.d, sr->tris.len);
	glDrawArrays(GL_TRIANGLES, 0, sr->tris.len);

	/* text stuff */
	static const struct vertex quad[] = {
		{ .pos = { 0, 0 }, .d = 0, .val = { 0, 0 } },
		{ .pos = { 0, 1 }, .d = 0, .val = { 0, 1 } },
		{ .pos = { 1, 0 }, .d = 0, .val = { 1, 0 } },
		{ .pos = { 1, 1 }, .d = 0, .val = { 1, 1 } },
	};
	glUseProgram(sr->prog_tex);
	set_vertices(sr->prog_tex, sr->vertex_buffer, quad, 4);
	GLuint u_tex = glGetUniformLocation(sr->prog_tex, "texture");
	GLuint u_tex_color = glGetUniformLocation(sr->prog_tex, "tex_color");

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(u_tex, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	float M[9];
	for (int i = 0; i < sr->texts.len; ++i) {
		struct text *t = vec_get(&sr->texts, i);

		mat3_ident(M);
		mat3_scale(M, (float[]){ t->tex_size[0], t->tex_size[1] });
		mat3_tran(M, (float[]){ t->p[0], t->p[1] });
		mat3_mul_l(M, mat);
		set_mat(sr->prog_tex, M);

		glUniform4f(u_tex_color,
			t->color[0], t->color[1], t->color[2], t->color[3]);

		glBindTexture(GL_TEXTURE_2D, t->tex);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDeleteTextures(1, &t->tex);
	}

	vec_clear(&sr->tris);
	vec_clear(&sr->texts);
}
