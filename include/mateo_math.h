#pragma once
#define PI 3.14159265358979323846
#include <math.h>

struct v2
{
	union
	{
		struct
		{
			float x, y;
		};
		float E[2];
	};

	inline v2& operator*=(float a);
	inline v2& operator+=(v2 a);
};
inline v2 operator*(float a, v2 b)
{
	v2 result;
	result.x = a*b.x;
	result.y = a*b.y;

	return result;
}
inline v2 operator-(v2 a, v2 b)
{
	v2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;

	return result;
}
inline v2 operator-(v2 a)
{
	v2 result;
	result.x = -a.x;
	result.y = -a.y;

	return result;
}
inline v2 operator+(v2 a, v2 b)
{
	v2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;

	return result;
}
inline v2& v2::operator*=(float a)
{
	*this = a * *this;
	return(*this);
}
inline v2& v2::operator+=(v2 a)
{
	*this = *this + a;
	return(*this);
}

struct v3
{
	union
	{
		struct
		{
			float x, y, z;
		};
		float E[3];
	};

	inline v3& operator*=(float a);
	inline v3& operator+=(v3 a);
};
inline v3 operator*(float a, v3 b)
{
	v3 result;
	result.x = a * b.x;
	result.y = a * b.y;
	result.z = a * b.z;

	return result;
}
inline v3 operator-(v3 a, v3 b)
{
	v3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z- b.z;

	return result;
}
inline v3 operator-(v3 a)
{
	v3 result;
	result.x = -a.x;
	result.y = -a.y;
	result.z = -a.z;

	return result;
}
inline v3 operator+(v3 a, v3 b)
{
	v3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;

	return result;
}
inline v3& v3::operator*=(float a)
{
	*this = a * *this;
	return(*this);
}
inline v3& v3::operator+=(v3 a)
{
	*this = *this + a;
	return(*this);
}

struct v4
{
	union
	{
		struct
		{
			float x, y, z, w;
		};
		float E[4];
	};
};
struct uv4
{
	union
	{
		struct
		{
			uint32_t x, y, z, w;
		};
		uint32_t E[4];
	};
};
union mat3
{
	float element[3][3];
	v3 column[3];
};
union mat4
{
	float element[4][4];
	v4 column[4];
	float m[16];
};

inline float clamp(float v, float min, float max)
{
	return (v < min) ? min : (v > max) ? max : v;
}
inline float DegToRad(float degrees)
{
	return (degrees * (PI / 180.0f));
}
inline float InvSqrtF(float Float)
{
	float result;
	result = 1.0f / sqrtf(Float);
	return result;
}

inline float v2_distance(v2 left, v2 right)
{
	float result = 0.0f;

	float dx = left.x - right.x;
	float dy = left.y - right.y;
	result = sqrtf(dx * dx + dy * dy);

	return result;
}
inline v2 v2_normalize(v2 vector)
{
	v2 result = vector;

	float length = sqrtf(vector.x * vector.x + vector.y * vector.y);
	if(length != 0.0f)
	{
		float ilength = 1.0f / length;

		result.x *= ilength;
		result.y *= ilength;
	}

	return result;
}

inline float v3_distance(v3 left, v3 right)
{
	float result = 0.0f;

	float dx = left.x - right.x;
	float dy = left.y - right.y;
	float dz = left.z - right.z;
	result = sqrtf(dx * dx + dy * dy + dz * dz);

	return result;
}
inline float v3_dot(v3 left, v3 right)
{
	return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}
inline v3 v3_cross(v3 left, v3 right)
{
	v3 result;

	result.x = (left.y * right.z) - (left.z * right.y);
	result.y = (left.z * right.x) - (left.x * right.z);
	result.z = (left.x * right.y) - (left.y * right.x);

	return result;
}
inline v3 v3_normalize(v3 vector)
{
	return (InvSqrtF(v3_dot(vector, vector)) * vector);
}
inline v3 v3_lerp(v3 left, v3 right, float amount)
{
	v3 result = {};

	result.x = left.x + amount * (right.x - left.x);
	result.y = left.y + amount * (right.y - left.y);
	result.z = left.z + amount * (right.z - left.z);

	return result;
}

inline v3 v3_divide(v3 left, v3 right)
{
	v3 result = {left.x / right.x, left.y / right.y, left.z / right.z};

	return result;
}
inline mat3 mat3_transpose(mat3 matrix)
{
	mat3 result = matrix;

	result.element[0][1] = matrix.element[1][0];
	result.element[0][2] = matrix.element[2][0];
	result.element[1][0] = matrix.element[0][1];
	result.element[1][2] = matrix.element[2][1];
	result.element[2][1] = matrix.element[1][2];
	result.element[2][0] = matrix.element[0][2];

	return result;
}

inline v4   mat4_v4_linear_combine(v4 vector, mat4 matrix)
{
	v4 result = {};

	result.x = vector.x * matrix.element[0][0];
	result.y = vector.x * matrix.element[0][1];
	result.z = vector.x * matrix.element[0][2];
	result.w = vector.x * matrix.element[0][3];

	result.x += vector.y * matrix.element[1][0];
	result.y += vector.y * matrix.element[1][1];
	result.z += vector.y * matrix.element[1][2];
	result.w += vector.y * matrix.element[1][3];

	result.x += vector.z * matrix.element[2][0];
	result.y += vector.z * matrix.element[2][1];
	result.z += vector.z * matrix.element[2][2];
	result.w += vector.z * matrix.element[2][3];

	result.x += vector.w * matrix.element[3][0];
	result.y += vector.w * matrix.element[3][1];
	result.z += vector.w * matrix.element[3][2];
	result.w += vector.w * matrix.element[3][3];

	return result;
}
inline mat4 mat4_diagonal(float diagonal)
{
	mat4 result = {};

	result.element[0][0] = diagonal;
	result.element[1][1] = diagonal;
	result.element[2][2] = diagonal;
	result.element[3][3] = diagonal;

	return result;
}
inline mat4 mat4_rotate_RH(float angle, v3 axis)
{
	mat4 result = mat4_diagonal(1.0f);

	axis = v3_normalize(axis);

	float SinTheta = sinf(angle);
	float CosTheta = cosf(angle);
	float CosValue = 1.0f - CosTheta;

	result.element[0][0] = (axis.x * axis.x) + CosTheta;
	result.element[0][1] = (axis.x * axis.y * CosValue) + (axis.z * SinTheta);
	result.element[0][2] = (axis.x * axis.z * CosValue) - (axis.y * SinTheta);

	result.element[1][0] = (axis.y * axis.x * CosValue) - (axis.z * SinTheta);
	result.element[1][1] = (axis.y * axis.y * CosValue) + CosTheta;
	result.element[1][2] = (axis.y * axis.z * CosValue) + (axis.x * SinTheta);

	result.element[2][0] = (axis.z * axis.x * CosValue) + (axis.y * SinTheta);
	result.element[2][1] = (axis.z * axis.y * CosValue) - (axis.x * SinTheta);
	result.element[2][2] = (axis.z * axis.z * CosValue) + CosTheta;

	return result;
}
inline mat4 mat4_translate(v3 vector)
{
	mat4 result = mat4_diagonal(1.0f);

	result.element[3][0] = vector.x;
	result.element[3][1] = vector.y;
	result.element[3][2] = vector.z;

	return result;
} 
inline mat4 mat4_scale(v3 vector)
{
	mat4 result = mat4_diagonal(1.0f);

	result.element[0][0] = vector.x;
	result.element[1][1] = vector.y;
	result.element[2][2] = vector.z;
	
	return result;
}
inline mat4 mat4_multiply(mat4 left, mat4 right)
{
	mat4 result = {};

	result.column[0] = mat4_v4_linear_combine(right.column[0], left);
	result.column[1] = mat4_v4_linear_combine(right.column[1], left);
	result.column[2] = mat4_v4_linear_combine(right.column[2], left);
	result.column[3] = mat4_v4_linear_combine(right.column[3], left);

	return result;
}
inline mat4 mat4_perspective(float FOV, float aspect_ratio, float Near)
{
	mat4 result = {}; // Initialize all elements to 0.0f safely

	// 1. Calculate focal scaling based on the field of view
	float cotangent = 1.0f / tanf(FOV / 2.0f);

	// Horizontal scale (Column 0, Row 0)
	result.element[0][0] = cotangent / aspect_ratio;

	// Vertical scale (Column 1, Row 1)
	result.element[1][1] = cotangent;

	// 2. Setup the perspective divide multiplier 
	// This maps the incoming Z coordinate straight into the W component
	result.element[2][3] = -1.0f;

	// 3. Configure the infinite far plane depth coefficients
	// Column 2, Row 2 maps the structural scaling of the Z-axis clip space
	result.element[2][2] = -1.0f;

	// Column 3, Row 2 holds the linear translation scale offset for the near plane
	result.element[3][2] = -Near;

	// 4. Ensure the homogeneous coordinate scale constant stays grounded
	result.element[3][3] = 0.0f;

	return result;
}
inline mat4 mat4_look_at(v3 eye, v3  center, v3  up)
{
	mat4 result = {}; 

	v3 F = v3_normalize(center - eye);

	v3 S = v3_normalize(v3_cross(F, up));
	v3 U = v3_cross(S, F);

	// Column 0 (S)
	result.element[0][0] = S.x;
	result.element[0][1] = U.x;
	result.element[0][2] = -F.x;
	result.element[0][3] = 0.0f;

	// Column 1 (U)
	result.element[1][0] = S.y;
	result.element[1][1] = U.y;
	result.element[1][2] = -F.y;
	result.element[1][3] = 0.0f;

	// Column 2 (-F)
	result.element[2][0] = S.z;
	result.element[2][1] = U.z;
	result.element[2][2] = -F.z;
	result.element[2][3] = 0.0f;

	// Column 3 (Translation)
	result.element[3][0] = -v3_dot(S, eye);
	result.element[3][1] = -v3_dot(U, eye);
	result.element[3][2] = v3_dot(F, eye);
	result.element[3][3] = 1.0f;

	return result;
}

inline mat4 mat4_inv_perspective(mat4 matrix)
{
	mat4 result = {}; // Initialize all elements to 0.0f safely

	// 1. Invert the horizontal and vertical focal scaling factors
	result.element[0][0] = (matrix.element[0][0] != 0.0f) ? 1.0f / matrix.element[0][0] : 0.0f;
	result.element[1][1] = (matrix.element[1][1] != 0.0f) ? 1.0f / matrix.element[1][1] : 0.0f;

	// 2. Clear the standard forward depth-scaling slot
	result.element[2][2] = 0.0f;

	// 3. Reconstruct the depth-to-W linear inverse mapping terms
	// This cleanly flips the perspective divide row back into a linear Z sequence
	result.element[2][3] = (matrix.element[3][2] != 0.0f) ? 1.0f / matrix.element[3][2] : 0.0f;
	result.element[3][2] = (matrix.element[2][3] != 0.0f) ? 1.0f / matrix.element[2][3] : 0.0f;

	// 4. Calculate the true depth offset multiplier matching your Z-clipping configuration
	result.element[3][3] = -(matrix.element[2][2] / (matrix.element[2][3] * matrix.element[3][2]));

	return result;
}
inline mat4 mat4_inv_lookat(mat4 matrix)
{
	mat4 Result = {};

	// 1. Extract the 3x3 rotation columns from the look-at matrix
	mat3 rotation = { 0 };
	rotation.column[0] = { matrix.column[0].x, matrix.column[0].y, matrix.column[0].z };
	rotation.column[1] = { matrix.column[1].x, matrix.column[1].y, matrix.column[1].z };
	rotation.column[2] = { matrix.column[2].x, matrix.column[2].y, matrix.column[2].z };

	// Inverse of a pure rotation matrix is its transpose
	rotation = mat3_transpose(rotation);

	// 2. Assign the inverted orientation basis vectors back to the Result columns
	Result.column[0] = { rotation.column[0].x, rotation.column[0].y, rotation.column[0].z, 0.0f };
	Result.column[1] = { rotation.column[1].x, rotation.column[1].y, rotation.column[1].z, 0.0f };
	Result.column[2] = { rotation.column[2].x, rotation.column[2].y, rotation.column[2].z, 0.0f };

	// 3. Isolate the look-at matrix translation factors from Column 3
	float tx = matrix.element[3][0];
	float ty = matrix.element[3][1];
	float tz = matrix.element[3][2];

	// 4. Calculate the true world-space position using vector dot products
	// This maps your eye position back correctly into a Z-up orientation
	Result.element[3][0] = -(tx * rotation.element[0][0] + ty * rotation.element[1][0] + tz * rotation.element[2][0]);
	Result.element[3][1] = -(tx * rotation.element[0][1] + ty * rotation.element[1][1] + tz * rotation.element[2][1]);
	Result.element[3][2] = -(tx * rotation.element[0][2] + ty * rotation.element[1][2] + tz * rotation.element[2][2]);
	Result.element[3][3] = 1.0f;

	return Result;
}
	