#include "render.h"

const GLchar mgu_shader_vert_simple[] =
"uniform mat3 tran;\n"
"attribute vec2 pos;\n"
"void main() {\n"
"	vec3 ph = tran * vec3(pos, 1.0);\n"
"	gl_Position = vec4(ph.xy / ph.z, 0, 1);\n"
"}\n";

const GLchar mgu_shader_frag_color[] =
"precision mediump float;\n"
"uniform vec4 color;\n"
"void main() { gl_FragColor = color; }\n";

