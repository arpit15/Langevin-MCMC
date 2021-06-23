#pragma once

#include "commondef.h"
#include "bsdf.h"
#include "utils.h"
#include "texture.h"

int GetBlendBSDFSerializedSize();

struct BlendBSDF : public BSDF {
    BlendBSDF(const bool /*twoSided*/,
                    const std::shared_ptr<const TextureRGB> &weight,
                    const std::shared_ptr<const BSDF> &bsdfA,
                    const std::shared_ptr<const BSDF> &bsdfB)
        : weight(weight), bsdfA(bsdfA), bsdfB(bsdfB) {
    }

    BSDFType GetType() const override {
        return BSDFType::BlendBSDF;
    }
    void Serialize(const Vector2 st, Float *buffer) const override;
    void Evaluate(const Vector3 &wi,
                  const Vector3 &normal,
                  const Vector3 &wo,
                  const Vector2 st,
                  Vector3 &contrib,
                  Float &cosWo,
                  Float &pdf,
                  Float &revPdf) const override;
    void EvaluateAdjoint(const Vector3 &wi,
                         const Vector3 &normal,
                         const Vector3 &wo,
                         const Vector2 st,
                         Vector3 &contrib,
                         Float &cosWo,
                         Float &pdf,
                         Float &revPdf) const override;
    bool Sample(const Vector3 &wi,
                const Vector3 &normal,
                const Vector2 st,
                const Vector2 rndParam,
                const Float uDiscrete,
                Vector3 &wo,
                Vector3 &contrib,
                Float &cosWo,
                Float &pdf,
                Float &revPdf) const override;
    bool SampleAdjoint(const Vector3 &wi,
                       const Vector3 &normal,
                       const Vector2 st,
                       const Vector2 rndParam,
                       const Float uDiscrete,
                       Vector3 &wo,
                       Vector3 &contrib,
                       Float &cosWo,
                       Float &pdf,
                       Float &revPdf) const override;

    Float Roughness(const Vector2 st, const Float /*uDiscrete*/) const override {
        // return std::max( bsdfA->Roughness(st), bsdfB->Roughness(st) );
        return 1.0f;
    }

    std::shared_ptr<const TextureRGB> weight;
    std::shared_ptr<const BSDF> bsdfA, bsdfB;
};

void EvaluateBlendBSDF(const bool adjoint,
                             const ADFloat *buffer,
                             const ADVector3 &wi,
                             const ADVector3 &normal,
                             const ADVector3 &wo,
                             const ADVector2 st,
                             ADVector3 &contrib,
                             ADFloat &cosWo,
                             ADFloat &pdf,
                             ADFloat &revPdf);


void SampleBlendBSDF(const bool adjoint,
                           const ADFloat *buffer,
                           const ADVector3 &wi,
                           const ADVector3 &normal,
                           const ADVector2 st,
                           const ADVector2 rndParam,
                           const ADFloat uDiscrete,
                           const bool fixDiscrete,
                           ADVector3 &wo,
                           ADVector3 &contrib,
                           ADFloat &cosWo,
                           ADFloat &pdf,
                           ADFloat &revPdf);
