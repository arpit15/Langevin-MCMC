#include "blendbsdf.h"

int GetBlendBSDFSerializedSize() {
    return 1 + // type
            1 + // weight
            GetMaxBSDFSerializedSize2() + // bsdfA
            GetMaxBSDFSerializedSize2(); // bsdfB 
}

void BlendBSDF::Serialize(const Vector2 st, Float *buffer) const {
    buffer = ::Serialize((Float)BSDFType::BlendBSDF, buffer);
    buffer = ::Serialize((Float)weight->Eval(st).mean(), buffer);
    bsdfA->Serialize(st, buffer);
    bsdfB->Serialize(st, buffer);
}

template <bool adjoint>
void Evaluate(const BlendBSDF *bsdf,
                // const Vector3 weight,
                const Float weight, 
              const Vector3 &wi,
              const Vector3 &normal,
              const Vector3 &wo,
              const Vector2 st,
              // outputs
              Vector3 &contrib,
              Float &cosWo,
              Float &pdf,
              Float &revPdf) {

    Float cosWi = Dot(wi, normal);
    contrib.setZero();
    cosWo = Float(0.0);
    pdf = revPdf = Float(0.0);
    if (fabs(cosWi) < c_CosEpsilon) {
        return;
    }

    cosWo = Dot(wo, normal);
    if (fabs(cosWo) < c_CosEpsilon) {
        return;
    }

    Float currWeight = std::min(
                    1.f, std::max ( 0.f,  weight )
    );

    // Vector3 currWeight = weight.cwiseMax(0.f).cwiseMin(1.f);
    

    Vector3 contribA, contribB;
    Float cosWoA, cosWoB, 
        pdfA, pdfB,
        revPdfA, revPdfB;

    if (!adjoint) {
        bsdf->bsdfA->Evaluate(wi, normal, wo, st, contribA, cosWoA, pdfA, revPdfA);
        bsdf->bsdfB->Evaluate(wi, normal, wo, st, contribB, cosWoB, pdfB, revPdfB);
    } else {
        bsdf->bsdfA->EvaluateAdjoint(wi, normal, wo, st, contribA, cosWoA, pdfA, revPdfA);
        bsdf->bsdfB->EvaluateAdjoint(wi, normal, wo, st, contribB, cosWoB, pdfB, revPdfB);
    }
    contrib = (Float(1.f) - currWeight) * contribA + currWeight * contribB;
    pdf = (Float(1.f) - currWeight) * pdfA + currWeight * pdfB;
    revPdf = (Float(1.f) - currWeight) * revPdfA + currWeight * revPdfB;

}


void BlendBSDF::Evaluate(const Vector3 &wi,
                               const Vector3 &normal,
                               const Vector3 &wo,
                               const Vector2 st,
                               Vector3 &contrib,
                               Float &cosWo,
                               Float &pdf,
                               Float &revPdf) const {
    ::Evaluate<false>(this, weight->Eval(st).mean(), wi, normal, wo, st, contrib, cosWo, pdf, revPdf);
}

void BlendBSDF::EvaluateAdjoint(const Vector3 &wi,
                                      const Vector3 &normal,
                                      const Vector3 &wo,
                                      const Vector2 st,
                                      Vector3 &contrib,
                                      Float &cosWo,
                                      Float &pdf,
                                      Float &revPdf) const {
    ::Evaluate<true>(this, weight->Eval(st).mean(), wi, normal, wo, st, contrib, cosWo, pdf, revPdf);
}

template <bool adjoint>
bool Sample(const BlendBSDF *bsdf,
            // const Vector3 weight,
            const Float weight,
            const Vector3 &wi,
            const Vector3 &normal,
            const Vector2 st,
            const Vector2 rndParam,
            const Float uDiscrete,
            Vector3 &wo,
            Vector3 &contrib,
            Float &cosWo,
            Float &pdf,
            Float &revPdf) {

    Vector2 sample = rndParam;

    Float weightB = std::min(
                    1.f, std::max ( 0.f, weight )
    );

    // Vector3 weightB = weight.cwiseMax(0.f).cwiseMin(1.f);

    Float weightA = (1.f - weightB);

    bool result;
    if (sample[0] < weightA) {
        sample[0] /= weightA;
        if(!adjoint)
            result = bsdf->bsdfA->Sample( wi, normal, st, sample, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
        else
            result = bsdf->bsdfA->SampleAdjoint( wi, normal, st, sample, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
        contrib *= weightA * pdf;
        pdf *= weightA;
        revPdf *= weightA;
    } else {
        sample[0] = ( sample[0] - weightA) / weightB;
        if(!adjoint)
            result = bsdf->bsdfB->Sample( wi, normal, st, sample, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
        else
            result = bsdf->bsdfB->SampleAdjoint( wi, normal, st, sample, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
        contrib *= weightB * pdf;
        pdf *= weightB;
        revPdf *= weightB;
    }

    // Need to also account for the same component in other BSDF

    return result;
}


bool BlendBSDF::Sample(const Vector3 &wi,
                             const Vector3 &normal,
                             const Vector2 st,
                             const Vector2 rndParam,
                             const Float uDiscrete,
                             Vector3 &wo,
                             Vector3 &contrib,
                             Float &cosWo,
                             Float &pdf,
                             Float &revPdf) const {
    return ::Sample<false>(
        this, weight->Eval(st).mean(), wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
}

bool BlendBSDF::SampleAdjoint(const Vector3 &wi,
                                    const Vector3 &normal,
                                    const Vector2 st,
                                    const Vector2 rndParam,
                                    const Float uDiscrete,
                                    Vector3 &wo,
                                    Vector3 &contrib,
                                    Float &cosWo,
                                    Float &pdf,
                                    Float &revPdf) const {
    return ::Sample<true>(
        this, weight->Eval(st).mean(), wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
}


void EvaluateBlendBSDF(const bool adjoint,
                             const ADFloat *buffer,
                             const ADVector3 &wi,
                             const ADVector3 &normal,
                             const ADVector3 &wo,
                             const ADVector2 st,
                             ADVector3 &contrib,
                             ADFloat &cosWo,
                             ADFloat &pdf,
                             ADFloat &revPdf) {

    ADFloat weightB;
    // BSDF bsdfA, bsdfB;
    buffer = Deserialize(buffer, weightB);
    // buffer = Deserialize(buffer, bsdfA);
    // buffer = Deserialize(buffer, bsdfB);

    ADFloat cosWi = Dot(wi, normal);
    cosWo = Dot(wo, normal);

    ADVector3 contribA, contribB;
    ADFloat cosWoA, cosWoB, 
        pdfA, pdfB,
        revPdfA, revPdfB;

    EvaluateBSDF(adjoint, buffer, wi, normal, wo, st, contribA, cosWoA, pdfA, revPdfA);
    EvaluateBSDF(adjoint, buffer + GetMaxBSDFSerializedSize2(), wi, normal, wo, st, contribB, cosWoB, pdfB, revPdfB);

    pdf = (Float(1.f) - weightB) * pdfA + weightB * pdfB;
    revPdf = (Float(1.f) - weightB) * revPdfA + weightB * revPdfB;
    contrib = (1.f - weightB) * contribA + weightB * contribB;
    
}


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
                           ADFloat &revPdf) {

    ADVector2 sample = rndParam;

    ADFloat weightB;
    // BSDF bsdfA, bsdfB;
    buffer = Deserialize(buffer, weightB);
    // buffer = Deserialize(buffer, bsdfA);
    // buffer = Deserialize(buffer, bsdfB);

    ADFloat weightA = Float(1.f) - weightB;

    BooleanCPtr sampleA = Lte(sample[0], weightA);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(9);
    BeginIf(sampleA, ret);
    {
        ADVector3 wo_, contrib_;
        ADFloat cosWo_, pdf_, revPdf_;
        sample[0] /= weightA;
        SampleBSDF(adjoint, buffer, wi, normal, st, sample, uDiscrete, fixDiscrete, wo_, contrib_, cosWo_, pdf_, revPdf_);
        contrib_ *= weightA * pdf;
        pdf *= weightA;
        revPdf *= weightA;
        SetCondOutput({wo_[0], wo_[1], wo_[2], contrib_[0], contrib_[1], contrib_[2], cosWo_, pdf_, revPdf_});
    }
    BeginElse();
    {
        ADVector3 wo_, contrib_;
        ADFloat cosWo_, pdf_, revPdf_;
        sample[0] = ( sample[0] - weightA) * inverse(weightB);
        SampleBSDF(adjoint, buffer + GetMaxBSDFSerializedSize2(), wi, normal, st, sample, uDiscrete, fixDiscrete, wo_, contrib_, cosWo_, pdf_, revPdf_);
        contrib_ *= weightB * pdf;
        pdf *= weightB;
        revPdf *= weightB;
        SetCondOutput({wo_[0], wo_[1], wo_[2], contrib_[0], contrib_[1], contrib_[2], cosWo_, pdf_, revPdf_});
    }
    EndIf();
    
    wo = ADVector3(ret[0], ret[1], ret[2]);
    contrib = ADVector3(ret[3], ret[4], ret[5]);
    cosWo = ret[6];
    pdf = ret[7];
    revPdf = ret[8];

}