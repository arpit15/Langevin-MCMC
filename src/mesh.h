#pragma once

#include "commondef.h"
#include "utils.h"

#include <vector>

struct TriIndex {
    TriIndex() {
    }
    TriIndex(const TriIndexID id0, const TriIndexID id1, const TriIndexID id2) {
        index[0] = id0;
        index[1] = id1;
        index[2] = id2;
    }
    TriIndexID index[3];
};

struct TriMeshData {
    std::vector<Vector3> position0;
    std::vector<Vector3> position1;
    std::vector<Vector3> normal0;
    std::vector<Vector3> normal1;
    std::vector<Vector2> st;
    std::vector<Vector3> colors;
    std::vector<TriIndex> indices;
    bool isMoving;
};

// Numerical robust computation of angle between unit vectors
template <typename VectorType>
inline Float UnitAngle(const VectorType &u, const VectorType &v) {
    if (Dot(u, v) < 0)
        return (c_PI - Float(2.0)) * asin(Float(0.5) * Length(Vector3(v + u)));
    else
        return Float(2.0) * asin(Float(0.5) * Length(Vector3(v - u)));
}


inline void ComputeNormal(const std::vector<Vector3> &vertices,
                          const std::vector<TriIndex> &triangles,
                          std::vector<Vector3> &normals) {
    normals.resize(vertices.size(), Vector3::Zero());

    // Nelson Max, "Computing Vertex Vector3ds from Facet Vector3ds", 1999
    for (auto &tri : triangles) {
        Vector3 n = Vector3::Zero();
        for (int i = 0; i < 3; ++i) {
            const Vector3 &v0 = vertices[tri.index[i]];
            const Vector3 &v1 = vertices[tri.index[(i + 1) % 3]];
            const Vector3 &v2 = vertices[tri.index[(i + 2) % 3]];
            Vector3 sideA(v1 - v0), sideB(v2 - v0);
            if (i == 0) {
                n = Cross(sideA, sideB);
                Float length = Length(n);
                if (length == 0)
                    break;
                n = n / length;
            }
            Float angle = UnitAngle(Normalize(sideA), Normalize(sideB));
            normals[tri.index[i]] = normals[tri.index[i]] + n * angle;
        }
    }

    for (auto &n : normals) {
        Float length = Length(n);
        if (length != 0) {
            n = n / length;
        } else {
            /* Choose some bogus value */
            n = Vector3::Zero();
        }
    }
}