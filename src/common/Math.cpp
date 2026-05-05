#include "Math.hpp"

namespace voxel {
float toRadians(const float degrees) {
    return degrees * (kPi / 180.0f);
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(const Vec3& v, const float scalar) {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

float length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len <= 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

Frustum calculateFrustum(const Vec3& pos, const Vec3& forward, float fovYDegrees, float aspect, float nearPlane, float farPlane) {
    Frustum f;
    const Vec3 worldUp = {0.0f, 1.0f, 0.0f};
    const Vec3 right = normalize(cross(forward, worldUp));
    const Vec3 up = normalize(cross(right, forward));

    const float halfVSide = farPlane * std::tan(toRadians(fovYDegrees) * 0.5f);
    const float halfHSide = halfVSide * aspect;
    const Vec3 frontMultFar = forward * farPlane;

    // Near plane
    f.planes[0] = {forward, -dot(forward, pos + forward * nearPlane)};
    // Far plane
    f.planes[1] = {forward * -1.0f, -dot(forward * -1.0f, pos + frontMultFar)};
    // Right plane
    f.planes[2] = {normalize(cross(up, frontMultFar + right * halfHSide)), -dot(normalize(cross(up, frontMultFar + right * halfHSide)), pos)};
    // Left plane
    f.planes[3] = {normalize(cross(frontMultFar - right * halfHSide, up)), -dot(normalize(cross(frontMultFar - right * halfHSide, up)), pos)};
    // Top plane
    f.planes[4] = {normalize(cross(right, frontMultFar - up * halfVSide)), -dot(normalize(cross(right, frontMultFar - up * halfVSide)), pos)};
    // Bottom plane
    f.planes[5] = {normalize(cross(frontMultFar + up * halfVSide, right)), -dot(normalize(cross(frontMultFar + up * halfVSide, right)), pos)};

    return f;
}
}  // namespace voxel
