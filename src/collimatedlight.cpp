#include "collimatedlight.h"
#include "shape.h"
#include "utils.h"
#include "sampling.h"
#include "animatedtransform.h"

int GetCollimatedLightSerializedSize() {
    return 1 +                            // type
            16 +     // toWorld
           16 +     // toLight
           1 +                              // radius
           3;                             // emission
}

CollimatedLight::CollimatedLight(const Float &samplingWeight,
            const AnimatedTransform &toWorld, 
            const Float &_radius, 
            const Vector3 &emission)
    : Light(samplingWeight),
      toWorld(toWorld), 
      toLight(Invert(toWorld)), 
      m_radius(_radius), emission(emission) {}

void CollimatedLight::Serialize(const LightPrimID &lPrimID, Float *buffer) const {
    buffer = ::Serialize((Float)LightType::CollimatedLight, buffer);
    buffer = ::Serialize(toWorld, buffer);
    buffer = ::Serialize(toLight, buffer);
    buffer = ::Serialize(m_radius, buffer);
    ::Serialize(emission, buffer);
}


template <typename FloatType>
void _SampleDirectCollimatedLight(const TAnimatedTransform<FloatType> &toWorld,
                            const TAnimatedTransform<FloatType> &toLight,
                            const FloatType radius, 
                             const TVector3<FloatType> &emission,
                             const TVector3<FloatType> &pos,
                             FloatType &dist,
                             TVector3<FloatType> &dirToLight,
                             TVector3<FloatType> &lightContrib,
                             FloatType &directPdf) {
    FloatType time = Const<FloatType>(0.f);
    TMatrix4x4<FloatType> traoW = Interpolate(toWorld, time),
        traoL = Interpolate(toLight, time);

    TVector3<FloatType> refLocal = XformPoint(traoL, pos);

    FloatType surfaceArea = radius * radius * FloatType(M_PI);
    TVector2<FloatType> planeProjection(refLocal[0], refLocal[1]);
    if ( Length(planeProjection) > radius || refLocal[2] < FloatType(0.f)) {
        directPdf = FloatType(0.f);
        lightContrib.setZero();
        // not setting dist, dirToLight
    } else {
        
        TVector3<FloatType> unitz(Const<FloatType>(0.f), Const<FloatType>(0.f), Const<FloatType>(1.f));
        dirToLight = -XformVector(traoW, unitz);
        dist = refLocal[2];
        FloatType distSq = dist * dist;
        lightContrib = emission * inverse(distSq) * surfaceArea;  
        directPdf =  distSq * inverse(surfaceArea);
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
    
    // no prim associated in this light
    lPrimID = 0;
    _SampleDirectCollimatedLight(toWorld, toLight, m_radius, emission, pos, 
                    dist, dirToLight, contrib, directPdf);
    if( directPdf > Float(0.0)) {
        cosAtLight = Float(1.f);
        emissionPdf = directPdf;
        // std::cout << "SampleDirect contrib:" << contrib.transpose() << std::endl;
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
    
    lPrimID = 0;
    cosAtLight = directPdf = Float(1.f);
    Matrix4x4 traoW = Interpolate(toWorld, time);

    Vector2 samplePoint = SampleConcentricDisc(rndParamPos) * m_radius;
    Vector3 localPointOnLight(samplePoint[0], samplePoint[1], 0.f);
    ray.org = XformPoint(traoW, localPointOnLight);
    ray.dir = XformVector(traoW, Vector3(0.f, 0.f, 1.f));

    Float surfaceArea = m_radius * m_radius * Float(M_PI);
    emission = this->emission * surfaceArea;
    emissionPdf = inverse(surfaceArea);
    directPdf = inverse(surfaceArea);

    // std::cout << "Emit : " 
    //         << "o:" << ray.org 
    //         << ", d:" << ray.dir 
    //         << ", c:" << emission
    //         << ", pdf:" << emissionPdf
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
    
    ADAnimatedTransform toWorld, toLight;
    ADFloat radius;
    ADVector3 emission;
    
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, radius);
    Deserialize(buffer, emission);
    
    ADFloat dist;
    // _SampleDirectCollimatedLight(toWorld, toLight, radius, emission, pos, 
    //                 dist, dirToLight, lightContrib, directPdf);

    ADMatrix4x4 traoW = Interpolate(toWorld, time),
        traoL = Interpolate(toLight, time);

    ADVector3 refLocal = XformPoint(traoL, pos);

    ADVector2 planeProjection(refLocal[0], refLocal[1]);
    BooleanCPtr valid = And( 
                        Lt( Length(planeProjection), radius ),
                        Gt(refLocal[2], Const<ADFloat>(0.f)) 
    );

    ADFloat surfaceArea = radius * radius * Const<ADFloat>(M_PI);
    ADFloat zeroVec = Const<ADFloat>(0.f);
    std::vector<CondExprCPtr> ret = CreateCondExprVec(8);
    // following if is flipped
    BeginIf ( valid, ret);
    {
        
        ADVector3 unitz(zeroVec, zeroVec, Const<ADFloat>(1.f));
        ADVector3 dirToLight_ = -XformVector(traoW, unitz);
        ADFloat dist_ = refLocal[2];
        ADVector3 lightContrib_ = emission * inverse(dist * dist) * surfaceArea;

        ADFloat pdf_ = dist * dist * inverse(surfaceArea);

        SetCondOutput({
            pdf_,
            lightContrib_[0], lightContrib_[1], lightContrib_[2],
            dirToLight_[0], dirToLight_[1], dirToLight_[2],
            dist_
        });
    } 
    BeginElse(); 
    {
        SetCondOutput({
            zeroVec, 
            zeroVec, zeroVec, zeroVec,
            zeroVec, zeroVec, zeroVec,
            zeroVec
            });
    }
    EndIf();

    directPdf = ret[0];
    lightContrib = ADVector3(ret[1], ret[2], ret[3]);
    dirToLight = ADVector3(ret[4], ret[5], ret[6]);
    dist = ret[7];
    
    cosAtLight = directPdf;
    emissionPdf = directPdf;
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

    ADAnimatedTransform toWorld, toLight;
    ADFloat radius;
    ADVector3 _emission;
    
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, radius);
    Deserialize(buffer, _emission);

    cosAtLight = Const<ADFloat>(1.0);
    
    ADMatrix4x4 traoW = Interpolate(toWorld, time);
    
    ADVector2 samplePoint = SampleConcentricDisc(rndParamPos) * radius;
    ADVector3 localPointOnLight(samplePoint[0], samplePoint[1], Const<ADFloat>(0.f));
    ray.org = XformPoint(traoW, localPointOnLight);

    ray.dir = XformVector(traoW, 
            ADVector3(Const<ADFloat>(0.f), Const<ADFloat>(0.f), Const<ADFloat>(1.f)));

    ADFloat surfaceArea = radius * radius * Const<ADFloat>(M_PI);
    emission = _emission * inverse(surfaceArea);
    emissionPdf = inverse(surfaceArea);
    directPdf = inverse(surfaceArea);
}
