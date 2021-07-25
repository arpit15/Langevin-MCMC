#include "ieslight.h"

#include "image.h"
#include "utils.h"
#include "sampling.h"
#include "animatedtransform.h"

// how do to serialize texture
int GetIESLightSerializedSize() {
    return 1 +  // type
           16 +     // toWorld
           16 +     // toLight
           3 +   // emission
           1;  // ies intensity evaluated along rndDir

}

IESLight::IESLight(const Float &samplingWeight, const Matrix4x4 &toWorld, const Vector3 &emission, const std::string fname)
    : Light(samplingWeight), 
    toWorld(toWorld), 
    toLight((toWorld.inverse())), 
    emission(emission), 
    image(new Image3(fname)) {
}

void IESLight::Serialize(const LightPrimID &lPrimID, const Vector2 &rndDir, Float *buffer) const {
    // std::cout << "Serializing IES light!" << std::endl;
    buffer = ::Serialize((Float)LightType::IESLight, buffer);
    buffer = ::Serialize(toWorld, buffer);
    buffer = ::Serialize(toLight, buffer);
    buffer = ::Serialize(emission, buffer);
    // evaluate ies using rndParam
    const Vector3 localDir = SampleSphere(rndDir);
    const Float iesVal = getIESVal(localDir);
    ::Serialize(iesVal, buffer);
}

Float IESLight::getIESVal(const Vector3 &local) const {
  Vector2 uv(
            std::atan2(local[1], local[0]) * c_INVTWOPI,
            std::acos(local[2]) * c_INVPI
        );

    Float phi = uv[0] * c_TWOPI,
        theta = uv[1] * c_PI;
        if(uv[0]<0.f) uv[0] = 1.f+uv[0];

    size_t col = std::floor(uv[0] * image->pixelWidth), 
        row = std::floor(uv[1] * image->pixelHeight);
    Float iesVal = image->RepAt(col, row).mean();
//   std::cout << "phi:" <<  phi*180.f/c_PI 
//             << ", theta:" << theta*180.f/c_PI 
//             << ", (" << row << "," << col << ")val : " << iesVal << std::endl;
  return iesVal;
}

template <typename FloatType>
void _SampleDirectIESLight(const TMatrix4x4<FloatType> &toWorld, 
                             const TVector3<FloatType> &emission,
                             const TVector3<FloatType> &pos,
                             const FloatType time, 
                             FloatType &dist,
                             TVector3<FloatType> &dirToLight,
                             TVector3<FloatType> &lightContrib,
                             FloatType &pdf) {
    

    TVector3<FloatType> origin;
    origin << Const<FloatType>(0.f), Const<FloatType>(0.f), Const<FloatType>(0.f);
    // origin.setZero();

    TVector3<FloatType> lightPos = XformPoint(toWorld, origin);
    dirToLight = lightPos - pos;
    const FloatType distSq = LengthSquared(dirToLight);
    pdf = distSq;
    dist = sqrt(distSq);
    dirToLight = dirToLight / dist;

    
    // const Vector3 local = XformVector(trao, FloatType(-1.f) * dirToLight);
    lightContrib = emission * inverse(distSq);  // returns without ies
}

bool IESLight::SampleDirect(const BSphere & /*sceneSphere*/,
                              const Vector3 &pos,
                              const Vector3 & /*normal*/,
                              const Vector2 /*rndParam*/,
                              const Float time,
                              LightPrimID &lPrimID,
                              Vector3 &dirToLight,
                              Float &dist,
                              Vector3 &contrib,
                              Float &cosAtLight,
                              Float &directPdf,
                              Float &emissionPdf) const {
    _SampleDirectIESLight(toWorld, emission, pos, time, dist, dirToLight, contrib, directPdf);

    const Vector3 dirFromLight = -dirToLight;
    const Vector3 local = XformVector(toLight, dirFromLight);
    contrib *= getIESVal(local);
    assert(dist > Float(0.0));
    emissionPdf = c_INVFOURPI;
    cosAtLight = Float(1.0);
    lPrimID = 0;
    return true;
    // AD version is wrong because the IES value will depend upon the pos instead of rndParams
    std::cout << "sample IES direct. AD version is wrong!" << std::endl;
}

void IESLight::Emit(const BSphere & /*sceneSphere*/,
                      const Vector2 /*rndParamPos*/,
                      const Vector2 rndParamDir,
                      const Float time,
                      LightPrimID & /*lPrimID*/,
                      Ray &ray,
                      Vector3 &emission,
                      Float &cosAtLight,
                      Float &emissionPdf,
                      Float &directPdf) const {

    Vector3 origin;
    origin.setZero();
    Vector3 lightPos = XformPoint(toWorld, origin);

    const Vector3 local = SampleSphere(rndParamDir);

    ray.org = lightPos;
    ray.dir = XformVector(toWorld, local);

    emission = this->emission * getIESVal(local);
    emissionPdf = c_INVFOURPI;
    cosAtLight = directPdf = Float(1.0);
}

void SampleDirectIESLight(const ADFloat *buffer,
                            const ADBSphere & /*sceneSphere*/,
                            const ADVector3 &pos,
                            const ADVector3 & /*normal*/,
                            const ADVector2 /*rndParam*/,
                            const ADFloat time,
                            const bool /*isStatic*/,
                            ADVector3 &dirToLight,
                            ADVector3 &lightContrib,
                            ADFloat &cosAtLight,
                            ADFloat &directPdf,
                            ADFloat &emissionPdf) {
    ADVector3 lightPos, emission;
    ADMatrix4x4 toWorld, toLight;
    ADFloat iesVal;

    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, emission);
    Deserialize(buffer, iesVal);
    

    ADFloat dist;

    _SampleDirectIESLight(toWorld, emission, pos, time, dist, dirToLight, lightContrib, directPdf);
    // Hack : ideally iesVal is affected by changing rndParamDir
    // incorrect
    // however the contribution of these paths should be negligible
    lightContrib *= iesVal;
    emissionPdf = Const<ADFloat>(c_INVFOURPI);
    cosAtLight = Const<ADFloat>(1.0);
    
}

void EmitIESLight(const ADFloat *buffer,
                    const ADBSphere &sceneSphere,
                    const ADVector2 rndParamPos,
                    const ADVector2 rndParamDir,
                    const ADFloat time,
                    const bool /*isStatic*/,
                    ADRay &ray,
                    ADVector3 &emission,
                    ADFloat &cosAtLight,
                    ADFloat &emissionPdf,
                    ADFloat &directPdf) {
    ADVector3 lightPos, emission_;
    ADMatrix4x4 toWorld, toLight;
    ADFloat iesVal;
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    buffer = Deserialize(buffer, emission_);
    Deserialize(buffer, iesVal);

    ADVector3 origin; origin.setZero();
    
    lightPos = XformPoint(toWorld, origin);
    ray.org = lightPos;
    ray.dir = XformVector(toWorld, SampleSphere(rndParamDir));
    // Hack : ideally iesVal is affected by changing rndParamDir
    emission = emission_ * iesVal;
    emissionPdf = Const<ADFloat>(c_INVFOURPI);
    cosAtLight = Const<ADFloat>(1.0);
    directPdf = Const<ADFloat>(1.0);
}
