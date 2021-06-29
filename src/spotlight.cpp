#include "spotlight.h"
#include "utils.h"
#include "sampling.h"
#include "transform.h"

int GetSpotLightSerializedSize() {
    return 1 +  // type
           16 +     // toWorld
           16 +     // toLight
           3 +   // emission
           1 + // cutoffangle
           1;  // beamWidth
}

SpotLight::SpotLight(const Float &samplingWeight, 
                const AnimatedTransform &toWorld, 
                const Vector3 &emission,
                const Float &cutoffangle, 
                const Float &beamWidth)
    : Light(samplingWeight), 
      toWorld(toWorld), 
      toLight(Invert(toWorld)), 
      emission(emission),
      cutoffAngle(cutoffAngle),
      beamWidth(beamWidth) {

        cosCutoffAngle = cos(cutoffAngle);
        cosBeamWidth = cos(beamWidth);
        uvFactor = tan(cutoffAngle);
        invTransitionWidth = 1.0f / (cutoffAngle - beamWidth);
}

void SpotLight::Serialize(const LightPrimID &lPrimID, const Vector2 &rndDir, Float *buffer) const {
    buffer = ::Serialize((Float)LightType::SpotLight, buffer);
    buffer = ::Serialize(toWorld, buffer);
    buffer = ::Serialize(toLight, buffer);
    buffer = ::Serialize(emission, buffer);
    buffer = ::Serialize(cutoffAngle, buffer);
    ::Serialize(beamWidth, buffer);
}

template <typename FloatType>
inline TVector3<FloatType> falloffCurve(const TVector3<FloatType> &d, 
                              const FloatType cutoffAngle,
                              const FloatType beamWidth) {
    const FloatType cosTheta = d[2];

    if (cosTheta <= cos(cutoffAngle))
        return TVector3<FloatType>::Zero();

    FloatType one = Const<FloatType>(1.f);
    TVector3<FloatType> result = TVector3<FloatType>(one, one, one);

    if (cosTheta >= cos(beamWidth))
        return result;

    FloatType invTransitionWidth = 1.0f / (cutoffAngle - beamWidth);  
    return result * ((cutoffAngle - acos(cosTheta))
            * invTransitionWidth);
}

template <typename FloatType>
void _SampleDirectSpotLight(const TAnimatedTransform<FloatType> &toWorld,
                            const TAnimatedTransform<FloatType> &toLight,
                             const TVector3<FloatType> &emission,
                             const FloatType cutoffAngle,
                             const FloatType beamWidth,
                             const TVector3<FloatType> &pos,
                             FloatType &dist,
                             TVector3<FloatType> &dirToLight,
                             TVector3<FloatType> &lightContrib,
                             FloatType &pdf) {
    
    FloatType time = Const<FloatType>(0.0);
    TMatrix4x4<FloatType> trao = Interpolate(toWorld, time);

    TVector3<FloatType> origin = TVector3<FloatType>::Zero();
    TVector3<FloatType> lightPos = XformPoint(trao, origin);

    dirToLight = lightPos - pos;
    const FloatType distSq = LengthSquared(dirToLight);
    pdf = distSq;
    dist = sqrt(distSq);
    dirToLight = dirToLight / dist;

    TMatrix4x4<FloatType> trao_l = Interpolate(toLight, time);
    TVector3<FloatType> local = -XformVector(trao_l, dirToLight);
    lightContrib = emission.cwiseProduct(falloffCurve(local, cutoffAngle, beamWidth)) * inverse(distSq);
}

bool SpotLight::SampleDirect(const BSphere & /*sceneSphere*/,
                              const Vector3 &pos,
                              const Vector3 & /*normal*/,
                              const Vector2 /*rndParam*/,
                              const Float /*time*/,
                              LightPrimID &lPrimID,
                              Vector3 &dirToLight,
                              Float &dist,
                              Vector3 &contrib,
                              Float &cosAtLight,
                              Float &directPdf,
                              Float &emissionPdf) const {
    _SampleDirectSpotLight(toWorld, toLight, emission, cutoffAngle, beamWidth, pos, dist, dirToLight, contrib, directPdf);
    assert(dist > Float(0.0));
    emissionPdf = c_INVTWOPI / (1-cosCutoffAngle);
    cosAtLight = Float(1.0);
    lPrimID = 0;
    return true;
}

void SpotLight::Emit(const BSphere & /*sceneSphere*/,
                      const Vector2 /*rndParamPos*/,
                      const Vector2 rndParamDir,
                      const Float /*time*/,
                      LightPrimID & /*lPrimID*/,
                      Ray &ray,
                      Vector3 &emission,
                      Float &cosAtLight,
                      Float &emissionPdf,
                      Float &directPdf) const {

    Float time(0.0);
    Matrix4x4 trao = Interpolate(toWorld, time);
    Vector3 origin(0.f, 0.f, 0.f);
    Vector3 lightPos = XformPoint(trao, origin);
    ray.org = lightPos;

    ray.dir = SampleCone(rndParamDir, cosCutoffAngle);

    Matrix4x4 trao_l = Interpolate(toLight, 0.f);
    Vector3 local = XformVector(trao_l, ray.dir);

    Vector3 fallOffTerm = falloffCurve(local, cutoffAngle, beamWidth);
    emission = this->emission.cwiseProduct(fallOffTerm);
    emissionPdf = c_INVTWOPI / (1.f-cosCutoffAngle);
    cosAtLight = directPdf = Float(1.0);
}

void SampleDirectSpotLight(const ADFloat *buffer,
                            const ADBSphere & /*sceneSphere*/,
                            const ADVector3 &pos,
                            const ADVector3 & /*normal*/,
                            const ADVector2 /*rndParam*/,
                            const ADFloat /*time*/,
                            const bool /*isStatic*/,
                            ADVector3 &dirToLight,
                            ADVector3 &lightContrib,
                            ADFloat &cosAtLight,
                            ADFloat &directPdf,
                            ADFloat &emissionPdf) {
    ADVector3 lightPos, emission;
    ADAnimatedTransform toWorld, toLight;
    ADFloat cutoffAngle, beamWidth;
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    Deserialize(buffer, emission);
    Deserialize(buffer, cutoffAngle);
    Deserialize(buffer, beamWidth);
    ADFloat dist;
    _SampleDirectSpotLight(toWorld, toLight, emission, cutoffAngle, beamWidth, pos, dist, dirToLight, lightContrib, directPdf);
    emissionPdf = c_INVTWOPI/ (1.f - cos(cutoffAngle));
    cosAtLight = Const<ADFloat>(1.0);
}

void EmitSpotLight(const ADFloat *buffer,
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
    ADAnimatedTransform toWorld, toLight;
    ADFloat cutoffAngle, beamWidth;
    buffer = Deserialize(buffer, toWorld);
    buffer = Deserialize(buffer, toLight);
    Deserialize(buffer, emission_);
    Deserialize(buffer, cutoffAngle);
    Deserialize(buffer, beamWidth);

    ADMatrix4x4 trao = Interpolate(toLight, time);
    ADVector3 origin = ADVector3::Zero();
    lightPos = XformPoint(trao, origin);

    ray.org = lightPos;
    ray.dir = SampleCone(rndParamDir, cos(cutoffAngle));
    ADVector3 local = ray.dir;
    emission = emission_.cwiseProduct(falloffCurve(local, cutoffAngle, beamWidth));
    emissionPdf = c_INVTWOPI/ (1.f - cos(cutoffAngle));
    cosAtLight = Const<ADFloat>(1.0);
    directPdf = Const<ADFloat>(1.0);
}
