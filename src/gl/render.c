#include <stdio.h>
#include <stdlib.h>
#include <mgu/gl.h>

static GLuint compile(GLuint type, const GLchar *src)
{
	GLint get;
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &get);
	if (get != GL_TRUE) {
		glGetShaderiv(s, GL_INFO_LOG_LENGTH, &get);
		GLchar *log = malloc(get + 1);
		glGetShaderInfoLog(s, get + 1, NULL, log);
		fprintf(stderr, "src: %s", src);
		fprintf(stderr, "shader compile failed:\n%s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

GLuint mgu_shader_program(const GLchar *vert, const GLchar *frag)
{
	GLint get;
	GLuint v = compile(GL_VERTEX_SHADER, vert);
	GLuint f = compile(GL_FRAGMENT_SHADER, frag);
	GLuint p = glCreateProgram();

	if (!(v && f && p)) {
		goto cleanup;
	}

	glAttachShader(p, v), glAttachShader(p, f);
	glLinkProgram(p);
	glDetachShader(p, v), glDetachShader(p, f);

	glGetProgramiv(p, GL_LINK_STATUS, &get);
	if (get != GL_TRUE) {
		glGetProgramiv(p, GL_INFO_LOG_LENGTH, &get);
		GLchar *log = malloc(get + 1);
		glGetProgramInfoLog(p, get + 1, NULL, log);
		fprintf(stderr, "program link failed:\n%s\n", log);
		goto cleanup;
	}

	goto finish;
cleanup:
	glDeleteProgram(p), p = 0;
finish:
	glDeleteShader(v), glDeleteShader(f);
	return p;
}
