#include "collimatedlight.h"
#include "shape.h"
#include "utils.h"
#include "sampling.h"
#include "transform.h"

int GetCollimatedLightSerializedSize() {
    return 1 +                            // type
            16 +     // toWorld
           16 +     // toLight
           1 +                              // radius
           3;                             // emission
}

CollimatedLight::CollimatedLight(const Float &samplingWeight,
            const Matrix4x4 &toWorld, 
            const Float &_radius, 
            const Vector3 &emission)
    : Light(samplingWeight),
      toWorld(toWorld), 
      toLight((toWorld.inverse())), 
      m_radius(_radius), emission(emission) {}

void CollimatedLight::Serialize(const LightPrimID &lPrimID, const Vector2 &rndDir, Float *buffer) const {
    // std::cout << "Serializing Collimated light!" << std::endl;
    buffer = ::Serialize((Float)LightType::CollimatedLight, buffer);
    buffer = ::Serialize(toWorld, buffer);
    buffer = ::Serialize(toLight, buffer);
    buffer = ::Serialize(m_radius, buffer);
    ::Serialize(emission, buffer);
}


template <typename FloatType>
void _SampleDirectCollimatedLight(const TMatrix4x4<FloatType> &toWorld,
                            const TMatrix4x4<FloatType> &toLight,
                            const FloatType radius, 
                             const TVector3<FloatType> &emission,
                             const TVector3<FloatType> &pos,
                             FloatType &dist,
                             TVector3<FloatType> &dirToLight,
                             TVector3<FloatType> &lightContrib,
                             FloatType &directPdf) {
    
    TVector3<FloatType> refLocal = XformPoint(toLight, pos);

    FloatType surfaceArea = radius * radius * FloatType(M_PI);
    TVector2<FloatType> planeProjection(refLocal[0], refLocal[1]);
    if ( Length(planeProjection) > radius || refLocal[2] < FloatType(0.f)) {
        directPdf = FloatType(0.f);
        lightContrib.setZero();
        // not setting dist, dirToLight
    } else {
        
        TVector3<FloatType> unitz(Const<FloatType>(0.f), Const<FloatType>(0.f), Const<FloatType>(1.f));
        dirToLight = -XformVector(toWorld, unitz);
        dist = refLocal[2];
        FloatType distSq = dist * dist;
        
        // f / p
        directPdf =  distSq; // * inverse(surfaceArea);
        lightContrib = emission * inverse(distSq); // * surfaceArea;  

        // directPdf =  inverse(surfaceArea);
        // lightContrib = emission * surfaceArea;
    }
}

bool CollimatedLight::SampleDirect(const BSphere & /*sceneSphere*/,
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
                             Float &emissionPdf) const {
    
    // std::cout << "SampleDirect !" << std::endl;
    // no prim associated in this light
    lPrimID = 0;
    _SampleDirectCollimatedLight(toWorld, toLight, m_radius, emission, pos, 
                    dist, dirToLight, contrib, directPdf);
    if( directPdf > Float(0.0)) {
        Float surfaceArea = m_radius * m_radius * Float(M_PI);
        cosAtLight = Float(1.f);
        emissionPdf = inverse(surfaceArea);
        // std::cout << "SampleDirect contrib:" << contrib.transpose() << std::endl;
        // std::cout << "Direct sample dir " << dirToLight.transpose() << std::endl;
        return true;
    } else 
    return false;
        
}

// void CollimatedLight::Emission(const BSphere & /*sceneSphere*/,
//                          const Vector3 &dirToLight,
//                          const Vector3 &normalOnLight,
//                          const Float time,
//                          LightPrimID &lPrimID,
//                          Vector3 &emission,
//                          Float &directPdf,
//                          Float &emissionPdf) const {
//     lPrimID = 0;
//     Float surfaceArea = m_radius * m_radius * Float(M_PI);
//     Float cosAtLight = -Dot(normalOnLight, dirToLight);
//     if (cosAtLight > (1-c_CosEpsilon)) {
//         emission = this->emission;
//         directPdf = inverse(surfaceArea);
//         emissionPdf = directPdf;
//         // std::cout << "Emission contrib:" << emission.transpose() << std::endl;
        
//     } else {
//         emission = Vector3::Zero();
//         directPdf = Float(0.0);
//         emissionPdf = Float(0.0);
//     }
// }

void CollimatedLight::Emit(const BSphere & /*sceneSphere*/,
                     const Vector2 rndParamPos,
                     const Vector2 /*rndParamDir*/,
                     const Float time,
                     LightPrimID &lPrimID,
                     Ray &ray,
                     Vector3 &emission,
                     Float &cosAtLight,
                     Float &emissionPdf,
                     Float &directPdf) const {
    
    // std::cout << "EMIT method !" << std::endl;
    lPrimID = 0;
    cosAtLight = directPdf = Float(1.f);
    
    Vector2 samplePoint = SampleConcentricDisc(rndParamPos) * m_radius;
    Vector3 localPointOnLight(samplePoint[0], samplePoint[1], 0.f);
    ray.org = XformPoint(toWorld, localPointOnLight);
    ray.dir = XformVector(toWorld, Vector3(0.f, 0.f, 1.f));

    Float surfaceArea = m_radius * m_radius * Float(M_PI);
    emissionPdf = inverse(surfaceArea);
    emission = this->emission / emissionPdf;
    directPdf = Float(1.0);

    // std::cout << "Emit : " 
    //         << "o:" << ray.org.transpose() 
    //         << ", d:" << ray.dir.transpose() 
    //         << ", c:" << emission.transpose()
    //         << ", emitpdf:" << emissionPdf
    //         << ", directPdf:" << directPdf << std::endl;
}

void SampleDirectCollimatedLight(const ADFloat *buffer,
                           const ADBSphere & /*sceneSphere*/,
                           const ADVector3 &pos,
                           const ADVector3 & /*normal*/,
                           const ADVector2 rndParam,
                           const ADFloat time,
                           const bool /*isStatic*/,
                           ADVector3 &dirToLight,
                           ADVector3 &lightContrib,
                           ADFloat &cosAtLight,
                           ADFloat &directPdf,
                           ADFloat &emissionPdf) {
    
    ADMatrix4x4 toWorld, toLight;
    ADFloat radius;
    ADVector3 emission;
    
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, radius);
    Deserialize(buffer, emission);
    
    ADFloat dist;
    // _SampleDirectCollimatedLight(toWorld, toLight, radius, emission, pos, 
    //                 dist, dirToLight, lightContrib, directPdf);

    // ADMatrix4x4 traoW = Interpolate(toWorld, time),
    //     traoL = Interpolate(toLight, time);

    ADVector3 refLocal = XformPoint(toLight, pos);

    ADVector2 planeProjection(refLocal[0], refLocal[1]);
    BooleanCPtr valid = And( 
                        Lt( Length(planeProjection), radius ),
                        Gt(refLocal[2], Const<ADFloat>(0.f)) 
    );

    ADFloat surfaceArea = radius * radius * Const<ADFloat>(M_PI);
    ADFloat zeroVec = Const<ADFloat>(0.f);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(10);
    // following if is flipped
    BeginIf ( valid, ret);
    {
        
        ADVector3 unitz(zeroVec, zeroVec, Const<ADFloat>(1.f));
        ADVector3 dirToLight_ = -XformVector(toWorld, unitz);
        ADFloat dist_ = refLocal[2];
        ADVector3 lightContrib_ = emission * inverse(dist_ * dist_); // * surfaceArea;

        ADFloat pdf_ = dist_ * dist_; // * inverse(surfaceArea);
        ADFloat cosAtLight_ = Const<ADFloat>(1.f);
        ADFloat emissionPdf_ = inverse(surfaceArea);
        SetCondOutput({
            pdf_,
            lightContrib_[0], lightContrib_[1], lightContrib_[2],
            dirToLight_[0], dirToLight_[1], dirToLight_[2],
            dist_, cosAtLight_, emissionPdf_
        });
    } 
    BeginElse(); 
    {
        SetCondOutput({
            zeroVec, 
            zeroVec, zeroVec, zeroVec,
            zeroVec, zeroVec, zeroVec,
            zeroVec, zeroVec, zeroVec
            });
    }
    EndIf();

    directPdf = ret[0];
    lightContrib = ADVector3(ret[1], ret[2], ret[3]);
    dirToLight = ADVector3(ret[4], ret[5], ret[6]);
    dist = ret[7];

    
    cosAtLight = ret[8];
    emissionPdf = ret[9];
}

// void EmissionCollimatedLight(const ADFloat *buffer,
//                        const ADBSphere &sceneSphere,
//                        const ADVector3 &dirToLight,
//                        const ADVector3 &normalOnLight,
//                        const ADFloat time,
//                        ADVector3 &emission,
//                        ADFloat &directPdf,
//                        ADFloat &emissionPdf) {
//     ADAnimatedTransform toWorld, toLight;
//     ADFloat radius;
//     ADVector3 emission_;
    
//     buffer = Deserialize(buffer, toWorld);
//     buffer = Deserialize(buffer, toLight);
//     buffer = Deserialize(buffer, radius);
//     Deserialize(buffer, emission_);

//     ADFloat cosAtLight = -Dot(normalOnLight, dirToLight);
//     ADFloat surfaceArea = radius * radius * Const<ADFloat>(M_PI);

//     std::vector<CondExprCPtr> ret = CreateCondExprVec(4);
//     BeginIf(Gt(cosAtLight, Float(1.f - c_CosEpsilon)), ret);
//     {
//         SetCondOutput({emission_[0], emission_[1], emission_[2], inverse(surfaceArea)});
//     }
//     BeginElse();
//     {
//         SetCondOutput({Const<ADFloat>(0.f), Const<ADFloat>(0.f), Const<ADFloat>(0.f), Const<ADFloat>(0.f)});
//     }
//     EndIf();
    
//     emission = ADVector3(ret[0], ret[1], ret[2]);
//     directPdf = ret[3];
//     emissionPdf = directPdf;
// }

void EmitCollimatedLight(const ADFloat *buffer,
                   const ADBSphere &sceneSphere,
                   const ADVector2 rndParamPos,
                   const ADVector2 /*rndParamDir*/,
                   const ADFloat time,
                   const bool /*isStatic*/,
                   ADRay &ray,
                   ADVector3 &emission,
                   ADFloat &cosAtLight,
                   ADFloat &emissionPdf,
                   ADFloat &directPdf) {

    ADMatrix4x4 toWorld, toLight;
    ADFloat radius;
    ADVector3 _emission;
    
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, radius);
    Deserialize(buffer, _emission);

    cosAtLight = Const<ADFloat>(1.0);
    
    // ADMatrix4x4 traoW = Interpolate(toWorld, time);
    
    ADVector2 samplePoint = SampleConcentricDisc(rndParamPos) * radius;
    ADVector3 localPointOnLight(samplePoint[0], samplePoint[1], Const<ADFloat>(0.f));
    ray.org = XformPoint(toWorld, localPointOnLight);

    ray.dir = XformVector(toWorld, 
            ADVector3(Const<ADFloat>(0.f), Const<ADFloat>(0.f), Const<ADFloat>(1.f)));

    ADFloat surfaceArea = radius * radius * Const<ADFloat>(M_PI);
    emissionPdf = inverse(surfaceArea);
    emission = _emission / emissionPdf;
    // directPdf = inverse(surfaceArea);
    directPdf =  Const<ADFloat>(1.0);
}
