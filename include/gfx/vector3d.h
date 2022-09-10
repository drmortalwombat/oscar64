#ifndef VECTOR3D_H
#define VECTOR3D_H

struct Vector2
{
	float	v[2];
};

inline void vec2_set(Vector2 * vd, float x, float y);

void vec2_sum(Vector2 * vd, const Vector2 * v1, const Vector2 * v2);

void vec2_diff(Vector2 * vd, const Vector2 * v1, const Vector2 * v2);

void vec2_add(Vector2 * vd, const Vector2 * vs);

void vec2_sub(Vector2 * vd, const Vector2 * vs);

void vec2_madd(Vector2 * vd, float s, const Vector2 * vs);

void vec2_scale(Vector2 * vd, float s);

void vec2_lincomb(Vector2 * vd, const Vector2 * vb, float x, const Vector2 * vx);

void vec2_lincomb2(Vector2 * vd, const Vector2 * vb, float x, const Vector2 * vx, float y, const Vector2 * vy);

void vec2_lerp(Vector2 * vd, const Vector2 * v1, const Vector2 * v2, float t);

bool vec2_is_zero(const Vector2 * v);

float vec2_vmul(const Vector2 * v1, const Vector2 * v2);

void vec2_norm(Vector2 * v);

float vec2_length(const Vector2 * v);

float vec2_qlength(const Vector2 * v);

float vec2_distance(const Vector2 * v1, const Vector2 * v2);

float vec2_qdistance(const Vector2 * v1, const Vector2 * v2);


struct Matrix2
{
	float	m[4];
};

void mat2_ident(Matrix2 * md);

void mat2_add(Matrix2 * md, const Matrix2 * ms);

void mat2_scale(Matrix2 * md, float s);

void mat2_mmul(Matrix2 * md, const Matrix2 * ms);

void mat2_rmmul(Matrix2 * md, const Matrix2 * ms);

void mat2_tranpose(Matrix2 * md);

void vec2_mmul(Vector2 * vd, const Matrix2 * m, const Vector2 * vs);

void vec2_mimul(Vector2 * vd, const Matrix2 * m, const Vector2 * vs);

void mat2_set_rotate(Matrix2 * md, float a);

void mat2_rotate(Matrix2 * md, float a);

float mat2_det(const Matrix2 * ms);

void mat2_invert(Matrix2 * md, const Matrix2 * ms);


struct Vector3
{
	float	v[3];
};

inline void vec3_set(Vector3 * vd, float x, float y, float z);

void vec3_sum(Vector3 * vd, const Vector3 * v1, const Vector3 * v2);

void vec3_diff(Vector3 * vd, const Vector3 * v1, const Vector3 * v2);

void vec3_add(Vector3 * vd, const Vector3 * vs);

void vec3_sub(Vector3 * vd, const Vector3 * vs);

void vec3_madd(Vector3 * vd, float s, const Vector3 * vs);

void vec3_scale(Vector3 * vd, float s);

void vec3_lincomb(Vector3 * vd, const Vector3 * vb, float x, const Vector3 * vx);

void vec3_lincomb2(Vector3 * vd, const Vector3 * vb, float x, const Vector3 * vx, float y, const Vector3 * vy);

void vec3_lerp(Vector3 * vd, const Vector3 * v1, const Vector3 * v2, float t);

bool vec3_is_zero(const Vector3 * v);

float vec3_vmul(const Vector3 * v1, const Vector3 * v2);

void vec3_norm(Vector3 * v);

float vec3_length(const Vector3 * v);

float vec3_qlength(const Vector3 * v);

float vec3_distance(const Vector3 * v1, const Vector3 * v2);

float vec3_qdistance(const Vector3 * v1, const Vector3 * v2);

void vec3_mcadd(Vector3 * vd, const Vector3 * v1, const Vector3 * v2);

void vec3_mscadd(Vector3 * vd, float s, const Vector3 * v1, const Vector3 * v2);

void vec3_cmul(Vector3 * vd, const Vector3 * v1, const Vector3 * v2);

void vec3_xmul(Vector3 * vd, const Vector3 * v1, const Vector3 * v2);

void vec3_mbase(Vector3 * v1, Vector3 * v2, Vector3 * v3);

void vec3_bend(Vector3 * vd, const Vector3 * vs, float chi1, float chi2);



struct Matrix3
{
	float	m[9];
};

void mat3_ident(Matrix3 * m);

void mat3_scale(Matrix3 * md, float s);

void mat3_add(Matrix3 * md, const Matrix3 * ms);

void mat3_mmul(Matrix3 * md, const Matrix3 * ms);

void mat3_rmmul(Matrix3 * md, const Matrix3 * ms);

void mat3_transpose(Matrix3 * md, const Matrix3 * ms);

float mat3_det(const Matrix3 * ms);

void mat3_invert(Matrix3 * md, const Matrix3 * ms);

void vec3_mmul(Vector3 * vd, const Matrix3 * m, const Vector3 * vs);

void mat3_set_rotate_x(Matrix3 * m, float a);

void mat3_set_rotate_y(Matrix3 * m, float a);

void mat3_set_rotate_z(Matrix3 * m, float a);

void mat3_set_rotate(Matrix3 * m, const Vector3 * v, float a);



struct Vector4
{
	float	v[4];
};

inline void vec4_set(Vector4 * vd, float x, float y, float z, float w);

void vec4_sum(Vector4 * vd, const Vector4 * v1, const Vector4 * v2);

void vec4_diff(Vector4 * vd, const Vector4 * v1, const Vector4 * v2);

void vec4_add(Vector4 * vd, const Vector4 * vs);

void vec4_sub(Vector4 * vd, const Vector4 * vs);

void vec4_madd(Vector4 * vd, float s, const Vector4 * vs);

void vec4_scale(Vector4 * vd, float s);

void vec4_lincomb(Vector4 * vd, const Vector4 * vb, float x, const Vector4 * vx);

void vec4_lincomb2(Vector4 * vd, const Vector4 * vb, float x, const Vector4 * vx, float y, const Vector4 * vy);

void vec4_lerp(Vector4 * vd, const Vector4 * v1, const Vector4 * v2, float t);

bool vec4_is_zero(const Vector4 * v);

float vec4_vmul(const Vector4 * v1, const Vector4 * v2);

void vec4_norm(Vector4 * v);

float vec4_length(const Vector4 * v);

float vec4_qlength(const Vector4 * v);

float vec4_distance(const Vector4 * v1, const Vector4 * v2);

float vec4_qdistance(const Vector4 * v1, const Vector4 * v2);

void vec4_mcadd(Vector4 * vd, const Vector4 * v1, const Vector4 * v2);

void vec4_mscadd(Vector4 * vd, float s, const Vector4 * v1, const Vector4 * v2);

void vec4_cmul(Vector4 * vd, const Vector4 * v1, const Vector4 * v2);

void vec4_xmul(Vector4 * vd, const Vector4 * v1, const Vector4 * v2);

void vec4_mbase(Vector4 * v1, Vector4 * v2, Vector4 * v3);

void vec4_bend(Vector4 * vd, const Vector4 * vs, float chi1, float chi2);


struct Matrix4
{
	float	m[16];
};

void mat4_ident(Matrix4 * m);

void mat4_from_vec4(Matrix4 * m, const Vector4 * vx, const Vector4 * vy, const Vector4 * vz, const Vector4 * vw);

void mat4_from_vec3(Matrix4 * m, const Vector3 * vx, const Vector3 * vy, const Vector3 * vz, const Vector3 * vw);

void mat4_make_perspective(Matrix4 * m, float fieldOfViewInRadians, float aspect, float near, float far);

void mat4_make_view(Matrix4 * m, const Vector3 * pos, const Vector3 * target, const Vector3 * up);

void mat4_scale(Matrix4 * md, float s);

void mat4_add(Matrix4 * md, const Matrix4 * ms);

void mat4_mmul(Matrix4 * md, const Matrix4 * ms);

void mat4_rmmul(Matrix4 * md, const Matrix4 * ms);

float mat4_det(const Matrix4 * m);

void mat4_invert(Matrix4 * md, const Matrix4 * ms);

void vec4_mmul(Vector4 * vd, const Matrix4 * m, const Vector4 * vs);

void vec3_mmulp(Vector3 * vd, const Matrix4 * m, const Vector3 * vs);

void vec3_mmuld(Vector3 * vd, const Matrix4 * m, const Vector3 * vs);


void mat4_set_rotate_x(Matrix4 * m, float a);

void mat4_set_rotate_y(Matrix4 * m, float a);

void mat4_set_rotate_z(Matrix4 * m, float a);

void mat4_set_rotate(Matrix4 * m, const Vector3 * v, float a);

void mat4_set_translate(Matrix4 * m, const Vector3 * v);

void mat4_set_scale(Matrix4 * m, float s);


void vec3_project(Vector3 * vd, const Matrix4 * m, const Vector3 * vs);

// And now for some fixpoint math in 4.12

struct F12Vector3
{
	int v[3];
};

struct F12Matrix3
{
	int	m[9];
};

static const int FIX12_ONE = 1 << 12;

void f12mat3_ident(F12Matrix3 * m);

void f12mat3_mmul(F12Matrix3 * md, const F12Matrix3 * ms);

void f12mat3_rmmul(F12Matrix3 * md, const F12Matrix3 * ms);

void f12mat3_set_rotate_x(F12Matrix3 * m, float a);

void f12mat3_set_rotate_y(F12Matrix3 * m, float a);


void f12mat3_set_rotate_z(F12Matrix3 * m, float a);

void f12vec3_mmul(F12Vector3 * vd, const F12Matrix3 * m, const F12Vector3 * vs);


#pragma compile("vector3d.c")

#endif

