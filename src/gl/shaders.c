#include <mgu/gl.h>

static void init_common(struct mgu_shader *s) {
	s->a_pos = glGetAttribLocation(s->prog, "pos");
	s->a_tex = glGetAttribLocation(s->prog, "tex");
	s->u_mat = glGetUniformLocation(s->prog, "mat");
}

void mgu_shader_init_color(struct mgu_shader *s) {
	s->prog = mgu_shader_program(mgu_shader_vert_simple,
		mgu_shader_frag_color);
	s->color.u_color = glGetUniformLocation(s->prog, "color");
	init_common(s);
}


const GLchar mgu_shader_frag_color[] =
"precision mediump float;\n"
"uniform vec4 color;\n"
"void main() { gl_FragColor = color; }\n";

const GLchar mgu_shader_vert_simple[] =
"uniform mat3 mat;\n"
"attribute vec2 pos;\n"
"attribute vec2 tex;\n"
"varying vec2 v_tex;\n"
"void main() {\n"
"	vec3 ph = mat * vec3(pos, 1.0);\n"
"	gl_Position = vec4(ph.xy / ph.z, 0, 1);\n"
"	v_tex = tex;\n"
"}\n";

const GLchar mgu_shader_frag_tex[] =
"precision mediump float;\n"
"varying vec2 v_tex;\n"
"uniform sampler2D tex;\n"
"void main() {\n"
"	gl_FragColor = texture2D(tex, v_tex);\n"
"}\n";
