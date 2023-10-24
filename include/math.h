#ifndef MATH_H
#define MATH_H

#define	PI	3.141592653


float fabs(float f);
float floor(float f);
float ceil(float f);

float cos(float f);
float sin(float f);
float tan(float f);
float acos(float f);
float asin(float f);
float atan(float f);
float atan2(float p, float q);

float exp(float f);
float log(float f);
float log10(float f);

float pow(float p, float q);
float sqrt(float f);

bool isinf(float f);
bool isfinite(float f);

#pragma intrinsic(fabs)
#pragma intrinsic(floor)
#pragma intrinsic(ceil)

#pragma intrinsic(sin)
#pragma intrinsic(cos)

#pragma compile("math.c")


#endif

