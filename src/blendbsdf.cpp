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
    // std::cout << "BlendBSDF Serialization" << std::endl;
    bsdfA->Serialize(st, buffer);
    // std::cout << "type A : " << (Float)bsdfA->GetType() << std::endl;
    bsdfB->Serialize(st, buffer + GetMaxBSDFSerializedSize2());
    // std::cout << "type B : " << (Float)bsdfB->GetType() << std::endl;
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

    Float weightB = Clamp(0.f, 1.f, weight);
    Float weightA = 1.f - weightB;

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
    contrib = weightA * contribA + weightB * contribB;
    pdf = weightA * pdfA + weightB * pdfB;
    revPdf = weightA * revPdfA + weightB * revPdfB;

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

    Float weightB = Clamp(0.f, 1.f, weight);
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

    ADFloat cosWi = Dot(wi, normal);
    cosWo = Dot(wo, normal);

    ADVector3 contribA, contribB;
    ADFloat cosWoA, cosWoB, 
        pdfA, pdfB,
        revPdfA, revPdfB;

    buffer = EvaluateBSDF2(adjoint, buffer, wi, normal, wo, st, contribA, cosWoA, pdfA, revPdfA);
    EvaluateBSDF2(adjoint, buffer, wi, normal, wo, st, contribB, cosWoB, pdfB, revPdfB);

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

    ADVector2 sampleForA = rndParam, sampleForB = rndParam;

    ADFloat weightB;
    // BSDF bsdfA, bsdfB;
    buffer = Deserialize(buffer, weightB);
    
    ADFloat weightA = Float(1.f) - weightB;

    BooleanCPtr sampleA = Lte(sampleForA[0], weightA);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(9);

    // Need to Deserialize both bsdf
    ADVector3 wo_A, contrib_A;
    ADFloat cosWo_A, pdf_A, revPdf_A;
    sampleForA[0] /= weightA;
    buffer = SampleBSDF2(adjoint, buffer, wi, normal, st, sampleForA, uDiscrete, fixDiscrete, wo_A, contrib_A, cosWo_A, pdf_A, revPdf_A);
    ADVector3 wo_B, contrib_B;
    ADFloat cosWo_B, pdf_B, revPdf_B;
    sampleForB[0] = ( sampleForB[0] - weightA) * inverse(weightB);
    SampleBSDF2(adjoint, buffer, wi, normal, st, sampleForB, uDiscrete, fixDiscrete, wo_B, contrib_B, cosWo_B, pdf_B, revPdf_B);

    BeginIf(sampleA, ret);
    {
        contrib_A *= weightA * pdf_A;
        pdf_A *= weightA;
        revPdf_A *= weightA;
        SetCondOutput({wo_A[0], wo_A[1], wo_A[2], contrib_A[0], contrib_A[1], contrib_A[2], cosWo_A, pdf_A, revPdf_A});
    }
    BeginElse();
    {
        contrib_B *= weightB * pdf_B;
        pdf_B *= weightB;
        revPdf_B *= weightB;
        SetCondOutput({wo_B[0], wo_B[1], wo_B[2], contrib_B[0], contrib_B[1], contrib_B[2], cosWo_B, pdf_B, revPdf_B});
    }
    EndIf();
    
    wo = ADVector3(ret[0], ret[1], ret[2]);
    contrib = ADVector3(ret[3], ret[4], ret[5]);
    cosWo = ret[6];
    pdf = ret[7];
    revPdf = ret[8];

}