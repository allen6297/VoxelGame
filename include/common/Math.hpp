#pragma once

#include <cmath>

namespace voxel {
constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x;
    float y;
    float z;

    float lengthSquared() const {
        return x * x + y * y + z * z;
    }
};

struct Int3 {
    int x;
    int y;
    int z;
};

float toRadians(float degrees);
Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& v, float scalar);
float length(const Vec3& v);
float dot(const Vec3& a, const Vec3& b);
    Vec3 cross(const Vec3& a, const Vec3& b);
    Vec3 normalize(const Vec3& v);

    struct Plane {
        Vec3 normal;
        float distance = 0.0f;

        float distanceTo(const Vec3& point) const {
            return dot(normal, point) + distance;
        }
    };

    struct Frustum {
        Plane planes[6];

        bool isBoxVisible(const Vec3& min, const Vec3& max) const {
            for (const auto& plane : planes) {
                Vec3 p = min;
                if (plane.normal.x >= 0) p.x = max.x;
                if (plane.normal.y >= 0) p.y = max.y;
                if (plane.normal.z >= 0) p.z = max.z;

                if (plane.distanceTo(p) < 0) {
                    return false;
                }
            }
            return true;
        }
    };

    Frustum calculateFrustum(const Vec3& pos, const Vec3& forward, float fovYDegrees, float aspect, float nearPlane, float farPlane);
}  // namespace voxel
