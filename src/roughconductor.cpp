#include "roughconductor.h"
#include "microfacet.h"

int GetRoughConductorSerializedSize() {
    return 1 +  // type
           3 +  // Ks
           1 +  // eta
           1;   // alpha
}

void RoughConductor::Serialize(const Vector2 st, Float *buffer) const {
    buffer = ::Serialize((Float)BSDFType::RoughConductor, buffer);
    buffer = ::Serialize(Ks->Eval(st), buffer);
    buffer = ::Serialize(eta, buffer);
    ::Serialize(alpha->Eval(st)[0], buffer);
}

template <bool adjoint>
void Evaluate(const RoughConductor *bsdf,
              const Vector3 &wi,
              const Vector3 &normal,
              const Vector3 &wo,
              const Vector2 st,
              Vector3 &contrib,
              Float &cosWo,
              Float &pdf,
              Float &revPdf) {
    
    // Init
    contrib.setZero();
    pdf = revPdf = Float(0.0);

    Float cosWi = Dot(wi, normal);
    cosWo = Dot(wo, normal);
    
    if ( (fabs(cosWi) < c_CosEpsilon) || (fabs(cosWo) < c_CosEpsilon) ||
        ( cosWi < 0.f ) || (cosWo < 0.f)) {
        return;
    }

    Float eta_ = bsdf->eta;
    // reflection half-vector
    Vector3 H = Normalize(Vector3(wi + wo));

    Float cosHWi = Dot(wi, H);
    Float cosHWo = Dot(wo, H);

    if (fabs(cosHWi) < c_CosEpsilon || fabs(cosHWo) < c_CosEpsilon) {
        return;
    }

    // Geometry term
    if ( ( cosHWi * cosWi <= Float(0.0) ) || 
            ( cosHWo * cosWo <= Float(0.0) )    
        ) {
        return;
    }

    Vector3 b0;
    Vector3 b1;
    CoordinateSystem(normal, b0, b1);

    Vector3 localH = Vector3(Dot(b0, H), Dot(b1, H), Dot(normal, H));
    Float alp = bsdf->alpha->Eval(st)[0];
    
    Float D = BeckmennDistributionTerm(localH, alp, alp);
    if (D <= Float(0.0)) {
        return;
    }

    Float revCosHWi = cosHWo;
    Float revCosHWo = cosHWi;

    Float F = FresnelConductorExact(cosHWi, eta_);

    Float aCosWi = fabs(cosWi);
    Float aCosWo = fabs(cosWo);

    Float G = BeckmennGeometryTerm(alp, aCosWi, aCosWo);

    Float scaledAlpha = alp * (Float(1.2) - Float(0.2) * sqrt(aCosWi));
    Float scaledD = BeckmennDistributionTerm(localH, scaledAlpha, scaledAlpha);
    Float prob = localH[2] * scaledD;

    if (prob < Float(1e-20)) {
        contrib = Vector3::Zero();
        return;
    }

    Float revScaledAlpha = alp * (Float(1.2) - Float(0.2) * sqrt(aCosWo));
    Float revScaledD = BeckmennDistributionTerm(localH, revScaledAlpha, revScaledAlpha);
    Float revProb = localH[2] * revScaledD;

    Float scalar = fabs(F * D * G / (Float(4.0) * cosWi));
    contrib = bsdf->Ks->Eval(st) * scalar;
    pdf = fabs(prob * F / (Float(4.0) * cosHWo));
    revPdf = fabs(revProb * F / (Float(4.0) * revCosHWo));
    
}

void RoughConductor::Evaluate(const Vector3 &wi,
                               const Vector3 &normal,
                               const Vector3 &wo,
                               const Vector2 st,
                               Vector3 &contrib,
                               Float &cosWo,
                               Float &pdf,
                               Float &revPdf) const {
    ::Evaluate<false>(this, wi, normal, wo, st, contrib, cosWo, pdf, revPdf);
}

void RoughConductor::EvaluateAdjoint(const Vector3 &wi,
                                      const Vector3 &normal,
                                      const Vector3 &wo,
                                      const Vector2 st,
                                      Vector3 &contrib,
                                      Float &cosWo,
                                      Float &pdf,
                                      Float &revPdf) const {
    ::Evaluate<true>(this, wi, normal, wo, st, contrib, cosWo, pdf, revPdf);
}

template <bool adjoint>
bool Sample(const RoughConductor *bsdf,
            const Vector3 &wi,
            const Vector3 &normal,
            const Vector2 st,
            const Vector2 rndParam,
            // const Float /*uDiscrete*/,
            const Float uDiscrete,
            Vector3 &wo,
            Vector3 &contrib,
            Float &cosWo,
            Float &pdf,
            Float &revPdf) {

    Float cosWi = Dot(wi, normal);
    if ( (fabs(cosWi) < c_CosEpsilon) || cosWi < 0.f) {
        return false;
    }
    Float alp = bsdf->alpha->Eval(st)[0];
    Float scaledAlp = alp * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWi)));
    Float mPdf;
    Vector3 localH = SampleMicronormal(rndParam, scaledAlp, mPdf);
    pdf = mPdf;
    Vector3 b0;
    Vector3 b1;
    CoordinateSystem(normal, b0, b1);
    Vector3 H = localH[0] * b0 + localH[1] * b1 + localH[2] * normal;
    Float cosHWi = Dot(wi, H);
    if (fabs(cosHWi) < c_CosEpsilon) {
        return false;
    }
    Float cosThetaT = 0.0;
    Float F = FresnelConductorExact(cosThetaT, bsdf->eta);
    
    Vector3 refl;
    Float cosHWo;

    wo = Reflect(wi, H);
    // side check
    if (F <= Float(0.0) || Dot(normal, wo) * Dot(normal, wi) <= Float(0.0)) {
        return false;
    }
    refl = bsdf->Ks->Eval(st);
    cosHWo = Dot(wo, H);
    pdf = fabs(pdf * F / (Float(4.0) * cosHWo));
    
    Float revCosHWo = cosHWi;
    Float rev_dwh_dwo = inverse(Float(4.0) * revCosHWo);
    cosWo = Dot(wo, normal);
    
    if (fabs(cosWo) < c_CosEpsilon) {
        return false;
    }

    Float revScaledAlp = alp * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWo)));
    Float revD = BeckmennDistributionTerm(localH, revScaledAlp, revScaledAlp);
    revPdf = fabs(F * revD * localH[2] * rev_dwh_dwo);

    if (fabs(cosHWo) < c_CosEpsilon) {
        return false;
    }

    if (pdf < Float(1e-20)) {
        return false;
    }

    // Geometry term
    if (cosHWi * cosWi <= Float(0.0)) {
        return false;
    }
    if (cosHWo * cosWo <= Float(0.0)) {
        return false;
    }

    Float aCosWi = fabs(cosWi);
    Float aCosWo = fabs(cosWo);

    Float D = BeckmennDistributionTerm(localH, alp, alp);
    Float G = BeckmennGeometryTerm(alp, aCosWi, aCosWo);
    Float numerator = D * G * cosHWi;
    Float denominator = mPdf * aCosWi;
    contrib = refl * fabs(numerator / denominator);

    return true;
}

bool RoughConductor::Sample(const Vector3 &wi,
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
        this, wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
}

bool RoughConductor::SampleAdjoint(const Vector3 &wi,
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
        this, wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);
}

void EvaluateRoughConductor(const bool adjoint,
                             const ADFloat *buffer,
                             const ADVector3 &wi,
                             const ADVector3 &normal,
                             const ADVector3 &wo,
                             const ADVector2 st,
                             ADVector3 &contrib,
                             ADFloat &cosWo,
                             ADFloat &pdf,
                             ADFloat &revPdf) {
    ADVector3 Ks;
    ADFloat eta;
    ADFloat alpha;
    buffer = Deserialize(buffer, Ks);
    buffer = Deserialize(buffer, eta);
    buffer = Deserialize(buffer, alpha);

    ADFloat cosWi = Dot(wi, normal);
    cosWo = Dot(wo, normal);
    
    ADVector3 H = Normalize(ADVector3(wi + wo));

    ADFloat cosHWi = Dot(wi, H);
    ADFloat cosHWo = Dot(wo, H);
   
    ADVector3 b0;
    ADVector3 b1;
    CoordinateSystem(normal, b0, b1);

    ADVector3 localH = ADVector3(Dot(b0, H), Dot(b1, H), Dot(normal, H));
    ADFloat D = BeckmennDistributionTerm(localH, alpha, alpha);
    // This is confusing, but when computing reverse probability,
    // wo and wi are reversed
    ADFloat revCosHWi = cosHWo;
    ADFloat revCosHWo = cosHWi;
    ADFloat F = FresnelConductorExact(cosHWi, eta);
    ADFloat aCosWi = fabs(cosWi);
    ADFloat aCosWo = fabs(cosWo);

    ADFloat G = BeckmennGeometryTerm(alpha, aCosWi, aCosWo);
    ADFloat scaledAlpha = alpha * (Float(1.2) - Float(0.2) * sqrt(aCosWi));
    ADFloat scaledD = BeckmennDistributionTerm(localH, scaledAlpha, scaledAlpha);
    ADFloat prob = localH[2] * scaledD;
    ADFloat revScaledAlpha = alpha * (Float(1.2) - Float(0.2) * sqrt(aCosWo));
    ADFloat revScaledD = BeckmennDistributionTerm(localH, revScaledAlpha, revScaledAlpha);
    ADFloat revProb = localH[2] * revScaledD;

    ADFloat scalar = fabs(F * D * G / (Float(4.0) * cosWi));
    contrib = Ks * scalar;
    pdf = fabs(prob * F / (Float(4.f) * cosHWo));
    revPdf = fabs(revProb * F / (Float(4.0) * revCosHWo));

}

void SampleRoughConductor(const bool adjoint,
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
    ADVector3 Ks;
    ADFloat eta;
    ADFloat alpha;
    buffer = Deserialize(buffer, Ks);
    buffer = Deserialize(buffer, eta);
    buffer = Deserialize(buffer, alpha);

    ADFloat cosWi = Dot(wi, normal);
    ADFloat scaledAlpha = alpha * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWi)));
    ADFloat mPdf;
    ADVector3 localH = SampleMicronormal(rndParam, scaledAlpha, mPdf);
    ADVector3 b0;
    ADVector3 b1;
    CoordinateSystem(normal, b0, b1);
    ADVector3 H = localH[0] * b0 + localH[1] * b1 + localH[2] * normal;
    ADFloat cosHWi = Dot(wi, H);

    ADFloat F = FresnelConductorExact(cosHWi, eta);
    
    wo = Reflect(wi, H);
    ADVector3 refl = Ks;
    if (fixDiscrete) {
        refl *= F;
    }
    ADFloat cosHWo = Dot(wo, H);
    pdf = fabs(mPdf * F / (Float(4.0) * cosHWo));
    ADFloat revCosHWo = cosHWi;
    ADFloat rev_dwh_dwo = inverse(Float(4.0) * revCosHWo);
    cosWo = Dot(wo, normal);
    ADFloat revScaledAlp = alpha * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWo)));
    ADFloat revD = BeckmennDistributionTerm(localH, revScaledAlp, revScaledAlp);
    revPdf = fabs(F * revD * localH[2] * rev_dwh_dwo);
        
    ADFloat aCosWi = fabs(cosWi);
    ADFloat aCosWo = fabs(cosWo);
    ADFloat D = BeckmennDistributionTerm(localH, alpha, alpha);
    ADFloat G = BeckmennGeometryTerm(alpha, aCosWi, aCosWo);
    ADFloat numerator = D * G * cosHWi;
    ADFloat denominator = mPdf * aCosWi;
    contrib = refl * fabs(numerator / denominator);

}