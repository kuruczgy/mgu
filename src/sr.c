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
"attribute vec2 tex;\n"
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
"varying vec4 v_val;\n"
"uniform sampler2D tex;\n"
"void main() {\n"
"	gl_FragColor = texture2D(tex, v_val.xy);\n"
"}\n";

struct vertex {
	float pos[2];
	float d;
	float val[4];
	float tex[2];
};
_Static_assert(sizeof(struct vertex) == 9 * sizeof(float));
struct text {
	GLuint tex;
	float p[4];
	int tex_size[2];
};

static void argb_color(float col[static 4], uint32_t c) {
	for (int i = 0; i < 4; ++i) {
		col[i] = ((c >> (i * 8)) & 0xFF) / 255.0f;
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

		v.tex[0] = quad[i].x;
		v.tex[1] = quad[i].y;

		v.d = d;

		argb_color(v.val, argb);

		vec_append(vec, &v);
	}
}
static void set_vertices(GLuint prog, struct vec *vec) {
	GLuint a_pos = glGetAttribLocation(prog, "pos");
	GLuint a_val = glGetAttribLocation(prog, "val");
	GLuint a_tex = glGetAttribLocation(prog, "tex");

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex), vec->d + offsetof(struct vertex, pos));
	glVertexAttribPointer(a_val, 4, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex), vec->d + offsetof(struct vertex, val));
	glVertexAttribPointer(a_tex, 2, GL_FLOAT, GL_FALSE,
		sizeof(struct vertex), vec->d + offsetof(struct vertex, tex));

	glEnableVertexAttribArray(a_pos);
	glEnableVertexAttribArray(a_val);
	glEnableVertexAttribArray(a_tex);
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

	struct mgu_text text;
};
struct sr *sr_create_opengl() {
	struct sr *sr = malloc(sizeof(struct sr));
	// asrt(sr, "");

	sr->tris = vec_new_empty(sizeof(struct vertex));
	sr->texts = vec_new_empty(sizeof(struct text));

	sr->prog_color = mgu_shader_program(shader_vert, shader_frag_color);
	sr->prog_tex = mgu_shader_program(shader_vert, shader_frag_tex);

	mgu_text_init(&sr->text);

	return sr;
}
void sr_destroy(struct sr *sr) {
	vec_free(&sr->tris);
	vec_free(&sr->texts);

	glDeleteProgram(sr->prog_color);
	glDeleteProgram(sr->prog_tex);

	mgu_text_finish(&sr->text);

	free(sr);
}

static void sr_put_text(struct sr *sr, struct sr_spec spec) {
	struct text t;
	memcpy(t.p, spec.p, sizeof(float) * 4);
	t.tex = mgu_tex_text(&sr->text, spec.text.s, t.tex_size);
	vec_append(&sr->texts, &t);
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
void sr_present(struct sr *sr, const float mat[static 9]) {

	/* adjust depth values for all tris */
	for (int i = 0; i < sr->tris.len; ++i) {
		struct vertex *v = vec_get(&sr->tris, i);
		v->d /= -sr->tris.len;
	}

	glUseProgram(sr->prog_color);
	set_mat(sr->prog_color, mat);
	set_vertices(sr->prog_color, &sr->tris);
	glDrawArrays(GL_TRIANGLES, 0, sr->tris.len);

	/* text stuff */
	static const GLfloat quad_pos[] = {
		0, 0, 0,
		0, 1, 0,
		1, 0, 0,
		1, 1, 0,
	};
	static const GLfloat quad_tex[] = { 0, 0, 0, 1, 1, 0, 1, 1 };
	glUseProgram(sr->prog_tex);
	GLuint a_pos = glGetAttribLocation(sr->prog_tex, "pos");
	GLuint a_val = glGetAttribLocation(sr->prog_tex, "val");
	GLuint u_tex = glGetUniformLocation(sr->prog_tex, "tex");
	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 0, quad_pos);
	glVertexAttribPointer(a_val, 2, GL_FLOAT, GL_FALSE, 0, quad_tex);
	glEnableVertexAttribArray(a_pos);
	glEnableVertexAttribArray(a_val);

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

		glBindTexture(GL_TEXTURE_2D, t->tex);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDeleteTextures(1, &t->tex);
	}

	vec_clear(&sr->tris);
	vec_clear(&sr->texts);
}
