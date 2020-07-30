#include <math.h>
#include <string.h>
#include "linalg.h"

/* Ideas taken from the wlroots matrix utilities
 * (wlroots/types/wlr_matrix.c). */

void mat3_ident(float m[static 9])
{
	static const float ident[9] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1,
	};
	memcpy(m, ident, sizeof(ident));
}

void mat3_mul_l(float A[static 9], const float B[static 9])
{
	float m[9] = { 0 };

	/* please, compiler gods, do some unrolling here. */
	static const size_t n = 3;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < n; ++j) {
			for (size_t k = 0; k < n; ++k) {
				m[n * i + j] += B[n * i + k] * A[n * k + j];
			}
		}
	}

	memcpy(A, m, sizeof(m));
}

void mat3_t(float m[static 9])
{
	const float t[9] = {
		m[0], m[3], m[6],
		m[1], m[4], m[7],
		m[2], m[5], m[8],
	};
	memcpy(m, t, sizeof(t));
}

void mat3_tran(float m[static 9], const float t[static 2])
{
	const float r[9] = {
		1, 0, t[0],
		0, 1, t[1],
		0, 0, 1,
	};
	mat3_mul_l(m, r);
}

void mat3_scale(float m[static 9], const float s[static 2])
{
	const float r[9] = {
		s[0], 0, 0,
		0, s[1], 0,
		0, 0, 1,
	};
	mat3_mul_l(m, r);
}

void mat3_rot(float m[static 9], float a)
{
	const float r[9] = {
		cos(a), -sin(a), 0,
		sin(a), cos(a), 0,
		0, 0, 1,
	};
	mat3_mul_l(m, r);
}

void mat3_proj(float m[static 9], const int size[static 2])
{
	const float s[2] = { 2.0f / size[0], 2.0f / size[1] };
	mat3_scale(m, s);

	const float t[2] = { -1, -1 };
	mat3_tran(m, t);

	const float k[2] = { 1, -1 };
	mat3_scale(m, k);

}
