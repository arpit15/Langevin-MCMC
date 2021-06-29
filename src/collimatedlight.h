#pragma once

#include "light.h"
#include "animatedtransform.h"

struct Shape;

int GetCollimatedLightSerializedSize();

struct CollimatedLight : public Light {
    CollimatedLight(const Float &samplingWeight, 
            const AnimatedTransform &toWorld,
            const Float &_radius, const Vector3 &emission);

    LightType GetType() const override {
        return LightType::CollimatedLight;
    }
    void Serialize(const LightPrimID &lPrimID, Float *buffer) const override;
    // LightPrimID SampleDiscrete(const Float uDiscrete) const override;
    bool SampleDirect(const BSphere &sceneSphere,
                      const Vector3 &pos,
                      const Vector3 &normal,
                      const Vector2 rndParam,
                      const Float time,
                      LightPrimID &lPrimID,
                      Vector3 &dirToLight,
                      Float &dist,
                      Vector3 &contrib,
                      Float &cosAtLight,
                      Float &directPdf,
                      Float &emissionPdf) const override;
    // void Emission(const BSphere &sceneSphere,
    //               const Vector3 &dirToLight,
    //               const Vector3 &normalAtLight,
    //               const Float time,
    //               LightPrimID &lPrimID,
    //               Vector3 &emission,
    //               Float &directPdf,
    //               Float &emissionPdf) const override;
    void Emit(const BSphere &sceneSphere,
              const Vector2 rndParamPos,
              const Vector2 rndParamDir,
              const Float time,
              LightPrimID &lPrimID,
              Ray &ray,
              Vector3 &emission,
              Float &cosAtLight,
              Float &emissionPdf,
              Float &directPdf) const override;
    bool IsFinite() const override {
        return true;
    }
    bool IsDelta() const override {
        return true;
    }

    const AnimatedTransform toWorld;
    const AnimatedTransform toLight;
    const Float m_radius;
    const Vector3 emission;
};

void SampleDirectCollimatedLight(const ADFloat *buffer,
                           const ADBSphere &sceneSphere,
                           const ADVector3 &pos,
                           const ADVector3 &normal,
                           const ADVector2 rndParam,
                           const ADFloat time,
                           const bool isStatic,
                           ADVector3 &dirToLight,
                           ADVector3 &lightContrib,
                           ADFloat &cosAtLight,
                           ADFloat &directPdf,
                           ADFloat &emissionPdf);

// void EmissionCollimatedLight(const ADFloat *buffer,
//                        const ADBSphere &sceneSphere,
//                        const ADVector3 &dirToLight,
//                        const ADVector3 &normalOnLight,
//                        const ADFloat time,
//                        ADVector3 &emission,
//                        ADFloat &directPdf,
//                        ADFloat &emissionPdf);

void EmitCollimatedLight(const ADFloat *buffer,
                   const ADBSphere &sceneSphere,
                   const ADVector2 rndParamPos,
                   const ADVector2 rndParamDir,
                   const ADFloat time,
                   const bool isStatic,
                   ADRay &ray,
                   ADVector3 &emission,
                   ADFloat &cosAtLight,
                   ADFloat &emissionPdf,
                   ADFloat &directPdf);
