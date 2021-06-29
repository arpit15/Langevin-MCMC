#pragma once

#include "light.h"
#include "animatedtransform.h"

int GetSpotLightSerializedSize();

struct SpotLight : public Light {
    SpotLight(const Float &samplingWeight, 
          const AnimatedTransform &toWorld, 
          const Vector3 &emission,
          const Float &cutoffAngle, 
          const Float &beamWidth);

    LightType GetType() const override {
        return LightType::SpotLight;
    }
    void Serialize(const LightPrimID &lPrimID, const Vector2 &rndDir, Float *buffer) const override;
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
        return false;
    }
    bool IsDelta() const override {
        return true;
    }

    const AnimatedTransform toWorld;
    const AnimatedTransform toLight;
    const Vector3 emission;
    const Float cutoffAngle, beamWidth;
    Float uvFactor, cosCutoffAngle, cosBeamWidth, invTransitionWidth;
};

void SampleDirectSpotLight(const ADFloat *buffer,
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

void EmitSpotLight(const ADFloat *buffer,
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
