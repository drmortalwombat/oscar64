#include "vector3d.h"
#include <math.h>

void vec2_sum(Vector2 * vd, const Vector2 * v1, const Vector2 * v2)
{
	vd->v[0] = v1->v[0] + v2->v[0];
	vd->v[1] = v1->v[1] + v2->v[1];
}

void vec2_diff(Vector2 * vd, const Vector2 * v1, const Vector2 * v2)
{
	vd->v[0] = v1->v[0] - v2->v[0];
	vd->v[1] = v1->v[1] - v2->v[1];	
}

void vec2_add(Vector2 * vd, const Vector2 * vs)
{
	vd->v[0] += vs->v[0];
	vd->v[1] += vs->v[1];
}

void vec2_madd(Vector2 * vd, float s, const Vector2 * vs)
{
	vd->v[0] += s * vs->v[0];
	vd->v[1] += s * vs->v[1];	
}

void vec2_sub(Vector2 * vd, const Vector2 * vs)
{
	vd->v[0] -= vs->v[0];
	vd->v[1] -= vs->v[1];
}

void vec2_scale(Vector2 * vd, float s)
{
	vd->v[0] *= s;
	vd->v[1] *= s;
}

void vec2_lincomb(Vector2 * vd, const Vector2 * vb, float x, const Vector2 * vx)
{
	vd->v[0] = vb->v[0] + x * vx->v[0];
	vd->v[1] = vb->v[1] + x * vx->v[1];
}

void vec2_lincomb2(Vector2 * vd, const Vector2 * vb, float x, const Vector2 * vx, float y, const Vector2 * vy)
{
	vd->v[0] = vb->v[0] + x * vx->v[0] + y * vy->v[0];
	vd->v[1] = vb->v[1] + x * vx->v[1] + y * vy->v[1];
}

void vec2_lerp(Vector2 * vd, const Vector2 * v1, const Vector2 * v2, float t)
{
	vd->v[0] = (1 - t) * v1->v[0] + t * v2->v[0];
	vd->v[1] = (1 - t) * v1->v[1] + t * v2->v[1];	
}

bool vec2_is_zero(const Vector2 * v)
{
	return v->v[0] == 0 && v->v[1] == 0;
}

float vec2_vmul(const Vector2 * v1, const Vector2 * v2)
{
	return v1->v[0] * v2->v[0] + v1->v[1] * v2->v[1];
}

void vec2_norm(Vector2 * v)
{
	vec2_scale(v, 1.0 / vec2_length(v));
}

float vec2_length(const Vector2 * v)
{
	return sqrt(vec2_qlength(v));
}

float vec2_qlength(const Vector2 * v)
{
	return vec2_vmul(v, v);
}

float vec2_distance(const Vector2 * v1, const Vector2 * v2)
{
	return sqrt(vec2_qdistance(v1, v2));
}

float vec2_qdistance(const Vector2 * v1, const Vector2 * v2)
{
	float	dx = v1->v[0] - v2->v[0], dy = v1->v[1] - v2->v[1];
	return dx * dx + dy * dy;
}




void mat2_ident(Matrix2 * md)
{
	md[0] = 1.0;
	md[1] = 0.0;
	md[2] = 0.0;
	md[3] = 1.0;
}

void mat2_add(Matrix2 * md, const Matrix2 * ms)
{
	for(char i=0; i<4; i++)
		md->m[i] += ms->m[i];
}

void mat2_scale(Matrix2 * md, float s)
{
	for(char i=0; i<4; i++)
		md->m[i] *= s;
}

void mat2_mmul(Matrix2 * md, const Matrix2 * ms)
{
	float	m0 = md->m[0] * ms->m[0] + md->m[2] * ms->m[1];
	float	m1 = md->m[1] * ms->m[0] + md->m[3] * ms->m[1];

	float	m2 = md->m[0] * ms->m[2] + md->m[2] * ms->m[3];
	float	m3 = md->m[1] * ms->m[2] + md->m[3] * ms->m[3];
	
	md->m[0] = m0; md->m[1] = m1;
	md->m[2] = m2; md->m[3] = m3;
}

void mat2_rmmul(Matrix2 * md, const Matrix2 * ms)
{
	float	m0 = ms->m[0] * md->m[0] + ms->m[2] * md->m[1];
	float	m1 = ms->m[1] * md->m[0] + ms->m[3] * md->m[1];

	float	m2 = ms->m[0] * md->m[2] + ms->m[2] * md->m[3];
	float	m3 = ms->m[1] * md->m[2] + ms->m[3] * md->m[3];
	
	md->m[0] = m0; md->m[1] = m1;
	md->m[2] = m2; md->m[3] = m3;
}

void mat2_tranpose(Matrix2 * md)
{
	float	t = md->m[1]; md->m[1] = md->m[2]; md->m[2] = t;
}

void vec2_mmul(Vector2 * vd, const Matrix2 * m, const Vector2 * vs)
{
	float	x = m->m[0] * vs->v[0] + m->m[2] * vs->v[1];
	float	y = m->m[1] * vs->v[0] + m->m[3] * vs->v[1];

	vd->v[0] = x;
	vd->v[1] = y;
}

void vec2_mimul(Vector2 * vd, const Matrix2 * m, const Vector2 * vs)
{
	float	det = 1 / mat2_det(m);

	float	x = det * ( m->m[3] * vs->v[0] - m->m[2] * vs->v[1]);
	float	y = det * (-m->m[1] * vs->v[0] + m->m[0] * vs->v[1]);

	vd->v[0] = x;
	vd->v[1] = y;
}

void mat2_set_rotate(Matrix2 * md, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	md->m[0] = c; md->m[2] = s;
	md->m[1] =-s; md->m[3] = c;

}

void mat2_rotate(Matrix2 * md, float a)
{
	Matrix2	mr;
	mat2_set_rotate(mr, a);
	mat2_rmmul(md, mr);
}

float mat2_det(const Matrix2 * ms) 
{
	return ms->m[0] * ms->m[3] - ms->m[1] * ms->m[2];
}

void mat2_invert(Matrix2 * md, const Matrix2 * ms)
{
	float	det = 1 / mat2_det(ms);

	float	m0 =  det * ms->m[3];
	float	m1 = -det * ms->m[1];
	float	m2 = -det * ms->m[2];
	float 	m3 =  det * ms->m[0];

	md->m[0] = m0; md->m[1] = m1; 
	md->m[2] = m2; md->m[3] = m3;
}


inline void vec3_set(Vector3 * vd, float x, float y, float z)
{
	vd->v[0] = x;
	vd->v[1] = y;
	vd->v[2] = z;
}

void vec3_sum(Vector3 * vd, const Vector3 * v1, const Vector3 * v2)
{
	for(char i=0; i<3; i++)
		vd->v[i] = v1->v[i] + v2->v[i];
}

void vec3_diff(Vector3 * vd, const Vector3 * v1, const Vector3 * v2)
{
	for(char i=0; i<3; i++)
		vd->v[i] = v1->v[i] - v2->v[i];
}

void vec3_add(Vector3 * vd, const Vector3 * vs)
{
	for(char i=0; i<3; i++)
		vd->v[i] += vs->v[i];
}

void vec3_madd(Vector3 * vd, float s, const Vector3 * vs)
{
	for(char i=0; i<3; i++)
		vd->v[i] += s * vs->v[i];
}

void vec3_sub(Vector3 * vd, const Vector3 * vs)
{
	for(char i=0; i<3; i++)
		vd->v[i] -= vs->v[i];
}

void vec3_scale(Vector3 * vd, float s)
{
	for(char i=0; i<3; i++)
		vd->v[i] *= s;
}

void vec3_lincomb(Vector3 * vd, const Vector3 * vb, float x, const Vector3 * vx)
{
	for(char i=0; i<3; i++)
		vd->v[i] = vb->v[i] + x * vx->v[i];
}

void vec3_lincomb2(Vector3 * vd, const Vector3 * vb, float x, const Vector3 * vx, float y, const Vector3 * vy)
{
	for(char i=0; i<3; i++)
		vd->v[i] = vb->v[i] + x * vx->v[i] + y * vy->v[i];
}

void vec3_lerp(Vector3 * vd, const Vector3 * v1, const Vector3 * v2, float t)
{
	for(char i=0; i<3; i++)
		vd->v[i] = (1 - t) * v1->v[i] + t * v2->v[i];
}

bool vec3_is_zero(const Vector3 * v)
{
	return v->v[0] == 0 && v->v[1] == 0 && v->v[2] == 0;
}

float vec3_vmul(const Vector3 * v1, const Vector3 * v2)
{
	return v1->v[0] * v2->v[0] + v1->v[1] * v2->v[1] + v1->v[2] * v2->v[2];
}

void vec3_norm(Vector3 * v)
{
	vec3_scale(v, 1.0 / vec3_length(v));
}

float vec3_length(const Vector3 * v)
{
	return sqrt(vec3_qlength(v));
}

float vec3_qlength(const Vector3 * v)
{
	return vec3_vmul(v, v);
}

float vec3_distance(const Vector3 * v1, const Vector3 * v2)
{
	return sqrt(vec3_qdistance(v1, v2));
}

float vec3_qdistance(const Vector3 * v1, const Vector3 * v2)
{
	float	dx = v1->v[0] - v2->v[0], dy = v1->v[1] - v2->v[1], dz = v1->v[2] - v2->v[2];
	return dx * dx + dy * dy + dz * dz;
}

void vec3_mcadd(Vector3 * vd, const Vector3 * v1, const Vector3 * v2)
{
	for(char i=0; i<3; i++)
		vd->v[i] += v1->v[i] * v2->v[i];
}

void vec3_mscadd(Vector3 * vd, float s, const Vector3 * v1, const Vector3 * v2)
{
	for(char i=0; i<3; i++)
		vd->v[i] += s * v1->v[i] * v2->v[i];
}

void vec3_cmul(Vector3 * vd, const Vector3 * v1, const Vector3 * v2)
{
	for(char i=0; i<3; i++)
		vd->v[i] = v1->v[i] * v2->v[i];	
}

void vec3_xmul(Vector3 * vd, const Vector3 * v1, const Vector3 * v2)
{
	float tx = v1->v[1] * v2->v[2] - v1->v[2] * v2->v[1];
	float ty = v1->v[2] * v2->v[0] - v1->v[0] * v2->v[2];
	float tz = v1->v[0] * v2->v[1] - v1->v[1] * v2->v[0];

	vd->v[0] = tx;
	vd->v[1] = ty;
	vd->v[2] = tz;
}

void vec3_mbase(Vector3 * v1, Vector3 * v2, Vector3 * v3)
{
	if (fabs(v1->v[2]) >= fabs(v1->v[0]) && fabs(v1->v[2]) >= fabs(v1->v[1])) 
	{
		v2->v[0] =   v1->v[2];
		v2->v[1] =   0;
		v2->v[2] = - v1->v[0];
	} 
	else if (fabs(v1->v[1]) >= fabs(v1->v[0]) && fabs(v1->v[1]) >= fabs(v1->v[2])) 
	{
		v2->v[0] =   v1->v[1];
		v2->v[1] =  -v1->v[0];
		v2->v[2] =   0;
	} 
	else 
	{
		v2->v[0] =  -v1->v[1];
		v2->v[1] =   v1->v[0];
		v2->v[2] =   0;
	}
	vec3_norm(v2);
	vec3_xmul(v3, v1, v2);
}

void vec3_bend(Vector3 * vd, const Vector3 * vs, float chi1, float chi2)
{
	float m0 = sqrt(1 - chi2);

	float	hn1x, hn1y, hn1z;
	float	hn2x, hn2y, hn2z;
	float	 hnl;
	
	if (fabs(vs->v[2]) >= fabs(vs->v[0]) && fabs(vs->v[2]) >= fabs(vs->v[1])) 
	{
		hn1x = vs->v[1]; hn1y = 0; hn1z = - vs->v[0];
		chi2 /= (hn1x * hn1x + hn1z * hn1z);
		hn2x = vs->v[1] * hn1z;
		hn2y = vs->v[2] * hn1x - vs->v[0] * hn1z;
		hn2z =                 - vs->v[1] * hn1x;
	} 
	else 
	{
		hn1x = - vs->v[1]; hn1y = vs->v[0]; hn1z = 0;
		chi2 /= (hn1x * hn1x + hn1y * hn1y );
		hn2x =                 - vs->v[2] * hn1y;
		hn2y = vs->v[2] * hn1x;
		hn2z = vs->v[0] * hn1y - vs->v[1] * hn1x;
	}
	
	float	m1 = cos(2 * PI * chi1) * sqrt(chi2);
	float	m2 = sin(2 * PI * chi1) * sqrt(chi2);

	Vector3	vt;
	vt.v[0] = vs->v[0] * m0 + hn1x * m1 + hn2x * m2;
	vt.v[1] = vs->v[1] * m0 + hn1y * m1 + hn2y * m2;
	vt.v[2] = vs->v[2] * m0 + hn1z * m1 + hn2z * m2;
	*vd = vt;
}



void mat3_ident(Matrix3 * m)
{
	m->m[0] = 1.0;
	m->m[1] = 0.0;
	m->m[2] = 0.0;

	m->m[3] = 0.0;
	m->m[4] = 1.0;
	m->m[5] = 0.0;

	m->m[6] = 0.0;
	m->m[7] = 0.0;
	m->m[8] = 1.0;
}

void mat3_scale(Matrix3 * md, float s)
{
	for(char i=0; i<9; i++)
		md->m[i] *= s;
}

void mat3_add(Matrix3 * md, const Matrix3 * ms)
{
	for(char i=0; i<9; i++)
		md->m[i] += md->m[i];
}

void mat3_mmul(Matrix3 * md, const Matrix3 * ms)
{
	for(char i=0; i<3; i++)
	{
		float	j = 3 * i;

		float	m0 = md->m[i + 0] * ms->m[0] + md->m[i + 3] * ms->m[1] + md->m[i + 6] * ms->m[2];
		float	m3 = md->m[i + 0] * ms->m[3] + md->m[i + 3] * ms->m[4] + md->m[i + 6] * ms->m[5];
		float	m6 = md->m[i + 0] * ms->m[6] + md->m[i + 3] * ms->m[7] + md->m[i + 6] * ms->m[8];

		md->m[i + 0] = m0; md->m[i + 3] = m3; md->m[i + 6] = m6;
	}
}

void mat3_rmmul(Matrix3 * md, const Matrix3 * ms)
{
	for(char i=0; i<9; i+=3)
	{
		float	m0 = md->m[i + 0] * ms->m[0] + md->m[i + 1] * ms->m[3] + md->m[i + 2] * ms->m[6];
		float	m1 = md->m[i + 0] * ms->m[1] + md->m[i + 1] * ms->m[4] + md->m[i + 2] * ms->m[7];
		float	m2 = md->m[i + 0] * ms->m[2] + md->m[i + 1] * ms->m[5] + md->m[i + 2] * ms->m[8];

		md->m[i + 0] = m0; md->m[i + 1] = m1; md->m[i + 2] = m2;
	}
}

void mat3_transpose(Matrix3 * md, const Matrix3 * ms)
{
	float	t
	t = ms->m[1]; md->m[1] = ms->m[3]; md->m[3] = t; md->m[0] = ms->m[0];
	t = ms->m[2]; md->m[2] = ms->m[6]; md->m[6] = t; md->m[4] = ms->m[4];
	t = ms->m[5]; md->m[5] = ms->m[7]; md->m[7] = t; md->m[8] = ms->m[8];
}

float mat3_det(const Matrix3 * ms)
{
	return	ms->m[0] * ms->m[4] * ms->m[8] +
	      	ms->m[1] * ms->m[5] * ms->m[6] +
			ms->m[2] * ms->m[3] * ms->m[7] -
			ms->m[0] * ms->m[5] * ms->m[7] -
			ms->m[2] * ms->m[4] * ms->m[6] -
			ms->m[1] * ms->m[3] * ms->m[8];
}

void mat3_invert(Matrix3 * md, const Matrix3 * ms)
{
	float	det = 1 / mat3_det(ms);

	Matrix3	mt;
	mt.m[0] = det * (ms->m[4] * ms->m[8] - ms->m[5] * ms->m[7]);
	mt.m[1] = det * (ms->m[7] * ms->m[2] - ms->m[1] * ms->m[8]);
	mt.m[2] = det * (ms->m[1] * ms->m[5] - ms->m[4] * ms->m[2]);
	
	mt.m[3] = det * (ms->m[6] * ms->m[5] - ms->m[3] * ms->m[8]);
	mt.m[4] = det * (ms->m[0] * ms->m[8] - ms->m[6] * ms->m[2]);
	mt.m[5] = det * (ms->m[3] * ms->m[2] - ms->m[0] * ms->m[5]);
	
	mt.m[6] = det * (ms->m[3] * ms->m[7] - ms->m[6] * ms->m[4]);
	mt.m[7] = det * (ms->m[6] * ms->m[1] - ms->m[0] * ms->m[7]);
	mt.m[8] = det * (ms->m[0] * ms->m[4] - ms->m[3] * ms->m[1]);

	*md = mt;
}

void vec3_mmul(Vector3 * vd, const Matrix3 * m, const Vector3 * vs)
{
	Vector3	vt;
	for(char i=0; i<3; i++)
		vt.v[i] = m->m[i] * vs->v[0] + m->m[3 + i] * vs->v[1] + m->m[6 + i] * vs->v[2];
	*vd = vt;
}

void mat3_set_rotate_x(Matrix3 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[0] = 1; m->m[3] = 0; m->m[6] = 0;
	m->m[1] = 0; m->m[4] = c; m->m[7] = s;
	m->m[2] = 0; m->m[5] =-s; m->m[8] = c;
}

void mat3_set_rotate_y(Matrix3 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[0] = c; m->m[3] = 0; m->m[6] = s;
	m->m[1] = 0; m->m[4] = 1; m->m[7] = 0;
	m->m[2] =-s; m->m[5] = 0; m->m[8] = c;
}


void mat3_set_rotate_z(Matrix3 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[0] = c; m->m[3] =-s; m->m[6] = 0;
	m->m[1] = s; m->m[4] = c; m->m[7] = 0;
	m->m[2] = 0; m->m[5] = 0; m->m[8] = 1;
}

void mat3_set_rotate(Matrix3 * m, const Vector3 * v, float a)
{
	float	co = cos(a);
	float	si = sin(a);
	float	ico = (1- co);

	m->m[0] = co + v->v[0]*v->v[0]*ico; 
	m->m[1] = v->v[1]*v->v[0]*ico+v->v[2]*si; 
	m->m[2] = v->v[2]*v->v[0]*ico-v->v[1]*si;

	m->m[3] = v->v[0]*v->v[1]*ico - v->v[2]*si; 
	m->m[4] = co + v->v[1]*v->v[1]*ico; 
	m->m[5] = v->v[2]*v->v[1]*ico+v->v[0]*si;

	m->m[6] = v->v[0]*v->v[2]*ico + v->v[1]*si; 
	m->m[7] = v->v[1]*v->v[2]*ico - v->v[0]*si; 
	m->m[8] = co + v->v[2]*v->v[2]*ico;
}





inline void vec4_set(Vector4 * vd, float x, float y, float z, float w)
{
	vd->v[0] = x;
	vd->v[1] = y;
	vd->v[2] = z;
	vd->v[3] = w;
}


void vec4_sum(Vector4 * vd, const Vector4 * v1, const Vector4 * v2)
{
	for(char i=0; i<4; i++)
		vd->v[i] = v1->v[i] + v2->v[i];
}

void vec4_diff(Vector4 * vd, const Vector4 * v1, const Vector4 * v2)
{
	for(char i=0; i<4; i++)
		vd->v[i] = v1->v[i] - v2->v[i];
}

void vec4_add(Vector4 * vd, const Vector4 * vs)
{
	for(char i=0; i<4; i++)
		vd->v[i] += vs->v[i];
}

void vec4_madd(Vector4 * vd, float s, const Vector4 * vs)
{
	for(char i=0; i<4; i++)
		vd->v[i] += s * vs->v[i];
}

void vec4_sub(Vector4 * vd, const Vector4 * vs)
{
	for(char i=0; i<4; i++)
		vd->v[i] -= vs->v[i];
}

void vec4_scale(Vector4 * vd, float s)
{
	for(char i=0; i<4; i++)
		vd->v[i] *= s;
}

void vec4_lincomb(Vector4 * vd, const Vector4 * vb, float x, const Vector4 * vx)
{
	for(char i=0; i<4; i++)
		vd->v[i] = vb->v[i] + x * vx->v[i];
}

void vec4_lincomb2(Vector4 * vd, const Vector4 * vb, float x, const Vector4 * vx, float y, const Vector4 * vy)
{
	for(char i=0; i<4; i++)
		vd->v[i] = vb->v[i] + x * vx->v[i] + y * vy->v[i];
}

void vec4_lerp(Vector4 * vd, const Vector4 * v1, const Vector4 * v2, float t)
{
	for(char i=0; i<4; i++)
		vd->v[i] = (1 - t) * v1->v[i] + t * v2->v[i];
}

bool vec4_is_zero(const Vector4 * v)
{
	return v->v[0] == 0 && v->v[1] == 0 && v->v[2] == 0 && v->v[3] == 0;
}

float vec4_vmul(const Vector4 * v1, const Vector4 * v2)
{
	return v1->v[0] * v2->v[0] + v1->v[1] * v2->v[1] + v1->v[2] * v2->v[2] + v1->v[3] * v2->v[3];
}

void vec4_norm(Vector4 * v)
{
	vec4_scale(v, 1.0 / vec4_length(v));
}

float vec4_length(const Vector4 * v)
{
	return sqrt(vec4_qlength(v));
}

float vec4_qlength(const Vector4 * v)
{
	return vec4_vmul(v, v);
}

float vec4_distance(const Vector4 * v1, const Vector4 * v2)
{
	return sqrt(vec4_qdistance(v1, v2));
}

float vec4_qdistance(const Vector4 * v1, const Vector4 * v2)
{
	float	dx = v1->v[0] - v2->v[0], dy = v1->v[1] - v2->v[1], dz = v1->v[2] - v2->v[2], dw = v1->v[3] - v2->v[3];
	return dx * dx + dy * dy + dz * dz + dw * dw;
}

void vec4_mcadd(Vector4 * vd, const Vector4 * v1, const Vector4 * v2)
{
	for(char i=0; i<4; i++)
		vd->v[i] += v1->v[i] * v2->v[i];
}

void vec4_mscadd(Vector4 * vd, float s, const Vector4 * v1, const Vector4 * v2)
{
	for(char i=0; i<4; i++)
		vd->v[i] += s * v1->v[i] * v2->v[i];
}

void vec4_cmul(Vector4 * vd, const Vector4 * v1, const Vector4 * v2)
{
	for(char i=0; i<4; i++)
		vd->v[i] = v1->v[i] * v2->v[i];	
}



void mat4_ident(Matrix4 * m)
{
	for(char i=0; i<4; i++)
		for(char j=0; j<4; j++)
			m->m[4 * i + j] = (i == j) ? 1 : 0;
}

void mat4_from_vec4(Matrix4 * m, const Vector4 * vx, const Vector4 * vy, const Vector4 * vz, const Vector4 * vw)
{
	m->m[ 0] = vx->v[0];
	m->m[ 1] = vx->v[1];
	m->m[ 2] = vx->v[2];
	m->m[ 3] = vx->v[3];
	
	m->m[ 4] = vy->v[0];
	m->m[ 5] = vy->v[1];
	m->m[ 6] = vy->v[2];
	m->m[ 7] = vy->v[3];

	m->m[ 8] = vz->v[0];
	m->m[ 9] = vz->v[1];
	m->m[10] = vz->v[2];
	m->m[11] = vz->v[3];

	m->m[12] = vw->v[0];
	m->m[13] = vw->v[1];
	m->m[14] = vw->v[2];
	m->m[15] = vw->v[3];
}

void mat4_from_vec3(Matrix4 * m, const Vector3 * vx, const Vector3 * vy, const Vector3 * vz, const Vector3 * vw)
{
	m->m[ 0] = vx->v[0];
	m->m[ 1] = vx->v[1];
	m->m[ 2] = vx->v[2];
	m->m[ 3] = 0;
	
	m->m[ 4] = vy->v[0];
	m->m[ 5] = vy->v[1];
	m->m[ 6] = vy->v[2];
	m->m[ 7] = 0;

	m->m[ 8] = vz->v[0];
	m->m[ 9] = vz->v[1];
	m->m[10] = vz->v[2];
	m->m[11] = 0;

	if (vw)
	{
		m->m[12] = vw->v[0];
		m->m[13] = vw->v[1];
		m->m[14] = vw->v[2];
		m->m[15] = 1;
	}
	else
	{
		m->m[12] = 0;
		m->m[13] = 0;
		m->m[14] = 0;
		m->m[15] = 0;		
	}
}

void mat4_make_perspective(Matrix4 * m, float fieldOfViewInRadians, float aspect, float near, float far)
{
	float f = tan(PI * 0.5 - 0.5 * fieldOfViewInRadians);
	float	rangeInv = 1.0 / (far - near);
 
 	m->m[ 0] = f / aspect; 
 	m->m[ 1] = 0;
 	m->m[ 2] = 0;
 	m->m[ 3] = 0;

 	m->m[ 4] = 0;
 	m->m[ 5] = f;
 	m->m[ 6] = 0;
 	m->m[ 7] = 0;

 	m->m[ 8] = 0;
 	m->m[ 9] = 0;
 	m->m[10] = (near + far) * rangeInv;
 	m->m[11] = 1;

 	m->m[12] = 0;
 	m->m[13] = 0;
 	m->m[14] = near * far * rangeInv * -2;
 	m->m[15] = 0;
}

void mat4_make_view(Matrix4 * m, const Vector3 * pos, const Vector3 * target, const Vector3 * up)
{
	Vector3	vx, vy, vz;

	vec3_diff(&vz, target, pos); vec3_norm(&vz);
	vec3_xmul(&vx, up, &vz);
	vec3_xmul(&vy, &vz, &vx);

	mat4_from_vec3(m, &vx, &vy, &vz, pos);
	mat4_invert(m);
}


void mat4_scale(Matrix4 * md, float s)
{
	for(char i=0; i<16; i++)
		md->m[i] *= s;
}

void mat4_add(Matrix4 * md, const Matrix4 * ms)
{
	for(char i=0; i<16; i++)
		md->m[i] += md->m[i];
}

void mat4_mmul(Matrix4 * md, const Matrix4 * ms)
{
	float	ma[4];
	for(char i=0; i<4; i++)
	{
		for(char j=0; j<4; j++)
			ma[j] = md->m[i + 0] * ms->m[4 * j] + md->m[i + 4] * ms->m[4 * j + 1] + md->m[i + 8] * ms->m[4 * j + 2] + md->m[i + 12] * ms->m[4 * j + 3];
		for(char j=0; j<4; j++)
			md->m[(char)(i + 4 * j)] = ma[j]; 
	}
}

void mat4_rmmul(Matrix4 * md, const Matrix4 * ms)
{
	float	ma[4];
	for(char i=0; i<16; i+=4)
	{
		for(char j=0; j<4; j++)
			ma[j] = md->m[i + 0] * ms->m[j] + md->m[i + 1] * ms->m[j + 4] + md->m[i + 2] * ms->m[j + 8] + md->m[i + 3] * ms->m[j + 12];

		for(char j=0; j<4; j++)
			md->m[i + j] = ma[j];
	}
}

float mat4_det(const Matrix4 * m)
{
	return	m->m[ 0] * m->m[ 5] * m->m[10] * m->m[15] + m->m[ 0] * m->m[ 9] * m->m[14] * m->m[ 7] + m->m[ 0] * m->m[13] * m->m[ 6] * m->m[11] +
			m->m[ 4] * m->m[ 1] * m->m[14] * m->m[11] + m->m[ 4] * m->m[ 9] * m->m[ 2] * m->m[15] + m->m[ 4] * m->m[13] * m->m[10] * m->m[ 3] +
			m->m[ 8] * m->m[ 1] * m->m[ 6] * m->m[15] + m->m[ 8] * m->m[ 5] * m->m[14] * m->m[ 3] + m->m[ 8] * m->m[13] * m->m[ 2] * m->m[ 7] +
			m->m[12] * m->m[ 1] * m->m[10] * m->m[ 7] + m->m[12] * m->m[ 5] * m->m[ 2] * m->m[11] + m->m[12] * m->m[ 9] * m->m[ 6] * m->m[ 3] -
			m->m[ 0] * m->m[ 5] * m->m[14] * m->m[11] - m->m[ 0] * m->m[ 9] * m->m[ 6] * m->m[15] - m->m[ 0] * m->m[13] * m->m[10] * m->m[ 7] -
			m->m[ 4] * m->m[ 1] * m->m[10] * m->m[15] - m->m[ 4] * m->m[ 9] * m->m[14] * m->m[ 3] - m->m[ 4] * m->m[13] * m->m[ 2] * m->m[11] -
			m->m[ 8] * m->m[ 1] * m->m[14] * m->m[ 7] - m->m[ 8] * m->m[ 5] * m->m[ 2] * m->m[15] - m->m[ 8] * m->m[13] * m->m[ 6] * m->m[ 3] -
			m->m[12] * m->m[ 1] * m->m[ 6] * m->m[11] - m->m[12] * m->m[ 5] * m->m[10] * m->m[ 3] - m->m[12] * m->m[ 9] * m->m[ 2] * m->m[ 7];
}

void mat4_invert(Matrix4 * md, const Matrix4 * ms)
{
	float	det = 1 / mat4_det(ms);

	Matrix4	mt;

	mt.m[ 0] = det * (ms->m[ 5] * ms->m[10] * ms->m[15]+ms->m[ 9] * ms->m[14] * ms->m[ 7]+ms->m[13] * ms->m[ 6] * ms->m[11]-ms->m[ 5] * ms->m[14] * ms->m[11]-ms->m[ 9] * ms->m[ 6] * ms->m[15]-ms->m[13] * ms->m[10] * ms->m[ 7]);
	mt.m[ 4] = det * (ms->m[ 4] * ms->m[14] * ms->m[11]+ms->m[ 8] * ms->m[ 6] * ms->m[15]+ms->m[12] * ms->m[10] * ms->m[ 7]-ms->m[ 4] * ms->m[10] * ms->m[15]-ms->m[ 8] * ms->m[14] * ms->m[ 7]-ms->m[12] * ms->m[ 6] * ms->m[11]);
	mt.m[ 8] = det * (ms->m[ 4] * ms->m[ 9] * ms->m[15]+ms->m[ 8] * ms->m[13] * ms->m[ 7]+ms->m[12] * ms->m[ 5] * ms->m[11]-ms->m[ 4] * ms->m[13] * ms->m[11]-ms->m[ 8] * ms->m[ 5] * ms->m[15]-ms->m[12] * ms->m[ 9] * ms->m[ 7]);
	mt.m[12] = det * (ms->m[ 4] * ms->m[13] * ms->m[10]+ms->m[ 8] * ms->m[ 5] * ms->m[14]+ms->m[12] * ms->m[ 9] * ms->m[ 6]-ms->m[ 4] * ms->m[ 9] * ms->m[14]-ms->m[ 8] * ms->m[13] * ms->m[ 6]-ms->m[12] * ms->m[ 5] * ms->m[10]);
	
	mt.m[ 1] = det * (ms->m[ 1] * ms->m[14] * ms->m[11]+ms->m[ 9] * ms->m[ 2] * ms->m[15]+ms->m[13] * ms->m[10] * ms->m[ 3]-ms->m[ 1] * ms->m[10] * ms->m[15]-ms->m[ 9] * ms->m[14] * ms->m[ 3]-ms->m[13] * ms->m[ 2] * ms->m[11]);
	mt.m[ 5] = det * (ms->m[ 0] * ms->m[10] * ms->m[15]+ms->m[ 8] * ms->m[14] * ms->m[ 3]+ms->m[12] * ms->m[ 2] * ms->m[11]-ms->m[ 0] * ms->m[14] * ms->m[11]-ms->m[ 8] * ms->m[ 2] * ms->m[15]-ms->m[12] * ms->m[10] * ms->m[ 3]);
	mt.m[ 9] = det * (ms->m[ 0] * ms->m[13] * ms->m[11]+ms->m[ 8] * ms->m[ 1] * ms->m[15]+ms->m[12] * ms->m[ 9] * ms->m[ 3]-ms->m[ 0] * ms->m[ 9] * ms->m[15]-ms->m[ 8] * ms->m[13] * ms->m[ 3]-ms->m[12] * ms->m[ 1] * ms->m[11]);
	mt.m[13] = det * (ms->m[ 0] * ms->m[ 9] * ms->m[14]+ms->m[ 8] * ms->m[13] * ms->m[ 2]+ms->m[12] * ms->m[ 1] * ms->m[10]-ms->m[ 0] * ms->m[13] * ms->m[10]-ms->m[ 8] * ms->m[ 1] * ms->m[14]-ms->m[12] * ms->m[ 9] * ms->m[ 2]);
	
	mt.m[ 2] = det * (ms->m[ 1] * ms->m[ 6] * ms->m[15]+ms->m[ 5] * ms->m[14] * ms->m[ 3]+ms->m[13] * ms->m[ 2] * ms->m[ 7]-ms->m[ 1] * ms->m[14] * ms->m[ 7]-ms->m[ 5] * ms->m[ 2] * ms->m[15]-ms->m[13] * ms->m[ 6] * ms->m[ 3]);
	mt.m[ 6] = det * (ms->m[ 0] * ms->m[14] * ms->m[ 7]+ms->m[ 4] * ms->m[ 2] * ms->m[15]+ms->m[12] * ms->m[ 6] * ms->m[ 3]-ms->m[ 0] * ms->m[ 6] * ms->m[15]-ms->m[ 4] * ms->m[14] * ms->m[ 3]-ms->m[12] * ms->m[ 2] * ms->m[ 7]);
	mt.m[10] = det * (ms->m[ 0] * ms->m[ 5] * ms->m[15]+ms->m[ 4] * ms->m[13] * ms->m[ 3]+ms->m[12] * ms->m[ 1] * ms->m[ 7]-ms->m[ 0] * ms->m[13] * ms->m[ 7]-ms->m[ 4] * ms->m[ 1] * ms->m[15]-ms->m[12] * ms->m[ 5] * ms->m[ 3]);
	mt.m[14] = det * (ms->m[ 0] * ms->m[13] * ms->m[ 6]+ms->m[ 4] * ms->m[ 1] * ms->m[14]+ms->m[12] * ms->m[ 5] * ms->m[ 2]-ms->m[ 0] * ms->m[ 5] * ms->m[14]-ms->m[ 4] * ms->m[13] * ms->m[ 2]-ms->m[12] * ms->m[ 1] * ms->m[ 6]);
	
	mt.m[ 3] = det * (ms->m[ 1] * ms->m[10] * ms->m[ 7]+ms->m[ 5] * ms->m[ 2] * ms->m[11]+ms->m[ 9] * ms->m[ 6] * ms->m[ 3]-ms->m[ 1] * ms->m[ 6] * ms->m[11]-ms->m[ 5] * ms->m[10] * ms->m[ 3]-ms->m[ 9] * ms->m[ 2] * ms->m[ 7]);
	mt.m[ 7] = det * (ms->m[ 0] * ms->m[ 6] * ms->m[11]+ms->m[ 4] * ms->m[10] * ms->m[ 3]+ms->m[ 8] * ms->m[ 2] * ms->m[ 7]-ms->m[ 0] * ms->m[10] * ms->m[ 7]-ms->m[ 4] * ms->m[ 2] * ms->m[11]-ms->m[ 8] * ms->m[ 6] * ms->m[ 3]);
	mt.m[11] = det * (ms->m[ 0] * ms->m[ 9] * ms->m[ 7]+ms->m[ 4] * ms->m[ 1] * ms->m[11]+ms->m[ 8] * ms->m[ 5] * ms->m[ 3]-ms->m[ 0] * ms->m[ 5] * ms->m[11]-ms->m[ 4] * ms->m[ 9] * ms->m[ 3]-ms->m[ 8] * ms->m[ 1] * ms->m[ 7]);
	mt.m[15] = det * (ms->m[ 0] * ms->m[ 5] * ms->m[10]+ms->m[ 4] * ms->m[ 9] * ms->m[ 2]+ms->m[ 8] * ms->m[ 1] * ms->m[ 6]-ms->m[ 0] * ms->m[ 9] * ms->m[ 6]-ms->m[ 4] * ms->m[ 1] * ms->m[10]-ms->m[ 8] * ms->m[ 5] * ms->m[ 2]);

	*md = mt;
}


void vec4_mmul(Vector4 * vd, const Matrix4 * m, const Vector4 * vs)
{
	Vector4	vt;
	for(char i=0; i<4; i++)
		vt.v[i] = m->m[0 + i] * vs->v[0] + m->m[4 + i] * vs->v[1] + m->m[8 + i] * vs->v[2] + m->m[12 + i] * vs->v[3];
	*vd = vt;
}

void vec3_mmulp(Vector3 * vd, const Matrix4 * m, const Vector3 * vs)
{
	Vector3	vt;
	for(char i=0; i<3; i++)
		vt.v[i] = m->m[0 + i] * vs->v[0] + m->m[4 + i] * vs->v[1] + m->m[8 + i] * vs->v[2] + m->m[12 + i];
	*vd = vt;	
}

void vec3_mmuld(Vector3 * vd, const Matrix4 * m, const Vector3 * vs)
{
	Vector3	vt;
	for(char i=0; i<3; i++)
		vt.v[i] = m->m[0 + i] * vs->v[0] + m->m[4 + i] * vs->v[1] + m->m[8 + i] * vs->v[2];
	*vd = vt;	
}


void mat4_set_rotate_x(Matrix4 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[ 0] = 1; m->m[ 4] = 0; m->m[ 8] = 0; m->m[12] = 0;
	m->m[ 1] = 0; m->m[ 5] = c; m->m[ 9] = s; m->m[13] = 0;
	m->m[ 2] = 0; m->m[ 6] =-s; m->m[10] = c; m->m[14] = 0;
	m->m[ 3] = 0; m->m[ 7] = 0; m->m[11] = 0; m->m[15] = 1;
}

void mat4_set_rotate_y(Matrix4 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[ 0] = c; m->m[ 4] = 0; m->m[ 8] = s; m->m[12] = 0;
	m->m[ 1] = 0; m->m[ 5] = 1; m->m[ 9] = 0; m->m[13] = 0;
	m->m[ 2] =-s; m->m[ 6] = 0; m->m[10] = c; m->m[14] = 0;
	m->m[ 3] = 0; m->m[ 7] = 0; m->m[11] = 0; m->m[15] = 1;
}


void mat4_set_rotate_z(Matrix4 * m, float a)
{
	float	c = cos(a);
	float	s = sin(a);
	m->m[ 0] = c; m->m[ 4] =-s; m->m[ 8] = 0; m->m[12] = 0;
	m->m[ 1] = s; m->m[ 5] = c; m->m[ 9] = 0; m->m[13] = 0;
	m->m[ 2] = 0; m->m[ 6] = 0; m->m[10] = 1; m->m[14] = 0;
	m->m[ 3] = 0; m->m[ 7] = 0; m->m[11] = 0; m->m[15] = 1;
}

void mat4_set_rotate(Matrix4 * m, const Vector3 * v, float a)
{
	float	co = cos(a);
	float	si = sin(a);
	float	ico = (1- co);

	m->m[ 0] = co + v->v[0]*v->v[0]*ico; 
	m->m[ 1] = v->v[1]*v->v[0]*ico+v->v[2]*si; 
	m->m[ 2] = v->v[2]*v->v[0]*ico-v->v[1]*si;
	m->m[ 3] = 0;

	m->m[ 4] = v->v[0]*v->v[1]*ico - v->v[2]*si; 
	m->m[ 5] = co + v->v[1]*v->v[1]*ico; 
	m->m[ 6] = v->v[2]*v->v[1]*ico+v->v[0]*si;
	m->m[ 7] = 0;

	m->m[ 8] = v->v[0]*v->v[2]*ico + v->v[1]*si; 
	m->m[ 9] = v->v[1]*v->v[2]*ico - v->v[0]*si; 
	m->m[10] = co + v->v[2]*v->v[2]*ico;
	m->m[11] = 0;

	m->m[12] = 0;
	m->m[13] = 0;
	m->m[14] = 0;
	m->m[15] = 1;
}


void mat4_set_translate(Matrix4 * m, const Vector3 * v)
{
	mat4_ident(m);
	m->m[12] = v->v[0];
	m->m[13] = v->v[1];
	m->m[14] = v->v[2];
}

void mat4_set_scale(Matrix4 * m, float s)
{
	mat4_ident(m);
	m->m[ 0] = s;
	m->m[ 5] = s;
	m->m[10] = s;
}


void vec3_project(Vector3 * vd, const Matrix4 * m, const Vector3 * vs)
{
	vec3_mmulp(vd, m, vs);
	vd->v[0] /= vd->v[2];
	vd->v[1] /= vd->v[2];
}
