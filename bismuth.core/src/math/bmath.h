#pragma once

#include "defines.h"
#include "math_types.h"
#include "memory/bmemory.h"

#define B_PI 3.14159265358979323846f
#define B_2PI (2.0f * B_PI)
#define B_4PI (4.0f * B_PI)
#define B_HALF_PI (0.5f * B_PI)
#define B_QUARTER_PI (0.25f * B_PI)
#define B_ONE_OVER_PI (1.0f / B_PI)
#define B_ONE_OVER_TWO_PI (1.0f / B_2PI)
#define B_SQRT_TWO 1.41421356237309504880f
#define B_SQRT_THREE 1.73205080756887729352f
#define B_SQRT_ONE_OVER_TWO 0.70710678118654752440f
#define B_SQRT_ONE_OVER_THREE 0.57735026918962576450f
#define B_DEG2RAD_MULTIPLIER (B_PI / 180.0f)
#define B_RAD2DEG_MULTIPLIER (180.0f / B_PI)

// Multiplier to convert seconds to microseconds
#define B_SEC_TO_US_MULTIPLIER (1000.0f * 1000.0f)

// Multiplier to convert seconds to milliseconds
#define B_SEC_TO_MS_MULTIPLIER 1000.0f

// Multiplier to convert milliseconds to seconds
#define B_MS_TO_SEC_MULTIPLIER 0.001f

// Huge number that should be larger than any valid number used
#define B_INFINITY (1e30f * 1e30f)

// Smallest positive number where 1.0 + FLOAT_EPSILON != 0
#define B_FLOAT_EPSILON 1.192092896e-07f

#define B_FLOAT_MIN -3.40282e+38F
#define B_FLOAT_MAX 3.40282e+38F

// -------- General math functions --------
BINLINE void bswapf(f32* a, f32* b)
{
    f32 temp = *a;
    *a = *b;
    *b = temp;
}

#define BSWAP(type, a, b) \
    {                     \
        type temp = a;    \
        a = b;            \
        b = temp;         \
    }

/** @brief Returns 0.0f if x == 0.0f, -1.0f if negative, otherwise 1.0f */
BINLINE f32 bsign(f32 x)
{
    return x == 0.0f ? 0.0f : x < 0.0f ? -1.0f : 1.0f;
}

/** @brief Compares x to edge, returning 0 if x < edge; otherwise 1.0f */
BINLINE f32 bstep(f32 edge, f32 x)
{
    return x < edge ? 0.0f : 1.0f;
}

BAPI f32 bsin(f32 x);
BAPI f32 bcos(f32 x);
BAPI f32 btan(f32 x);
BAPI f32 batan(f32 x);
BAPI f32 batan2(f32 x, f32 y);
BAPI f32 basin(f32 x);
BAPI f32 bacos(f32 x);
BAPI f32 bsqrt(f32 x);
BAPI f32 babs(f32 x);
BAPI f32 bfloor(f32 x);
BAPI f32 bceil(f32 x);
BAPI f32 blog(f32 x);
BAPI f32 blog2(f32 x);
BAPI f32 bpow(f32 x, f32 y);
BAPI f32 bexp(f32 x);

BINLINE f32 blerp(f32 a, f32 b, f32 t)
{
    return a + t * (b - a);
}

/**
 * Indicates if the value is a power of 2. 0 is considered _not_ a power of 2.
 * @param value The value to be interpreted.
 * @returns True if a power of 2, otherwise false.
 */
BINLINE b8 is_power_of_2(u64 value)
{
    return (value != 0) && ((value & (value - 1)) == 0);
}

BAPI i32 brandom(void);
BAPI i32 brandom_in_range(i32 min, i32 max);

BAPI u64 brandom_u64(void);

BAPI f32 bfrandom(void);
BAPI f32 bfrandom_in_range(f32 min, f32 max);

/**
 * @brief Perform Hermite interpolation between two values
 * 
 * @param edge_0 The lower edge of the Hermite function
 * @param edge_1 The upper edge of the Hermite function
 * @param x The value to interpolate
 * @return The interpolated value
 */
BINLINE f32 bsmoothstep(f32 edge_0, f32 edge_1, f32 x)
{
    f32 t = BCLAMP((x - edge_0) / (edge_1 - edge_0), 0.0f, 1.0f);
    return t * t * (3.0 - 2.0 * t);
}

/**
 * @brief Returns the attenuation of x based off distance from the midpoint of min and max
 *
 * @param min The minimum value
 * @param max The maximum value
 * @param x The value to attenuate
 * @return The attenuation of x based on distance of the midpoint of min and max
 */
BAPI f32 battenuation_min_max(f32 min, f32 max, f32 x);

/**
 * @brief Compares two floats and returns true if both are less than B_FLOAT_EPSILON; otherwise false
 */
BINLINE b8 bfloat_compare(f32 f_0, f32 f_1)
{
    return babs(f_0 - f_1) < B_FLOAT_EPSILON;
}

/**
 * @brief Converts provided degrees to radians
 *
 * @param degrees The degrees to be converted
 * @return The amount in radians
 */
BINLINE f32 deg_to_rad(f32 degrees)
{
    return degrees * B_DEG2RAD_MULTIPLIER;
}

/**
 * @brief Converts provided radians to degrees
 *
 * @param radians The radians to be converted
 * @return The amount in degrees
 */
BINLINE f32 rad_to_deg(f32 radians)
{
    return radians * B_RAD2DEG_MULTIPLIER;
}

/**
 * @brief Converts value from the "old" range to the "new" range
 *
 * @param value The value to be converted
 * @param from_min The minimum value from the old range
 * @param from_max The maximum value from the old range
 * @param to_min The minimum value from the new range
 * @param to_max The maximum value from the new range
 * @return The converted value
 */
BINLINE f32 range_convert_f32(f32 value, f32 old_min, f32 old_max, f32 new_min, f32 new_max)
{
    return (((value - old_min) * (new_max - new_min)) / (old_max - old_min)) + new_min;
}

/**
 * @brief Converts rgb int values [0-255] to a single 32-bit integer
 *
 * @param r The red value [0-255]
 * @param g The green value [0-255]
 * @param b The blue value [0-255]
 * @param out_u32 A pointer to hold the resulting integer
 */
BINLINE void rgbu_to_u32(u32 r, u32 g, u32 b, u32* out_u32)
{
    *out_u32 = (((r & 0x0FF) << 16) | ((g & 0x0FF) << 8) | (b & 0x0FF));
}

/**
 * @brief Converts the given 32-bit integer to rgb values [0-255]
 *
 * @param rgbu The integer holding a rgb value
 * @param out_r A pointer to hold the red value
 * @param out_g A pointer to hold the green value
 * @param out_b A pointer to hold the blue value
 */
BINLINE void u32_to_rgb(u32 rgbu, u32* out_r, u32* out_g, u32* out_b)
{
    *out_r = (rgbu >> 16) & 0x0FF;
    *out_g = (rgbu >> 8) & 0x0FF;
    *out_b = (rgbu) & 0x0FF;
}

/**
 * @brief Converts rgb integer values [0-255] to a vec3 of floating-point values [0.0-1.0]
 *
 * @param r The red value [0-255]
 * @param g The green value [0-255]
 * @param b The blue value [0-255]
 * @param out_v A pointer to hold the vector of floating-point values
 */
BINLINE void rgb_u32_to_vec3(u32 r, u32 g, u32 b, vec3* out_v)
{
    out_v->r = r / 255.0f;
    out_v->g = g / 255.0f;
    out_v->b = b / 255.0f;
}

/**
 * @brief Converts a vec3 of rgbvalues [0.0-1.0] to integer rgb values [0-255]
 *
 * @param v The vector of rgb values [0.0-1.0] to be converted
 * @param out_r A pointer to hold the red value
 * @param out_g A pointer to hold the green value
 * @param out_b A pointer to hold the blue value
 */
BINLINE void vec3_to_rgb_u32(vec3 v, u32* out_r, u32* out_g, u32* out_b)
{
    *out_r = v.r * 255;
    *out_g = v.g * 255;
    *out_b = v.b * 255;
}

// --------------------------------------------------------------------------------
// ----------------------------------- Vector 2 -----------------------------------
// --------------------------------------------------------------------------------
/**
 * @brief Creates and returns a new 2-element vector using the supplied values.
 * 
 * @param x The x value.
 * @param y The y value.
 * @return A new 2-element vector.
 */
BINLINE vec2 vec2_create(f32 x, f32 y)
{
    return (vec2){.x = x, .y = y};
}

/**
 * @brief Creates and returns a 2-component vector with all components set to 0.0f.
 */
BINLINE vec2 vec2_zero(void)
{
    return (vec2){0.0f, 0.0f};
}

/**
 * @brief Creates and returns a 2-component vector with all components set to 1.0f.
 */
BINLINE vec2 vec2_one(void)
{
    return (vec2){1.0f, 1.0f};
}

/**
 * @brief Creates and returns a 2-component vector pointing up (0, 1).
 */
BINLINE vec2 vec2_up(void)
{
    return (vec2){0.0f, 1.0f};
}

/**
 * @brief Creates and returns a 2-component vector pointing down (0, -1).
 */
BINLINE vec2 vec2_down(void)
{
    return (vec2){0.0f, -1.0f};
}

/**
 * @brief Creates and returns a 2-component vector pointing left (-1, 0).
 */
BINLINE vec2 vec2_left(void)
{
    return (vec2){-1.0f, 0.0f};
}

/**
 * @brief Creates and returns a 2-component vector pointing right (1, 0).
 */
BINLINE vec2 vec2_right(void)
{
    return (vec2){1.0f, 0.0f};
}

/**
 * @brief Adds vector_1 to vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec2 vec2_add(vec2 vector_0, vec2 vector_1)
{
    return (vec2){
        vector_0.x + vector_1.x,
        vector_0.y + vector_1.y};
}

/**
 * @brief Subtracts vector_1 from vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec2 vec2_sub(vec2 vector_0, vec2 vector_1)
{
    return (vec2){
        vector_0.x - vector_1.x,
        vector_0.y - vector_1.y};
}

/**
 * @brief Multiplies vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec2 vec2_mul(vec2 vector_0, vec2 vector_1)
{
    return (vec2){
        vector_0.x * vector_1.x,
        vector_0.y * vector_1.y};
}

/**
 * @brief Multiplies all elements of vector_0 by scalar and returns a copy of the result
 *
 * @param vector_0 The vector to be multiplied
 * @param scalar The scalar value
 * @return A copy of the resulting vector
 */
BINLINE vec2 vec2_mul_scalar(vec2 vector_0, f32 scalar)
{
    return (vec2){vector_0.x * scalar, vector_0.y * scalar};
}

/**
 * @brief Multiplies vector_0 by vector_1, then adds the result to vector_2
 *
 * @param vector_0 First vector
 * @param vector_1 Second vector
 * @param vector_2 Third vector
 * @return Resulting vector
 */
BINLINE vec2 vec2_mul_add(vec2 vector_0, vec2 vector_1, vec2 vector_2)
{
    return (vec2){vector_0.x * vector_1.x + vector_2.x, vector_0.y * vector_1.y + vector_2.y};
}

/**
 * @brief Multiplies vector_0 by scalar, then adds to vector_1
 *
 * @param vector_0 The first vector
 * @param vector_1 The second vector
 * @param scalar The scalar value
 * @return The resulting vector
 */
BINLINE vec2 vec2_mul_add_scalar(vec2 vector_0, f32 scalar, vec2 vector_1)
{
    return (vec2){vector_0.x * scalar + vector_1.x, vector_0.y * scalar + vector_1.y};
}

/**
 * @brief Divides vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec2 vec2_div(vec2 vector_0, vec2 vector_1)
{
    return (vec2){
        vector_0.x / vector_1.x,
        vector_0.y / vector_1.y};
}

/**
 * @brief Returns the squared length of the provided vector.
 * 
 * @param vector The vector to retrieve the squared length of.
 * @return The squared length.
 */
BINLINE f32 vec2_length_squared(vec2 vector)
{
    return vector.x * vector.x + vector.y * vector.y;
}

/**
 * @brief Returns the length of the provided vector.
 * 
 * @param vector The vector to retrieve the length of.
 * @return The length.
 */
BAPI f32 vec2_length(vec2 vector);

/**
 * @brief Normalizes the provided vector in place to a unit vector.
 * 
 * @param vector A pointer to the vector to be normalized.
 */
BAPI void vec2_normalize(vec2* vector);

/**
 * @brief Returns a normalized copy of the supplied vector.
 * 
 * @param vector The vector to be normalized.
 * @return A normalized copy of the supplied vector 
 */
BAPI vec2 vec2_normalized(vec2 vector);

/**
 * @brief Compares all elements of vector_0 and vector_1 and ensures the difference is less than tolerance
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @param tolerance The difference tolerance. Typically B_FLOAT_EPSILON or similar.
 * @return True if within tolerance; otherwise false. 
 */
BAPI b8 vec2_compare(vec2 vector_0, vec2 vector_1, f32 tolerance);

/**
 * @brief Returns the distance between vector_0 and vector_1.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The distance between vector_0 and vector_1.
 */
BAPI f32 vec2_distance(vec2 vector_0, vec2 vector_1);

/* @brief Returns the squared distance between vector_0 and vector_1 */
BAPI f32 vec2_distance_squared(vec2 vector_0, vec2 vector_1);

// --------------------------------------------------------------------------------
// ----------------------------------- Vector 3 -----------------------------------
// --------------------------------------------------------------------------------
/**
 * @brief Creates and returns a new 3-element vector using the supplied values.
 * 
 * @param x The x value.
 * @param y The y value.
 * @param z The z value.
 * @return A new 3-element vector.
 */
BINLINE vec3 vec3_create(f32 x, f32 y, f32 z)
{
    return (vec3){x, y, z};
}

/**
 * @brief Returns a new vec3 containing the x, y and z components of the 
 * supplied vec4, essentially dropping the w component.
 * 
 * @param vector The 4-component vector to extract from.
 * @return A new vec3 
 */
BINLINE vec3 vec3_from_vec4(vec4 vector)
{
    return (vec3){vector.x, vector.y, vector.z};
}

/**
 * @brief Returns a new vec3 containing the x and y components of the supplied vec2, with a z component specified
 *
 * @param vector The 2-component vector to extract from
 * @param z The value to use for the z element
 * @return A new vec3
 */
BINLINE vec3 vec3_from_vec2(vec2 vector, f32 z)
{
    return (vec3){vector.x, vector.y, z};
}

/**
 * @brief Returns a new vec4 using vector as the x, y and z components and w for w.
 * 
 * @param vector The 3-component vector.
 * @param w The w component.
 * @return A new vec4 
 */
BINLINE vec4 vec3_to_vec4(vec3 vector, f32 w)
{
    return (vec4){vector.x, vector.y, vector.z, w};
}

/**
 * @brief Creates and returns a 3-component vector with all components set to 0.0f.
 */
BINLINE vec3 vec3_zero(void)
{
    return (vec3){0.0f, 0.0f, 0.0f};
}

/**
 * @brief Creates and returns a 3-component vector with all components set to 1.0f.
 */
BINLINE vec3 vec3_one(void)
{
    return (vec3){1.0f, 1.0f, 1.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing up (0, 1, 0).
 */
BINLINE vec3 vec3_up(void)
{
    return (vec3){0.0f, 1.0f, 0.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing down (0, -1, 0).
 */
BINLINE vec3 vec3_down(void)
{
    return (vec3){0.0f, -1.0f, 0.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing left (-1, 0, 0).
 */
BINLINE vec3 vec3_left(void)
{
    return (vec3){-1.0f, 0.0f, 0.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing right (1, 0, 0).
 */
BINLINE vec3 vec3_right(void)
{
    return (vec3){1.0f, 0.0f, 0.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing forward (0, 0, -1).
 */
BINLINE vec3 vec3_forward(void)
{
    return (vec3){0.0f, 0.0f, -1.0f};
}

/**
 * @brief Creates and returns a 3-component vector pointing backward (0, 0, 1).
 */
BINLINE vec3 vec3_backward(void)
{
    return (vec3){0.0f, 0.0f, 1.0f};
}

/**
 * @brief Adds vector_1 to vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec3 vec3_add(vec3 vector_0, vec3 vector_1)
{
    return (vec3){vector_0.x + vector_1.x, vector_0.y + vector_1.y, vector_0.z + vector_1.z};
}

/**
 * @brief Subtracts vector_1 from vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec3 vec3_sub(vec3 vector_0, vec3 vector_1)
{
    return (vec3){vector_0.x - vector_1.x, vector_0.y - vector_1.y, vector_0.z - vector_1.z};
}

/**
 * @brief Multiplies vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec3 vec3_mul(vec3 vector_0, vec3 vector_1)
{
    return (vec3){vector_0.x * vector_1.x, vector_0.y * vector_1.y, vector_0.z * vector_1.z};
}

/**
 * @brief Multiplies all elements of vector_0 by scalar and returns a copy of the result.
 * 
 * @param vector_0 The vector to be multiplied.
 * @param scalar The scalar value.
 * @return A copy of the resulting vector.
 */
BINLINE vec3 vec3_mul_scalar(vec3 vector_0, f32 scalar)
{
    return (vec3){
        vector_0.x * scalar,
        vector_0.y * scalar,
        vector_0.z * scalar};
}

/**
 * @brief Multiplies vector_0 by vector_1, then adds result to vector_2
 *
 * @param vector_0 First vector
 * @param vector_1 Second vector
 * @param vector_2 Third vector
 * @return Resulting vector
 */
BINLINE vec3 vec3_mul_add(vec3 vector_0, vec3 vector_1, vec3 vector_2)
{
    return (vec3){
        vector_0.x * vector_1.x + vector_2.x,
        vector_0.y * vector_1.y + vector_2.y,
        vector_0.z * vector_1.z + vector_2.z};
}

/**
 * @brief Multiplies vector_0 by scalar, then add that result to vector_1
 *
 * @param vector_0 The first vector
 * @param scalar The scalar value
 * @param vector_1 The second vector
 * @return The resulting vector
 */
BINLINE vec3 vec3_mul_add_scalar(vec3 vector_0, f32 scalar, vec3 vector_1)
{
    return (vec3){
        vector_0.x * scalar + vector_1.x,
        vector_0.y * scalar + vector_1.y,
        vector_0.z * scalar + vector_1.z};
}

/**
 * @brief Divides vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BINLINE vec3 vec3_div(vec3 vector_0, vec3 vector_1)
{
    return (vec3){
        vector_0.x / vector_1.x,
        vector_0.y / vector_1.y,
        vector_0.z / vector_1.z};
}

BINLINE vec3 vec3_div_scalar(vec3 vector_0, f32 scalar)
{
    vec3 result;
    for (u64 i = 0; i < 3; ++i)
        result.elements[i] = vector_0.elements[i] / scalar;

    return result;
}

/**
 * @brief Returns the squared length of the provided vector.
 * 
 * @param vector The vector to retrieve the squared length of.
 * @return The squared length.
 */
BINLINE f32 vec3_length_squared(vec3 vector)
{
    return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

/**
 * @brief Returns the length of the provided vector.
 * 
 * @param vector The vector to retrieve the length of.
 * @return The length.
 */
BAPI f32 vec3_length(vec3 vector);

/**
 * @brief Normalizes the provided vector in place to a unit vector.
 * 
 * @param vector A pointer to the vector to be normalized.
 */
BAPI void vec3_normalize(vec3* vector);

/**
 * @brief Returns a normalized copy of the supplied vector.
 * 
 * @param vector The vector to be normalized.
 * @return A normalized copy of the supplied vector 
 */
BAPI vec3 vec3_normalized(vec3 vector);

/**
 * @brief Returns the dot product between the provided vectors. Typically used
 * to calculate the difference in direction.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The dot product. 
 */
BAPI f32 vec3_dot(vec3 vector_0, vec3 vector_1);

/**
 * @brief Calculates and returns the cross product of the supplied vectors.
 * The cross product is a new vector which is orthoganal to both provided vectors.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The cross product. 
 */
BAPI vec3 vec3_cross(vec3 vector_0, vec3 vector_1);

/**
 * @brief Returns the midpoint between two vectors
 *
 * @param vector_0 The first vector
 * @param vector_1 The second vector
 * @return The midpoint vector
 */
BINLINE vec3 vec3_mid(vec3 vector_0, vec3 vector_1)
{
    return (vec3){
        (vector_0.x - vector_1.x) * 0.5f,
        (vector_0.y - vector_1.y) * 0.5f,
        (vector_0.z - vector_1.z) * 0.5f};
}

/**
 * @brief Linearly interpolates between the first and second vectors based on parameter t
 *
 * Performs linear interpolation between vector_0 and vector_1 based on parameter t, where:
 * - When t = 0.0, returns vector_0
 * - When t = 1.0, returns vector_1
 * - When t = 0.5, returns the midpoint of vector_0 and vector_1
 * - Values of t outside [0,1] extrapolate beyond the endpoints
 *
 * @param vector_0 The first vector
 * @param vector_1 The second vector
 * @param t The interpolation factor (typically between 0 and 1)
 * @return The interpolated vector
 */
BINLINE vec3 vec3_lerp(vec3 vector_0, vec3 vector_1, f32 t)
{
    return (vec3){
        vector_0.x + (vector_1.x - vector_0.x) * t,
        vector_0.y + (vector_1.y - vector_0.y) * t,
        vector_0.z + (vector_1.z - vector_0.z) * t};
}

/**
 * @brief Compares all elements of vector_0 and vector_1 and ensures the difference
 * is less than tolerance.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @param tolerance The difference tolerance. Typically B_FLOAT_EPSILON or similar.
 * @return True if within tolerance; otherwise false. 
 */
BAPI b8 vec3_compare(vec3 vector_0, vec3 vector_1, f32 tolerance);

/**
 * @brief Returns the distance between vector_0 and vector_1
 * 
 * @param vector_0 The first vector
 * @param vector_1 The second vector
 * @return The distance between vector_0 and vector_1
 */
BAPI f32 vec3_distance(vec3 vector_0, vec3 vector_1);

/**
 * @brief Returns the squared distance between vector_0 and vector_1.
 * Less intensive than calling the non-squared version due to sqrt
 *
 * @param vector_0 The first vector
 * @param vector_1 The second vector
 * @return The squared distance between vector_0 and vector_1
 */
BAPI f32 vec3_distance_squared(vec3 vector_0, vec3 vector_1);

/**
 * @brief Projects v_0 onto v_1
 *
 * @param v_0 The first vector
 * @param v_1 The second vector
 * @return The projected vector
 */
BAPI vec3 vec3_project(vec3 v_0, vec3 v_1);

BAPI void vec3_project_points_onto_axis(vec3* points, u32 count, vec3 axis, f32* out_min, f32* out_max);

/**
 * @brief Reflects vector v along the given normal using r = v - 2(v·n)n
 *
 * @param v The vector to be reflected
 * @param normal The normal to reflect along
 * @returns The reflected vector
 */
BAPI vec3 vec3_reflect(vec3 v, vec3 normal);

/**
 * @brief Transform v by m. NOTE: It is assumed by this function that the vector v is a point, not a direction.
 *
 * @param v The vector to transform.
 * @param w Pass 1.0f for a point, or 0.0f for a direction
 * @param m The matrix to transform by.
 * @return A transformed copy of v.
 */
BAPI vec3 vec3_transform(vec3 v, f32 w, mat4 m);

/**
 * @brief Calculates the shortest Euclidean distance from a point to a line in 3D space
 *
 * This function uses the cross product method to efficiently calculate the shortest
 * distance between a point and a line. The line is defined by a starting point and
 * a direction vector. The calculation is based on the geometric property that the
 * magnitude of the cross product of two vectors equals the area of the parallelogram
 * they form, divided by the length of one vector
 *
 * @note If the line direction vector has zero length, returns the distance from the point to the line start point
 * @warning The line direction vector should not be zero-length for proper line distance calculation
 *
 * @param point The point to calculate distance from
 * @param line_start Starting point of the line
 * @param line_direction Direction vector of the line
 *
 * @return The shortest Euclidean distance from the point to the line
 */
BAPI f32 vec3_distance_to_line(vec3 point, vec3 line_start, vec3 line_direction);

BAPI vec3 vec3_min(vec3 vector_0, vec3 vector_1);

BAPI vec3 vec3_max(vec3 vector_0, vec3 vector_1);

BAPI vec3 vec3_sign(vec3 v);

BAPI vec3 vec3_rotate(vec3 v, quat q);

// --------------------------------------------------------------------------------
// ----------------------------------- Vector 4 -----------------------------------
// --------------------------------------------------------------------------------
/**
 * @brief Creates and returns a new 4-element vector using the supplied values.
 * 
 * @param x The x value.
 * @param y The y value.
 * @param z The z value.
 * @param w The w value.
 * @return A new 4-element vector.
 */
BAPI vec4 vec4_create(f32 x, f32 y, f32 z, f32 w);

/**
 * @brief Returns a new vec3 containing the x, y and z components of the 
 * supplied vec4, essentially dropping the w component.
 * 
 * @param vector The 4-component vector to extract from.
 * @return A new vec3 
 */
BAPI vec3 vec4_to_vec3(vec4 vector);

/**
 * @brief Returns a new vec4 using vector as the x, y and z components and w for w.
 * 
 * @param vector The 3-component vector.
 * @param w The w component.
 * @return A new vec4 
 */
BAPI vec4 vec4_from_vec3(vec3 vector, f32 w);

/**
 * @brief Creates and returns a 4-component vector with all components set to 0.0f.
 */
BINLINE vec4 vec4_zero(void)
{
    return (vec4){0.0f, 0.0f, 0.0f, 0.0f};
}

/**
 * @brief Creates and returns a 4-component vector with all components set to 1.0f.
 */
BINLINE vec4 vec4_one(void)
{
    return (vec4){1.0f, 1.0f, 1.0f, 1.0f};
}

/**
 * @brief Adds vector_1 to vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BAPI vec4 vec4_add(vec4 vector_0, vec4 vector_1);

/**
 * @brief Subtracts vector_1 from vector_0 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BAPI vec4 vec4_sub(vec4 vector_0, vec4 vector_1);

/**
 * @brief Multiplies vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BAPI vec4 vec4_mul(vec4 vector_0, vec4 vector_1);

/**
 * @brief Multiplies all elements of vector_0 by scalar and returns a copy of the result
 *
 * @param vector_0 The vector to be multiplied
 * @param scalar Scalar value
 * @return A copy of the resulting vector
 */
BAPI vec4 vec4_mul_scalar(vec4 vector_0, f32 scalar);

/**
 * @brief Multiplies vector_0 by vector_1, then adds the result to vector_2
 *
 * @param vector_0 First vector
 * @param vector_1 Second vector
 * @param vector_2 Third vector
 * @return Resulting vector
 */
BAPI vec4 vec4_mul_add(vec4 vector_0, vec4 vector_1, vec4 vector_2);

/**
 * @brief Divides vector_0 by vector_1 and returns a copy of the result.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @return The resulting vector. 
 */
BAPI vec4 vec4_div(vec4 vector_0, vec4 vector_1);

BAPI vec4 vec4_div_scalar(vec4 vector_0, f32 scalar);

/**
 * @brief Returns the squared length of the provided vector.
 * 
 * @param vector The vector to retrieve the squared length of.
 * @return The squared length.
 */
BAPI f32 vec4_length_squared(vec4 vector);

/**
 * @brief Returns the length of the provided vector.
 * 
 * @param vector The vector to retrieve the length of.
 * @return The length.
 */
BAPI f32 vec4_length(vec4 vector);

/**
 * @brief Normalizes the provided vector in place to a unit vector.
 * 
 * @param vector A pointer to the vector to be normalized.
 */
BAPI void vec4_normalize(vec4* vector);

/**
 * @brief Returns a normalized copy of the supplied vector.
 * 
 * @param vector The vector to be normalized.
 * @return A normalized copy of the supplied vector 
 */
BAPI vec4 vec4_normalized(vec4 vector);

/**
 * @brief Calculates the dot product using the elements of vec4s provided in split-out format
 *
 * @param a0 The first element of the a vector
 * @param a1 The second element of the a vector
 * @param a2 The third element of the a vector
 * @param a3 The fourth element of the a vector
 * @param b0 The first element of the b vector
 * @param b1 The second element of the b vector
 * @param b2 The third element of the b vector
 * @param b3 The fourth element of the b vector
 * @return The dot product of vectors and b
 */
BAPI f32 vec4_dot_f32(f32 a0, f32 a1, f32 a2, f32 a3, f32 b0, f32 b1, f32 b2, f32 b3);

/**
 * @brief Compares all elements of vector_0 and vector_1 and ensures the difference
 * is less than tolerance.
 * 
 * @param vector_0 The first vector.
 * @param vector_1 The second vector.
 * @param tolerance The difference tolerance. Typically B_FLOAT_EPSILON or similar.
 * @return True if within tolerance; otherwise false. 
 */
BAPI b8 vec4_compare(vec4 vector_0, vec4 vector_1, f32 tolerance);

/**
 * @brief Clamps the provided vector in-place to the given min/max values
 *
 * @param vector A pointer to the vector to be clamped.
 * @param min The minimum value.
 * @param max The maximum value.
 */
BAPI void vec4_clamp(vec4* vector, f32 min, f32 max);

/**
 * @brief Returns a clamped copy of the provided vector
 *
 * @param vector The vector to clamp.
 * @param min The minimum value.
 * @param max The maximum value.
 * @return A clamped copy of the provided vector.
 */
BAPI vec4 vec4_clamped(vec4 vector, f32 min, f32 max);

// --------------------------------------------------------------------------------
// ------------------------------- Mat4 (4x4 matrix) ------------------------------
// --------------------------------------------------------------------------------
/**
 * @brief Creates and returns an identity matrix:
 * 
 * {
 *   {1, 0, 0, 0},
 *   {0, 1, 0, 0},
 *   {0, 0, 1, 0},
 *   {0, 0, 0, 1}
 * }
 * 
 * @return A new identity matrix 
 */
BAPI mat4 mat4_identity(void);

/**
 * @brief Returns the result of multiplying matrix_0 and matrix_1.
 * 
 * @param matrix_0 The first matrix to be multiplied.
 * @param matrix_1 The second matrix to be multiplied.
 * @return The result of the matrix multiplication.
 */
BAPI mat4 mat4_mul(mat4 matrix_0, mat4 matrix_1);

/**
 * @brief Creates and returns an orthographic projection matrix. Typically used to render flat or 2D scenes
 * 
 * @param left The left side of the view frustum.
 * @param right The right side of the view frustum.
 * @param bottom The bottom side of the view frustum.
 * @param top The top side of the view frustum.
 * @param near_clip The near clipping plane distance.
 * @param far_clip The far clipping plane distance.
 * @return A new orthographic projection matrix. 
 */
BAPI mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near_clip, f32 far_clip);

/**
 * @brief Creates and returns a perspective matrix. Typically used to render 3d scenes
 * 
 * @param fov_radians The field of view in radians.
 * @param aspect_ratio The aspect ratio.
 * @param near_clip The near clipping plane distance.
 * @param far_clip The far clipping plane distance.
 * @return A new perspective matrix. 
 */
BAPI mat4 mat4_perspective(f32 fov_radians, f32 aspect_ratio, f32 near_clip, f32 far_clip);

/**
 * @brief Creates and returns a look-at matrix, or a matrix looking at target from the perspective of position
 * 
 * @param position The position of the matrix.
 * @param target The position to "look at".
 * @param up The up vector.
 * @return A matrix looking at target from the perspective of position. 
 */
BAPI mat4 mat4_look_at(vec3 position, vec3 target, vec3 up);

/**
 * @brief Returns a transposed copy of the provided matrix (rows->colums)
 * 
 * @param matrix The matrix to be transposed.
 * @return A transposed copy of of the provided matrix.
 */
BAPI mat4 mat4_transposed(mat4 matrix);

/**
 * @brief Calculates determinant of the given matrix
 *
 * @param matrix The matrix to calculate the determinant of
 * @return Determinant of the given matrix
 */
BAPI f32 mat4_determinant(mat4 matrix);

/**
 * @brief Creates and returns an inverse of the provided matrix.
 * 
 * @param matrix The matrix to be inverted.
 * @return A inverted copy of the provided matrix. 
 */
BAPI mat4 mat4_inverse(mat4 matrix);

/**
 * @brief Creates and returns a translation matrix from the given position
 *
 * @param position The position to be used to create the matrix
 * @return A newly created translation matrix
 */
BAPI mat4 mat4_translation(vec3 position);

/**
 * @brief Returns a scale matrix using the provided scale.
 * 
 * @param scale The 3-component scale.
 * @return A scale matrix.
 */
BAPI mat4 mat4_scale(vec3 scale);

/**
 * @brief Returns a matrix created from the provided translation, rotation and scale (TRS)
 *
 * @param t The position to be used to create the matrix.
 * @param r The quaternion rotation to be used to create the matrix.
 * @param s The 3-component scale to be used to create the matrix.
 * @return A matrix created in TRS order.
 */
BAPI mat4 mat4_from_translation_rotation_scale(vec3 t, quat r, vec3 s);

BAPI mat4 mat4_euler_x(f32 angle_radians);

BAPI mat4 mat4_euler_y(f32 angle_radians);

BAPI mat4 mat4_euler_z(f32 angle_radians);

BAPI mat4 mat4_euler_xyz(f32 x_radians, f32 y_radians, f32 z_radians);

/**
 * @brief Returns a forward vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_forward(mat4 matrix);

/**
 * @brief Returns a backward vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_backward(mat4 matrix);

/**
 * @brief Returns a upward vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_up(mat4 matrix);

/**
 * @brief Returns a downward vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_down(mat4 matrix);

/**
 * @brief Returns a left vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_left(mat4 matrix);

/**
 * @brief Returns a right vector relative to the provided matrix.
 * 
 * @param matrix The matrix from which to base the vector.
 * @return A 3-component directional vector.
 */
BAPI vec3 mat4_right(mat4 matrix);

/**
 * @brief Returns the position relative to the provided matrix.
 *
 * @param matrix A pointer to the matrix from which to base the vector.
 * @return A 3-component positional vector.
 */
BAPI vec3 mat4_position_get(const mat4* matrix);

/**
 * @brief Returns the quaternion relative to the provided matrix.
 *
 * @param matrix A pointer to the matrix from which to base the quaternion.
 * @return A quaternion.
 */
BAPI quat mat4_rotation_get(const mat4* matrix);

/**
 * @brief Returns the scale relative to the provided matrix.
 *
 * @param matrix A pointer to the matrix from which to base the vector.
 * @return A 3-component scale vector.
 */
BAPI vec3 mat4_scale_get(const mat4* matrix);

BAPI vec3 mat4_mul_vec3(mat4 m, vec3 v);

BAPI vec3 vec3_mul_mat4(vec3 v, mat4 m);

BAPI vec4 mat4_mul_vec4(mat4 m, vec4 v);

BAPI vec4 vec4_mul_mat4(vec4 v, mat4 m);

// --------------------------------------------------------------------------------
// ---------------------------------- Quaternion ----------------------------------
// --------------------------------------------------------------------------------
BAPI quat quat_identity(void);

BAPI quat quat_from_surface_normal(vec3 normal, vec3 reference_up);

BAPI f32 quat_normal(quat q);

BAPI quat quat_normalize(quat q);

BAPI quat quat_conjugate(quat q);

BAPI quat quat_inverse(quat q);

BAPI quat quat_mul(quat q_0, quat q_1);

BAPI f32 quat_dot(quat q_0, quat q_1);

BAPI mat4 quat_to_mat4(quat q);

// Calculates a rotation matrix based on the quaternion and the passed in center point.
BAPI mat4 quat_to_rotation_matrix(quat q, vec3 center);

BAPI quat quat_from_axis_angle(vec3 axis, f32 angle, b8 normalize);

BAPI quat quat_from_euler_radians(vec3 euler_rotation_radians);

BAPI vec3 quat_to_euler_radians(quat q);

BAPI vec3 quat_to_euler(quat q);

BAPI quat quat_from_direction(vec3 direction);

BAPI quat quat_lookat(vec3 from, vec3 to);

BAPI quat quat_slerp(quat q_0, quat q_1, f32 percentage);

// --------------------------------------------------------------------------------
// ------------------------------------ Plane3D -----------------------------------
// --------------------------------------------------------------------------------
BAPI plane_3d plane_3d_create(vec3 position, vec3 normal);

BAPI f32 plane_signed_distance(const plane_3d* p, const vec3* position);

BAPI b8 plane_intersects_sphere(const plane_3d* p, const vec3* center, f32 radius);

BAPI b8 plane_intersects_aabb(const plane_3d* p, const vec3* center, const vec3* extents);

// --------------------------------------------------------------------------------
// ----------------------------------- Frustum ------------------------------------
// --------------------------------------------------------------------------------
BAPI frustum frustum_create(const vec3* position, const vec3* target, const vec3* up, f32 aspect, f32 fov, f32 near, f32 far);

BAPI frustum frustum_from_view_projection(mat4 view_projection);

BAPI b8 frustum_intersects_sphere(const frustum* f, const vec3* center, f32 radius);

BAPI b8 frustum_intersects_aabb(const frustum* f, const vec3* center, const vec3* extents);

BAPI void frustum_corner_points_world_space(mat4 projection_view, vec4* corners);

// --------------------------------------------------------------------------------
// ------------------------- Oriented Bounding Box (OBB) --------------------------
// --------------------------------------------------------------------------------
BAPI f32 oriented_bounding_box_project(const oriented_bounding_box* obb, vec3 axis);

BINLINE b8 rect_2d_contains_point(rect_2d rect, vec2 point)
{
    return (point.x >= rect.x && point.x <= rect.x + rect.width) && (point.y >= rect.y && point.y <= rect.y + rect.height);
}

BINLINE vec3 extents_2d_half(extents_2d extents)
{
    return (vec3)
    {
        (extents.min.x + extents.max.x) * 0.5f,
        (extents.min.y + extents.max.y) * 0.5f,
    };
}

BINLINE vec3 extents_3d_half(extents_3d extents)
{
    return (vec3)
    {
        (extents.min.x + extents.max.x) * 0.5f,
        (extents.min.y + extents.max.y) * 0.5f,
        (extents.min.z + extents.max.z) * 0.5f
    };
}

BINLINE vec2 vec2_mid(vec2 v_0, vec2 v_1)
{
    return (vec2){
        (v_0.x - v_1.x) * 0.5f,
        (v_0.y - v_1.y) * 0.5f
    };
}

BINLINE vec3 edge_3d_get_closest_point(vec3 point, vec3 edge_start, vec3 edge_end)
{
    vec3 edge = vec3_sub(edge_end, edge_start);
    f32 edge_length_sq = vec3_length_squared(edge);

    if (edge_length_sq == 0.0f)
    {
        // Degenerate edge, just use the edge's start point
        return edge_start;
    }

    // Project the point onto the edge, clamping it to within the edge segment as well
    vec3 point_to_start = vec3_sub(point, edge_start);
    f32 t = vec3_dot(point_to_start, edge) / edge_length_sq;
    t = BCLAMP(t, 0.0f, 1.0f);

    // Interpolate along the edge to find the closest point
    return vec3_add(edge_start, vec3_mul_scalar(edge, t));
}

BINLINE vec3 triangle_3d_get_normal(const triangle_3d* tri)
{
    vec3 edge1 = vec3_sub(tri->verts[1], tri->verts[0]);
    vec3 edge2 = vec3_sub(tri->verts[2], tri->verts[0]);

    vec3 normal = vec3_cross(edge1, edge2);
    return vec3_normalized(normal);
}

BINLINE vec3 triangle_3d_get_closest_point(vec3 point, const triangle_3d* tri)
{
    vec3 p0 = tri->verts[0];
    vec3 p1 = tri->verts[1];
    vec3 p2 = tri->verts[2];

    vec3 closest_0_1 = edge_3d_get_closest_point(point, p0, p1);
    vec3 closest_1_2 = edge_3d_get_closest_point(point, p1, p2);
    vec3 closest_2_0 = edge_3d_get_closest_point(point, p2, p0);

    f32 dist_0 = vec3_distance(point, closest_0_1);
    f32 dist_1 = vec3_distance(point, closest_1_2);
    f32 dist_2 = vec3_distance(point, closest_2_0);

    if (dist_0 < dist_1 && dist_0 < dist_2)
    {
        return closest_0_1;
    }
    else if (dist_1 < dist_2)
    {
        return closest_1_2;
    }
    else
    {
        return closest_2_0;
    }
}
