#pragma once

#include "commondef.h"
#include "animatedtransform.h"
#include "image.h"
#include "ray.h"

int GetCameraSerializedSize();

struct Camera {
    Camera(const AnimatedTransform &camToWorld,
           const Float fov,
           const std::shared_ptr<Image3> film,
           const Float nearClip,
           const Float farClip, 
           const int pixelWidth,
           const int pixelHeight,
           const int cropOffsetX, 
           const int cropOffsetY, 
           const int cropWidth, 
           const int cropHeight );

    Matrix4x4 sampleToCam, camToSample;
    AnimatedTransform camToWorld, worldToCamera;

    std::shared_ptr<Image3> film;
    Float nearClip, farClip;
    Float dist;
    int pixelWidth, pixelHeight;
    int cropWidth, cropHeight;
    int cropOffsetX, cropOffsetY;
};

Float *Serialize(const Camera *camera, Float *buffer);

void SamplePrimary(const Camera *camera,
                   const Vector2 screenPos,
                   const Float time,
                   RaySegment &raySeg);

void SamplePrimary(const ADMatrix4x4 &sampleToCam,
                   const ADAnimatedTransform &camToWorld,
                   const ADVector2 screenPos,
                   const ADFloat time,
                   const bool isStatic,
                   ADRay &ray);

bool ProjectPoint(const Camera *camera, const Vector3 &p, const Float t, Vector2 &screenPos);

inline std::shared_ptr<Image3> GetFilm(const Camera *camera) {
    return camera->film;
}

inline int GetPixelHeight(const Camera *camera) {
    return camera->pixelHeight;
}

inline int GetPixelWidth(const Camera *camera) {
    return camera->pixelWidth;
}

inline int GetCropHeight(const Camera *camera) {
    return camera->cropHeight;
}

inline int GetCropWidth(const Camera *camera) {
    return camera->cropWidth;
}

inline int GetCropOffsetX(const Camera *camera) {
    return camera->cropOffsetX;
}

inline int GetCropOffsetY(const Camera *camera) {
    return camera->cropOffsetY;
}
