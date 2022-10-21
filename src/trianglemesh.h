#pragma once

#include "mesh.h"
#include "shape.h"
#include "distribution.h"

int GetTriangleMeshSerializedSize();

struct TriangleMesh : public Shape {
    TriangleMesh(const std::shared_ptr<const BSDF> bsdf, const std::shared_ptr<TriMeshData> data,
        const std::string _id=""
        );
    ShapeType GetType() const override {
        return ShapeType::TriangleMesh;
    }
    ShapeID RtcRegister(const RTCScene &scene, const RTCDevice &device) const override;
    void Serialize(const PrimID primID, Float *buffer) const override;
    bool Intersect(const PrimID &primID,
                   const Float time,
                   const RaySegment &raySeg,
                   Intersection &isect,
                   Vector2 &st) const override;
    Vector2 GetSampleParam(const PrimID &primID,
                           const Vector3 &position,
                           const Float time) const override;
    // void SetAreaLight(const AreaLight *areaLight) override;
    void SetAreaLight(const Light *areaLight) override;
    PrimID Sample(const Float u) const override;
    void Sample(const Vector2 rndParam,
                const Float time,
                const PrimID primID,
                Vector3 &position,
                Vector3 &normal,
                Float *pdf) const override;
    Float SamplePdf() const override {
        return inverse(totalArea);
    }
    BBox GetBBox() const override {
        return bbox;
    }
    bool IsMoving() const override {
        return data->isMoving;
    }

    std::string GetId() const override { return id; }

    const std::shared_ptr<const TriMeshData> data;
    const BBox bbox;
    // Only used when the mesh is associated with an area light
    Float totalArea;
    std::unique_ptr<PiecewiseConstant1D> areaDist;
    std::string id;
};

void IntersectTriangleMesh(const ADFloat *buffer,
                           const ADRay &ray,
                           const ADFloat time,
                           const bool isStatic,
                           ADIntersection &isect,
                           ADVector2 &st);

void SampleTriangleMesh(const ADFloat *buffer,
                        const ADVector2 rndParam,
                        const ADFloat time,
                        const bool isStatic,
                        ADVector3 &position,
                        ADVector3 &normal,
                        ADFloat &pdf);

ADFloat SampleTriangleMeshPdf(const ADFloat *buffer);
