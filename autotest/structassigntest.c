#include <stdio.h>

struct Vec3
{
	float	x, y, z;
};

Vec3	v;

Vec3 vadd(Vec3 s1, Vec3 s2)
{
	Vec3	r;
	r.x = s1.x + s2.x;
	r.y = s1.y + s2.y;
	r.z = s1.z + s2.z;
	return r;
}

	
int main(void)
{
	Vec3	m = {1, 2, -3}, u = {4, 5, -9}, t = {7, -2, -5};
	
	v.x = 99;
	v.y = 100;
	v.z = 101;
	
	v = vadd(m, vadd(u, t));
	
	return v.x + v.y + v.z;
}
