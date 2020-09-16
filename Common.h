/*
Container area for a lot of garbage I probably don't use.
*/

// TYPEDEFS

typedef struct
{
	float3 c0, c1, c2;
} mat3;

__DEVICE__ mat3 make_mat3(float3 a, float3 b, float3 c)
{
	mat3 result;
	result.c0 = a;
	result.c1 = b;
	result.c2 = c;

	return result;
}

typedef struct
{
	float2 x, y, z, w;
} Chromaticities;

// COLORSPACE CRAP
__CONSTANT__ Chromaticities BMD_FILM_PRI = {
	{0.9173f, 0.2502f},
	{0.2833f, 1.7072f},
	{0.0856f, -0.0708f},
	{0.3135f, 0.3305f}
};

// UTIL FUNCTIONS
__DEVICE__ float getLum(float3 rgb)
{
	return 0.2126f * rgb.x + 0.7152f * rgb.y + 0.0722f * rgb.z;
}

__DEVICE__ float interpolate1D( float2 table[], int Size, float p) {
	if (p <= table[0].x) return table[0].y;
	if (p >= table[Size - 1].x) return table[Size - 1].y;

	for (int i = 0; i < Size - 1; ++i)
	{
		if (table[i].x <= p && p < table[i + 1].x ){
			float s = (p - table[i].x) / (table[i + 1].x - table[i].x);
			return table[i].y * (1.0f - s) + table[i + 1].y * s;
		}
	}

	return 0.0f;
}

__DEVICE__ inline float _pow10f(float x) {
	return _powf(10.0f, x);
}


__DEVICE__ float lerp(float min, float max, float a)
{
	return (a - min) / (max - min);
}

__DEVICE__ float3 mult_f3_f33( float3 X, mat3 A) {
	float r[3];
	float x[3] = {X.x, X.y, X.z};
	float a[3][3] =	{
		{A.c0.x, A.c0.y, A.c0.z},
		{A.c1.x, A.c1.y, A.c1.z},
		{A.c2.x, A.c2.y, A.c2.z}
	};

	for (int i = 0; i < 3; ++i)
	{
		r[i] = 0.0f;
		for (int j = 0; j < 3; ++j)
		{
			r[i] = r[i] + x[j] * a[j][i];
		}
	}

	return make_float3(r[0], r[1], r[2]);
}

__DEVICE__ mat3 mult_f_f33( float f, mat3 A) {
	float r[3][3];
	float a[3][3] = {{A.c0.x, A.c0.y, A.c0.z}, {A.c1.x, A.c1.y, A.c1.z}, {A.c2.x, A.c2.y, A.c2.z}};
	for( int i = 0; i < 3; ++i ){
	for( int j = 0; j < 3; ++j ){
		r[i][j] = f * a[i][j];
	}}
	mat3 R = make_mat3(
		make_float3(r[0][0], r[0][1], r[0][2]),
		make_float3(r[1][0], r[1][1], r[1][2]),
		make_float3(r[2][0], r[2][1], r[2][2]));
	return R;
}

__DEVICE__ float determinant(mat3 A)
{
	float a[3][3] = {{A.c0.x, A.c0.y, A.c0.z}, {A.c1.x, A.c1.y, A.c1.z}, {A.c2.x, A.c2.y, A.c2.z}};
	float det = a[0][0] * a[1][1] * a[2][2] + a[0][1] * a[1][2] * a[2][0]
	+ a[0][2] * a[1][0] * a[2][1] - a[2][0] * a[1][1] * a[0][2]
	- a[2][1] * a[1][2] * a[0][0] - a[2][2] * a[1][0] * a[0][1];	

	return det;
}

__DEVICE__ mat3 invert_f33(mat3 A) {
	mat3 R;
	float result[3][3];

	float det = determinant(A);

	if (det != 0.0f)
	{
		float a[3][3] = {{A.c0.x, A.c0.y, A.c0.z}, {A.c1.x, A.c1.y, A.c1.z}, {A.c2.x, A.c2.y, A.c2.z}};
		result[0][0] = a[1][1] * a[2][2] - a[1][2] * a[2][1];
		result[0][1] = a[2][1] * a[0][2] - a[2][2] * a[0][1];


		result[0][2] = a[0][1] * a[1][2] - a[0][2] * a[1][1]; result[1][0] = a[2][0] * a[1][2] - a[1][0] * a[2][2];
		result[1][1] = a[0][0] * a[2][2] - a[2][0] * a[0][2]; result[1][2] = a[1][0] * a[0][2] - a[0][0] * a[1][2];
		result[2][0] = a[1][0] * a[2][1] - a[2][0] * a[1][1]; result[2][1] = a[2][0] * a[0][1] - a[0][0] * a[2][1];
		result[2][2] = a[0][0] * a[1][1] - a[1][0] * a[0][1];

		R = make_mat3(make_float3(
				result[0][0],
				result[0][1],
				result[0][2]),
			make_float3(
				result[1][0],
				result[1][1],
				result[1][2]),
			make_float3(
				result[2][0],
				result[2][1], 
				result[2][2]
			)
		);
		return mult_f_f33( 1.0f / det, R);
	}

	R = make_mat3(make_float3(1.0f, 0.0f, 0.0f),
		make_float3(0.0f, 1.0f, 0.0f),
		make_float3(0.0f, 0.0f, 1.0f));

	return R;
} 

// Weird RGB / YAB / YCH crap
__CONSTANT__ mat3 RGB_2_YAB_MAT = {
	{0.333333f, 0.5f, 0.0f},
	{0.333333f, -0.25f, 0.433012701892219f},
	{0.333333f, -0.25f, -0.433012701892219f}
};

__CONSTANT__ mat3 YAB_2_RGB_MAT = {
	{1.0f, 1.0f, 1.0f}, 
	{1.333333f, -0.666666f, -0.666666f},
	{0.0f, 1.154701f, -1.154701f}
};

__DEVICE__ float3 rgb_2_yab(float3 rgb)
{
	float3 yab = mult_f3_f33(rgb, RGB_2_YAB_MAT);

	return yab;
}

__DEVICE__ float3 yab_2_rgb(float3 yab)
{
	//float3 rgb = mult_f3_f33(yab, invert_f33(RGB_2_YAB_MAT));
	float3 rgb = mult_f3_f33(yab, YAB_2_RGB_MAT);

	return rgb;
}

__DEVICE__ float3 yab_2_ych(float3 yab)
{
	float3 ych = yab;
	ych.y = _sqrtf(ych.y * ych.y + yab.z * yab.z);
	ych.z = _atan2f(yab.z, yab.y) * (180.0f / M_PI);

	if (ych.z < 0.0f) ych.z = ych.z += 360.0f;

	return ych;
}

__DEVICE__ float3 ych_2_yab(float3 ych)
{
	float3 yab;
	yab.x = ych.x;
	float h = ych.z * (M_PI / 180.0f);
	yab.y = ych.y * _cosf(h);
	yab.z = ych.y * _sinf(h);

	return yab;
}
__DEVICE__ inline float3 rgb_2_ych(float3 rgb)
{
	return yab_2_ych(rgb_2_yab(rgb));
}

__DEVICE__ inline float3 ych_2_rgb(float3 ych)
{
	return yab_2_rgb(ych_2_yab(ych));
}

