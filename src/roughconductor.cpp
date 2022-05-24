#include "roughconductor.h"
#include "microfacet.h"

int GetRoughConductorSerializedSize() {
    return 1 +  // type
           3 +  // Ks
           1 +  // eta
           1 +  // k
           1;   // alpha
}

void RoughConductor::Serialize(const Vector2 st, Float *buffer) const {
    buffer = ::Serialize((Float)BSDFType::RoughConductor, buffer);
    buffer = ::Serialize(Ks->Eval(st), buffer);
    buffer = ::Serialize(eta, buffer);
    buffer = ::Serialize(k, buffer);
    ::Serialize(alpha->Eval(st)[0], buffer);
}

template <bool adjoint>
void Evaluate(
                const bool twoSided,
                const RoughConductor *bsdf,
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

    Vector3 wi_ = wi, wo_ = wo;
    
    Float cosWi = Dot(wi, normal);

    Vector3 normal_ = normal;
    
    if (cosWi < Float(0.0)) {
        if (twoSided) {
            cosWi = -cosWi;
            normal_ = -normal_;
            // wi_[2] = -wi_[2];  // just flip the z component
            // flipped = true;
        } 
        else {
            // std::cout << "coswi < 0 for some reason" << std::endl;
            return;
        }
    }

    // std::cout << "Eval coswi : " << cosWi << ", coswo : " << cosWo << std::endl;
    cosWo = Dot(wo_, normal_);
    
    if ( (fabs(cosWi) < c_CosEpsilon) || (fabs(cosWo) < c_CosEpsilon) ||
             (cosWo < 0.f)) {
        return;
    }

    // if ( cosWo * cosWi < 0.f)
    //     std::cout << "Something fishy is going on coswi : " << cosWi << ", coswo : " << cosWo << std::endl;
    
    Float eta_ = bsdf->eta, k_ = bsdf->k;
    // reflection half-vector
    Vector3 H = Normalize(Vector3(wi_ + wo_));

    Float cosHWi = Dot(wi_, H);
    Float cosHWo = Dot(wo_, H);

    if (fabs(cosHWi) < c_CosEpsilon || fabs(cosHWo) < c_CosEpsilon || 
        cosHWi < c_CosEpsilon || cosHWo < c_CosEpsilon ) {
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
    CoordinateSystem(normal_, b0, b1);

    Vector3 localH = Vector3(Dot(b0, H), Dot(b1, H), Dot(normal_, H));
    Float alp = bsdf->alpha->Eval(st)[0];
    
    Float D = BeckmennDistributionTerm(localH, alp, alp);
    if (D <= Float(0.0)) {
        return;
    }

    Float revCosHWi = cosHWo;
    Float revCosHWo = cosHWi;

    // Float F = FresnelConductorExact(cosHWi, eta_);
    Float F = FresnelConductorExact(cosHWi, eta_, k_);

    Float aCosWi = fabs(cosWi);
    Float aCosWo = fabs(cosWo);

    Float G = BeckmennGeometryTerm(alp, aCosWi, aCosWo);

    Float scaledAlpha = alp * (Float(1.2) - Float(0.2) * sqrt(aCosWi));
    Float scaledD = BeckmennDistributionTerm(localH, scaledAlpha, scaledAlpha);
    Float prob = localH[2] * scaledD;

    if (prob < Float(1e-20)) {
        return;
    }

    Float revScaledAlpha = alp * (Float(1.2) - Float(0.2) * sqrt(aCosWo));
    Float revScaledD = BeckmennDistributionTerm(localH, revScaledAlpha, revScaledAlpha);
    Float revProb = localH[2] * revScaledD;

    Float scalar = fabs(F * D * G / (Float(4.0) * cosWi));
    contrib = bsdf->Ks->Eval(st) * scalar;
    
    // orig
    // pdf = fabs(prob * F/ (Float(4.0) * cosHWo));
    pdf = fabs(prob / (Float(4.0) * cosHWo));
    
    revPdf = fabs(revProb / (Float(4.0) * revCosHWo));
    
    // taken from phong.cpp
    // Just for numerical stability
    if (contrib.maxCoeff() < Float(1e-10)) {
        contrib.setZero();
    }
}

void RoughConductor::Evaluate(const Vector3 &wi,
                               const Vector3 &normal,
                               const Vector3 &wo,
                               const Vector2 st,
                               Vector3 &contrib,
                               Float &cosWo,
                               Float &pdf,
                               Float &revPdf) const {
    ::Evaluate<false>(twoSided, this, wi, normal, wo, st, contrib, cosWo, pdf, revPdf);

    if (std::isnan(contrib.sum()))
        std::cout << "roughconductor contrib:" << contrib.transpose() << std::endl;
}

void RoughConductor::EvaluateAdjoint(const Vector3 &wi,
                                      const Vector3 &normal,
                                      const Vector3 &wo,
                                      const Vector2 st,
                                      Vector3 &contrib,
                                      Float &cosWo,
                                      Float &pdf,
                                      Float &revPdf) const {
    ::Evaluate<true>(twoSided, this, wi, normal, wo, st, contrib, cosWo, pdf, revPdf);

    if (std::isnan(contrib.sum()))
        std::cout << "roughconductor Adjoint contrib:" << contrib.transpose() << std::endl;
}

template <bool adjoint>
bool Sample(
            const bool twoSided,
            const RoughConductor *bsdf,
            const Vector3 &wi,
            const Vector3 &normal,
            const Vector2 st,
            const Vector2 rndParam,
            const Float /*uDiscrete*/,
            // const Float uDiscrete,
            Vector3 &wo,
            Vector3 &contrib,
            Float &cosWo,
            Float &pdf,
            Float &revPdf) {


    Vector3 wi_ = wi;
    Float cosWi = Dot(wi, normal);

    Vector3 normal_ = normal;
    if (cosWi < Float(0.0)) {
        if (twoSided) {
            cosWi = -cosWi;
            normal_ = -normal_;
            // wi_[2] = -wi_[2];  // just flip the z component
            // flipped = true;
        } else {
            return false;
        }
    }
    
    if ( (fabs(cosWi) < c_CosEpsilon) || cosWi < 0.f) {
        return false;
    }
    Float alp = bsdf->alpha->Eval(st)[0];
    Float scaledAlp = alp * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWi)));
    Float mPdf;
    Vector3 localH = SampleMicronormal(rndParam, scaledAlp, mPdf);
    // pdf = mPdf;
    Vector3 b0;
    Vector3 b1;
    CoordinateSystem(normal_, b0, b1);
    Vector3 H = localH[0] * b0 + localH[1] * b1 + localH[2] * normal_;
    Float cosHWi = Dot(wi_, H);
    if (fabs(cosHWi) < c_CosEpsilon) {
        return false;
    }
    
    // Float F = FresnelConductorExact(cosHWi, bsdf->eta);
    Float F = FresnelConductorExact(cosHWi, bsdf->eta, bsdf->k);
    
    Vector3 refl;
    Float cosHWo;

    wo = Reflect(wi_, H);
    cosWo = Dot(wo, normal_);
    // side check
    if (F <= Float(0.0) || ((cosWo * cosWi) <= Float(0.0))) {
        return false;
    }
    refl = F * bsdf->Ks->Eval(st);
    cosHWo = Dot(wo, H);
    // pdf = fabs(mPdf * F/ (Float(4.0) * cosHWo));
    pdf = fabs(mPdf / (Float(4.0) * cosHWo));
    
    Float revCosHWo = cosHWi;
    Float rev_dwh_dwo = inverse(Float(4.0) * revCosHWo);
    
    
    if (fabs(cosWo) < c_CosEpsilon) {
        return false;
    }

    Float revScaledAlp = alp * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWo)));
    Float revD = BeckmennDistributionTerm(localH, revScaledAlp, revScaledAlp);

    // revPdf = fabs(F * revD * localH[2] * rev_dwh_dwo);
    revPdf = fabs( revD * localH[2] * rev_dwh_dwo);

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

    // std::cout << "F : " << F << ", D : " << D << ", G : " << G << ", cosWi : " << cosWi << ", localH : " << localH.transpose() << std::endl;
    Float numerator = D * G * cosHWi;
    Float denominator = mPdf * cosWi;
    contrib = refl * fabs(numerator / denominator);

    // std::cout << "refl : " << refl << ", num : " << numerator << ", den : " << denominator << std::endl;

    if ( cosWo * cosWi < 0.f)
        std::cout << " Stupid sampling!" << std::endl;
    // if ( flipped ) {
    //     wo[2] = -wo[2];
    // }
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
        twoSided, this, wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);

    if (std::isnan(contrib.sum()))
        std::cout << "Sample contrib:" << contrib.transpose() << std::endl;
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
        twoSided, this, wi, normal, st, rndParam, uDiscrete, wo, contrib, cosWo, pdf, revPdf);

    if (std::isnan(contrib.sum()))
        std::cout << "sample adjoint contrib:" << contrib.transpose() << std::endl;
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
    ADFloat eta, k;
    ADFloat alpha;
    buffer = Deserialize(buffer, Ks);
    buffer = Deserialize(buffer, eta);
    buffer = Deserialize(buffer, k);
    buffer = Deserialize(buffer, alpha);

    ADFloat cosWi = Dot(normal, wi);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(7);
    // assuming the following case was sampled because the bsdf is two-sided
    BeginIf(Gt(cosWi, Float(0.0)), ret);
    { SetCondOutput({normal[0], normal[1], normal[2], cosWi, wi[0], wi[1], wi[2]}); }
    BeginElse();
    { SetCondOutput({-normal[0], -normal[1], -normal[2], -cosWi, wi[0], wi[1], wi[2]}); }
    EndIf();
    ADVector3 normal_(ret[0], ret[1], ret[2]);
    cosWi = ret[3];
    ADVector3 wi_(ret[4], ret[5], ret[6]);

    cosWo = Dot(wo, normal_);
    
    ADVector3 H = Normalize(ADVector3(wi_ + wo));

    ADFloat cosHWi = Dot(wi_, H);
    ADFloat cosHWo = Dot(wo, H);
   
    ADVector3 b0;
    ADVector3 b1;
    CoordinateSystem(normal_, b0, b1);

    ADVector3 localH = ADVector3(Dot(b0, H), Dot(b1, H), Dot(normal_, H));
    ADFloat D = BeckmennDistributionTerm(localH, alpha, alpha);
    // This is confusing, but when computing reverse probability,
    // wo and wi are reversed
    ADFloat revCosHWi = cosHWo;
    ADFloat revCosHWo = cosHWi;
    ADFloat F = FresnelConductorExact(cosHWi, eta, k);
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
    // pdf = fabs(prob * F / (Float(4.f) * cosHWo));
    pdf = fabs(prob / (Float(4.f) * cosHWo));
    // revPdf = fabs(revProb * F / (Float(4.0) * revCosHWo));
    revPdf = fabs(revProb / (Float(4.0) * revCosHWo));
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
    ADFloat eta, k;
    ADFloat alpha;
    buffer = Deserialize(buffer, Ks);
    buffer = Deserialize(buffer, eta);
    buffer = Deserialize(buffer, k);
    buffer = Deserialize(buffer, alpha);

    ADFloat cosWi = Dot(normal, wi);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(7);
    BeginIf(Gt(cosWi, Float(0.0)), ret);
    { SetCondOutput({normal[0], normal[1], normal[2], cosWi, wi[0], wi[1], wi[2]}); }
    BeginElse();
    { SetCondOutput({-normal[0], -normal[1], -normal[2], -cosWi, wi[0], wi[1], wi[2]}); }
    EndIf();
    ADVector3 normal_(ret[0], ret[1], ret[2]);
    cosWi = ret[3];
    ADVector3 wi_(ret[4], ret[5], ret[6]);

    ADFloat scaledAlpha = alpha * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWi)));
    ADFloat mPdf;
    ADVector3 localH = SampleMicronormal(rndParam, scaledAlpha, mPdf);
    ADVector3 b0;
    ADVector3 b1;
    CoordinateSystem(normal_, b0, b1);
    ADVector3 H = localH[0] * b0 + localH[1] * b1 + localH[2] * normal_;
    ADFloat cosHWi = Dot(wi_, H);

    ADFloat F = FresnelConductorExact(cosHWi, eta, k);
    
    wo = Reflect(wi_, H);
    ADVector3 refl = Ks;
    refl *= F;
    ADFloat cosHWo = Dot(wo, H);
    // pdf = fabs(mPdf * F / (Float(4.0) * cosHWo));
    pdf = fabs(mPdf / (Float(4.0) * cosHWo));
    ADFloat revCosHWo = cosHWi;
    ADFloat rev_dwh_dwo = inverse(Float(4.0) * revCosHWo);
    cosWo = Dot(wo, normal_);
    ADFloat revScaledAlp = alpha * (Float(1.2) - Float(0.2) * sqrt(fabs(cosWo)));
    ADFloat revD = BeckmennDistributionTerm(localH, revScaledAlp, revScaledAlp);
    // revPdf = fabs(F * revD * localH[2] * rev_dwh_dwo);
    revPdf = fabs(revD * localH[2] * rev_dwh_dwo);
        
    ADFloat aCosWi = fabs(cosWi);
    ADFloat aCosWo = fabs(cosWo);
    ADFloat D = BeckmennDistributionTerm(localH, alpha, alpha);
    ADFloat G = BeckmennGeometryTerm(alpha, aCosWi, aCosWo);
    ADFloat numerator = D * G * cosHWi;
    ADFloat denominator = mPdf * cosWi;
    contrib = refl * fabs(numerator / denominator);

}