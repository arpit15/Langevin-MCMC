#pragma once

#include "commondef.h"
#include "utils.h"

template <typename FloatType>
FloatType BeckmennDistributionTerm(const TVector3<FloatType> &localH,
                                   const FloatType &alphaU,
                                   const FloatType &alphaV) {
    const FloatType cosTheta = localH[2];
    const FloatType mu = localH[0];
    const FloatType mv = localH[1];
    FloatType cosTheta2 = square(cosTheta);
    FloatType beckmannExponent =
        (square(mu) / square(alphaU) + square(mv) / square(alphaV)) / cosTheta2;
    FloatType D = exp(-beckmannExponent) / (c_PI * alphaU * alphaV * square(cosTheta2));
    return D;
}

inline Float BeckmennGeometryTerm(const Float alpha, const Float cosTheta) {
    Float tanTheta = sqrt(fabs(Float(1.0) - square(cosTheta))) / cosTheta;
    if (tanTheta <= 0.0)
        return Float(1.0);
    Float G = 0.0;
    Float a = Float(1.0) / (alpha * tanTheta);
    if (a >= Float(1.6)) {
        G = Float(1.0);
    } else {
        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        // Note: this could be a bad approximation for derivatives,
        //       ideally we should refit the function
        Float aSqr = a * a;
        G = (Float(3.535) * a + Float(2.181) * aSqr) /
            (Float(1.0) + Float(2.276) * a + Float(2.577) * aSqr);
    }
    return G;
}

inline ADFloat BeckmennGeometryTerm(const ADFloat alpha, const ADFloat cosTheta) {
    ADFloat tanTheta = sqrt(fabs((Float(1.0) + Float(1e-6)) - square(cosTheta))) / cosTheta;
    CondExprCPtr ret = CondExpr::Create();
    BeginIf(Lte(tanTheta, Float(0.0)), {ret});
    { SetCondOutput({Const<ADFloat>(1.0)}); }
    BeginElse();
    {
        CondExprCPtr G = CondExpr::Create();
        ADFloat a = inverse(alpha * tanTheta);
        BeginIf(Gte(a, Float(1.6)), {G});
        { SetCondOutput({Const<ADFloat>(1.0)}); }
        BeginElse();
        {
            ADFloat aSqr = square(a);
            ADFloat G_ = (Float(3.535) * a + Float(2.181) * aSqr) /
                         (Float(1.0) + Float(2.276) * a + Float(2.577) * aSqr);
            SetCondOutput({G_});
        }
        EndIf();
        SetCondOutput({G});
    }
    EndIf();
    return ret;
}

template <typename FloatType>
FloatType BeckmennGeometryTerm(const FloatType alpha,
                               const FloatType cosWi,
                               const FloatType cosWo) {
    return BeckmennGeometryTerm(alpha, cosWi) * BeckmennGeometryTerm(alpha, cosWo);
}

inline Float FresnelDielectricExt(const Float cosThetaI_,
                                  Float &cosThetaT_,
                                  const Float eta,
                                  const Float invEta) {
    Float scale = (cosThetaI_ > 0) ? invEta : eta;
    Float cosThetaTSqr = Float(1.0) - (Float(1.0) - square(cosThetaI_)) * square(scale);

    if (cosThetaTSqr <= Float(0.0)) {
        cosThetaT_ = Float(0.0);
        return Float(1.0);
    }

    Float cosThetaI = fabs(cosThetaI_);
    Float cosThetaT = sqrt(cosThetaTSqr);

    Float Rs = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    Float Rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

    cosThetaT_ = (cosThetaI_ > 0) ? -cosThetaT : cosThetaT;

    return Float(0.5) * (square(Rs) + square(Rp));
}

inline Float FresnelDielectricExt(const Float cosThetaI_, const Float eta, const Float invEta) {
    Float scale = (cosThetaI_ > 0) ? invEta : eta;
    Float cosThetaTSqr = Float(1.0) - (Float(1.0) - square(cosThetaI_)) * square(scale);

    if (cosThetaTSqr <= Float(0.0)) {
        return Float(1.0);
    }

    Float cosThetaI = fabs(cosThetaI_);
    Float cosThetaT = sqrt(cosThetaTSqr);

    Float Rs = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    Float Rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

    return Float(0.5) * (square(Rs) + square(Rp));
}

inline ADFloat FresnelDielectricExt(const ADFloat cosThetaI_,
                                    ADFloat &cosThetaT_,
                                    const ADFloat eta,
                                    const ADFloat invEta) {
    ADFloat scale = if_else(Gt(cosThetaI_, Float(0.0)), invEta, eta);
    ADFloat cosThetaTSqr = Float(1.0) - (Float(1.0) - square(cosThetaI_)) * square(scale);

    std::vector<CondExprCPtr> ret = CreateCondExprVec(2);
    BeginIf(Lte(cosThetaTSqr, Float(0.0)), ret);
    { SetCondOutput({Const<ADFloat>(0.0), Const<ADFloat>(1.0)}); }
    BeginElse();
    {
        ADFloat cosThetaI = fabs(cosThetaI_);
        ADFloat cosThetaT = sqrt(cosThetaTSqr);
        ADFloat etaCosThetaT = eta * cosThetaT;
        ADFloat etaCosThetaI = eta * cosThetaI;
        ADFloat Rs = (cosThetaI - etaCosThetaT) / (cosThetaI + etaCosThetaT);
        ADFloat Rp = (etaCosThetaI - cosThetaT) / (etaCosThetaI + cosThetaT);

        ADFloat cosThetaT_ = if_else(Gt(cosThetaI_, Float(0.0)), -cosThetaT, cosThetaT);
        ADFloat F = Float(0.5) * (square(Rs) + square(Rp));
        SetCondOutput({cosThetaT_, F});
    }
    EndIf();

    cosThetaT_ = ret[0];
    return ret[1];
}

inline ADFloat FresnelDielectricExt(const ADFloat cosThetaI_,
                                    const ADFloat eta,
                                    const ADFloat invEta) {
    ADFloat scale = if_else(Gt(cosThetaI_, Float(0.0)), invEta, eta);
    ADFloat cosThetaTSqr = Float(1.0) - (Float(1.0) - square(cosThetaI_)) * square(scale);

    std::vector<CondExprCPtr> ret = CreateCondExprVec(1);
    BeginIf(Lte(cosThetaTSqr, Float(0.0)), ret);
    { SetCondOutput({Const<ADFloat>(1.0)}); }
    BeginElse();
    {
        ADFloat cosThetaI = fabs(cosThetaI_);
        ADFloat cosThetaT = sqrt(cosThetaTSqr);
        ADFloat etaCosThetaT = eta * cosThetaT;
        ADFloat etaCosThetaI = eta * cosThetaI;
        ADFloat Rs = (cosThetaI - etaCosThetaT) / (cosThetaI + etaCosThetaT);
        ADFloat Rp = (etaCosThetaI - cosThetaT) / (etaCosThetaI + cosThetaT);

        ADFloat F = Float(0.5) * (square(Rs) + square(Rp));
        SetCondOutput({F});
    }
    EndIf();
    return ret[0];
}


inline Float FresnelConductorExact(const Float cosThetaI_, const Float eta, const Float k) {
    Float cosThetaI2 = square(cosThetaI_),
        sinThetaI2 = 1.f - cosThetaI2,
        sinThetaI4 = sinThetaI2 * sinThetaI2;

    Float temp1 = eta*eta - k*k - sinThetaI2,
          a2pb2 = sqrt(temp1*temp1 + 4*k*k*eta*eta),
          a     = sqrt(0.5f * (a2pb2 + temp1));

    Float term1 = a2pb2 + cosThetaI2,
          term2 = 2.f*a*cosThetaI_;

    Float Rs2 = (term1 - term2) / (term1 + term2);

    Float term3 = a2pb2*cosThetaI2 + sinThetaI4,
          term4 = term2*sinThetaI2;

    Float Rp2 = Rs2 * (term3 - term4) / (term3 + term4);

    return 0.5f * (Rp2 + Rs2);
}

inline ADFloat FresnelConductorExact(const ADFloat cosThetaI_, const ADFloat eta, const ADFloat k) {
    ADFloat cosThetaI2 = Float(1.f) * square(cosThetaI_),
        sinThetaI2 = Float(1.f) - cosThetaI2,
        sinThetaI4 = sinThetaI2 * sinThetaI2;

    ADFloat temp1 = eta*eta - k*k - sinThetaI2,
          a2pb2 = sqrt(temp1*temp1 + Float(4.f)*k*k*eta*eta),
          a     = sqrt( Float(0.5f) * (a2pb2 + temp1));

    ADFloat term1 = a2pb2 + cosThetaI2,
          term2 = Float(2.f)*a*cosThetaI_;

    ADFloat Rs2 = (term1 - term2) / (term1 + term2);

    ADFloat term3 = a2pb2*cosThetaI2 + sinThetaI4,
          term4 = term2*sinThetaI2;

    ADFloat Rp2 = Rs2 * (term3 - term4) / (term3 + term4);

    return Float(0.5f) * (Rp2 + Rs2);
}

inline Vector3 FresnelConductorExact(const Float cosThetaI_, const Vector3 eta, const Vector3 k) {
    Float cosThetaI2 = square(cosThetaI_),
        sinThetaI2 = 1.f - cosThetaI2,
        sinThetaI4 = sinThetaI2 * sinThetaI2;

    Vector3 temp1 = eta.array()*eta.array() - k.array()*k.array() - (sinThetaI2),
          a2pb2_sq = (temp1.array()*temp1.array() + 4.f*k.array()*k.array()*eta.array()*eta.array());
          

    Vector3 a2pb2(sqrt(a2pb2[0]), sqrt(a2pb2[1]), sqrt(a2pb2[2]));

    Vector3 a_sq     = (0.5f * (a2pb2 + temp1));
    Vector3 a(sqrt(a_sq[0]), sqrt(a_sq[1]), sqrt(a_sq[2]));

    Vector3 term1 = a2pb2 + Vector3(cosThetaI2, cosThetaI2, cosThetaI2),
          term2 = 2.f*a.array()*cosThetaI_;

    Vector3 Rs2 = (term1 - term2).array() / (term1 + term2).array();

    Vector3 term3 = a2pb2*(cosThetaI2) + Vector3(sinThetaI4, sinThetaI4, sinThetaI4),
          term4 = term2*sinThetaI2;

    Vector3 Rp2 = Rs2.array() * (term3 - term4).array() / (term3 + term4).array();

    return 0.5f * (Rp2 + Rs2);
}

inline ADVector3 FresnelConductorExact(const ADFloat cosThetaI_, const ADVector3 eta, const ADVector3 k) {
    ADFloat cosThetaI2 = Float(1.f) * square(cosThetaI_),
        sinThetaI2 = Float(1.f) - cosThetaI2,
        sinThetaI4 = sinThetaI2 * sinThetaI2;

    ADVector3 temp1 = eta*eta - k*k - ADVector3(sinThetaI2, sinThetaI2, sinThetaI2),
          a2pb2_sq = (temp1*temp1 + Const<ADFloat>(4.f)*k*k*eta*eta);

        
    ADVector3 a2pb2(sqrt(a2pb2_sq[0]), sqrt(a2pb2_sq[1]), sqrt(a2pb2_sq[2]));

    ADVector3 a_sq     = ( Const<ADFloat>(0.5f) * (a2pb2 + temp1).array());
    ADVector3 a(sqrt(a_sq[0]), sqrt(a_sq[1]), sqrt(a_sq[2]));

    ADVector3 term1 = a2pb2 + ADVector3(cosThetaI2, cosThetaI2, cosThetaI2),
          term2 = Const<ADFloat>(2.f)*a*ADVector3(cosThetaI_, cosThetaI_, cosThetaI_);

    ADVector3 Rs2 = (term1 - term2).array() / (term1 + term2).array();

    ADVector3 term3 = a2pb2*ADVector3(cosThetaI2, cosThetaI2, cosThetaI2) + ADVector3(sinThetaI4, sinThetaI4, sinThetaI4),
          term4 = term2*ADVector3(sinThetaI2, sinThetaI2, sinThetaI2);

    ADVector3 Rp2 = Rs2.array() * (term3 - term4).array() / (term3 + term4).array();

    return Const<ADFloat>(0.5f) * (Rp2 + Rs2);
}

template <typename FloatType>
TVector3<FloatType> SampleMicronormal(const TVector2<FloatType> rndParam,
                                      const FloatType alpha,
                                      FloatType &pdfW) {
    FloatType phiM = c_TWOPI * rndParam[1];
    FloatType sinPhiM = sin(phiM);
    FloatType cosPhiM = cos(phiM);
    FloatType alphaSqr = square(alpha);

    FloatType tanThetaMSqr = alphaSqr * (-log(fmax(Float(1.0) - rndParam[0], Float(1e-6))));
    FloatType cosThetaM = Float(1.0) / sqrt(Float(1.0) + tanThetaMSqr);
    FloatType cosThetaMSqr = square(cosThetaM);

    pdfW = (Float(1.0) - rndParam[0]) / (c_PI * alphaSqr * cosThetaM * cosThetaMSqr);
    FloatType sinThetaMSq = fmax(Float(1.0) - cosThetaMSqr, ADEpsilon<FloatType>());
    FloatType sinThetaM = sqrt(sinThetaMSq);

    TVector3<FloatType> localH(sinThetaM * cosPhiM, sinThetaM * sinPhiM, cosThetaM);
    return localH;
}
