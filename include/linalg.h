#ifndef MGU_LINALG_H
#define MGU_LINALG_H

void mat3_ident(float m[static 9]);
/* A <- B * A */
void mat3_mul_l(float A[static 9], const float B[static 9]);
void mat3_t(float m[static 9]);
void mat3_tran(float m[static 9], const float t[static 2]);
void mat3_scale(float m[static 9], const float s[static 2]);
void mat3_rot(float m[static 9], float a);

/* Project a left handed coordinate system to NDC. */ 
void mat3_proj(float m[static 9], const int size[static 2]);

#endif
