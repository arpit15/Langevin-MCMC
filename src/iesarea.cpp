#include "iesarea.h"
#include "shape.h"
#include "utils.h"
#include "sampling.h"
#include "transform.h"

int GetIESAreaSerializedSize() {
    return 1 +                            // type
           GetMaxShapeSerializedSize() +  // shape
           3 +                             // emission
           1+ // ies intensity for Emit
           1; // ies intensity for AD SampleDirect and Emission
}

IESArea::IESArea(const Float &samplingWeight, Shape *_shape, const Vector3 &emission, const std::string fname, const Matrix4x4 _toWorld)
    : Light(samplingWeight), shape(_shape), emission(emission),
    image(new Image3(fname)),
    toWorld(_toWorld), toLight(toWorld.inverse()) {
    _shape->SetAreaLight(this);
    // std::cout << "IESArea created!" << std::endl;
}

Float IESArea::getIESVal(const Vector3 &local_) const {
    const Vector3 local = Normalize(local_);
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

void IESArea::Serialize(const LightPrimID &lPrimID, const Vector2 &rndDir, const Vector3 &dirToLight, Float *buffer) const {
    buffer = ::Serialize((Float)LightType::IESArea, buffer);
    shape->Serialize(lPrimID, buffer);
    buffer += GetMaxShapeSerializedSize();
    buffer = ::Serialize(emission, buffer);
    // evaluate ies using rndParam
    const Vector3 localDir = SampleSphere(rndDir);
    const Float iesVal1 = getIESVal(localDir);
    buffer = ::Serialize(iesVal1, buffer);
    // for AD sampledirect and Emit
    const Vector3 dirFromLight = -dirToLight;
    const Vector3 localDirSampleDirect = XformVector(toLight, dirFromLight);
    const Float iesVal2 = getIESVal(localDirSampleDirect);
    ::Serialize(iesVal2, buffer);
}

LightPrimID IESArea::SampleDiscrete(const Float uDiscrete) const {
    return shape->Sample(uDiscrete);
}

bool IESArea::SampleDirect(const BSphere & /*sceneSphere*/,
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
    Vector3 posOnLight;
    Vector3 normalOnLight;
    Float shapePdf;
    shape->Sample(rndParam, time, lPrimID, posOnLight, normalOnLight, &shapePdf);
    dirToLight = posOnLight - pos;
    Float distSq = LengthSquared(dirToLight);
    dist = sqrt(distSq);
    assert(dist > Float(0.0));
    dirToLight = dirToLight / dist;
    cosAtLight = -Dot(dirToLight, normalOnLight);
    if (cosAtLight > c_CosEpsilon) {  // ignore grazing angle
        contrib = (cosAtLight / (distSq * shapePdf)) * emission;
        directPdf = shapePdf * distSq / cosAtLight;
        emissionPdf = shapePdf * cosAtLight * c_INVPI;
        // multiple ies val
        const Vector3 dirFromLight = -dirToLight;
        const Vector3 local = XformVector(toLight, dirFromLight);
        const Float iesVal = getIESVal(local);
        contrib *= getIESVal(local);
        return true;
    } else {
        return false;
    }
}

void IESArea::Emission(const BSphere & /*sceneSphere*/,
                         const Vector3 &dirToLight,
                         const Vector3 &normalOnLight,
                         const Float time,
                         LightPrimID &lPrimID,
                         Vector3 &emission,
                         Float &directPdf,
                         Float &emissionPdf) const {
    Float cosAtLight = -Dot(normalOnLight, dirToLight);
    if (cosAtLight > Float(0.0)) {
        const Vector3 dirFromLight = -dirToLight;
        const Vector3 local = XformVector(toLight, dirFromLight);
        const Float iesVal = getIESVal(local);
        emission = this->emission * getIESVal(local);
        directPdf = shape->SamplePdf();
        emissionPdf = cosAtLight * directPdf * c_INVPI;
    } else {
        emission = Vector3::Zero();
        directPdf = Float(0.0);
        emissionPdf = Float(0.0);
    }
}

void IESArea::Emit(const BSphere & /*sceneSphere*/,
                     const Vector2 rndParamPos,
                     const Vector2 rndParamDir,
                     const Float time,
                     LightPrimID &lPrimID,
                     Ray &ray,
                     Vector3 &emission,
                     Float &cosAtLight,
                     Float &emissionPdf,
                     Float &directPdf) const {
    Vector3 normal;
    Float shapePdf;
    shape->Sample(rndParamPos, time, lPrimID, ray.org, normal, &shapePdf);
    assert(shapePdf > Float(0.0));

    Vector3 d = SampleCosHemisphere(rndParamDir);
    Vector3 b0;
    Vector3 b1;
    CoordinateSystem(normal, b0, b1);
    ray.dir = d[0] * b0 + d[1] * b1 + d[2] * normal;
    const Vector3 dirFromLight = ray.dir ;
    const Vector3 local = XformVector(toLight, dirFromLight);
    const Float IESVal = getIESVal(local);
    emission = this->emission * (Float(M_PI) / shapePdf) * getIESVal(local);
    cosAtLight = d[2];
    emissionPdf = d[2] * c_INVPI * shapePdf;
    directPdf = shapePdf;
}

template <typename FloatType>
void _SampleDirectIESArea(const FloatType *buffer,
                            const TVector3<FloatType> &pos,
                            const TVector3<FloatType> &normal,
                            const TVector2<FloatType> rndParam,
                            const FloatType time,
                            const bool isStatic,
                            TVector3<FloatType> &dirToLight,
                            TVector3<FloatType> &lightContrib,
                            FloatType &cosAtLight,
                            FloatType &directPdf,
                            FloatType &emissionPdf) {
    TVector3<FloatType> posOnLight;
    TVector3<FloatType> normalOnLight;
    FloatType shapePdf;
    SampleShape(buffer, rndParam, time, isStatic, posOnLight, normalOnLight, shapePdf);
    buffer += GetMaxShapeSerializedSize();
    TVector3<FloatType> emission;
    buffer = Deserialize(buffer, emission);
    FloatType iesVal1, iesVal2;
    buffer = Deserialize(buffer, iesVal1);
    Deserialize(buffer, iesVal2);

    dirToLight = posOnLight - pos;
    FloatType distSq = LengthSquared(dirToLight);
    FloatType dist = sqrt(distSq);
    dirToLight = dirToLight / dist;
    cosAtLight = -Dot(dirToLight, normalOnLight);
    directPdf = shapePdf * distSq / cosAtLight;
    lightContrib = emission / directPdf;
    lightContrib *= iesVal2;
    emissionPdf = shapePdf * cosAtLight * c_INVPI;
}

void SampleDirectIESArea(const ADFloat *buffer,
                           const ADBSphere & /*sceneSphere*/,
                           const ADVector3 &pos,
                           const ADVector3 &normal,
                           const ADVector2 rndParam,
                           const ADFloat time,
                           const bool isStatic,
                           ADVector3 &dirToLight,
                           ADVector3 &lightContrib,
                           ADFloat &cosAtLight,
                           ADFloat &directPdf,
                           ADFloat &emissionPdf) {
    _SampleDirectIESArea(buffer,
                           pos,
                           normal,
                           rndParam,
                           time,
                           isStatic,
                           dirToLight,
                           lightContrib,
                           cosAtLight,
                           directPdf,
                           emissionPdf);
}

void EmissionIESArea(const ADFloat *buffer,
                       const ADBSphere &sceneSphere,
                       const ADVector3 &dirToLight,
                       const ADVector3 &normalOnLight,
                       const ADFloat time,
                       ADVector3 &emission,
                       ADFloat &directPdf,
                       ADFloat &emissionPdf) {
    ADFloat shapePdf;
    buffer = SampleShapePdf(buffer, shapePdf);
    ADVector3 emission_;
    buffer = Deserialize(buffer, emission_);
    ADFloat iesVal1, iesVal2;
    buffer = Deserialize(buffer, iesVal1);
    Deserialize(buffer, iesVal2);

    ADFloat cosAtLight = -Dot(normalOnLight, dirToLight);
    // Assume cosAtLight > 0
    emission = emission_ * iesVal2;
    directPdf = shapePdf;
    emissionPdf = cosAtLight * directPdf * c_INVPI;
}

void EmitIESArea(const ADFloat *buffer,
                   const ADBSphere &sceneSphere,
                   const ADVector2 rndParamPos,
                   const ADVector2 rndParamDir,
                   const ADFloat time,
                   const bool isStatic,
                   ADRay &ray,
                   ADVector3 &emission,
                   ADFloat &cosAtLight,
                   ADFloat &emissionPdf,
                   ADFloat &directPdf) {
    ADVector3 normalOnLight;
    ADFloat shapePdf;
    SampleShape(buffer, rndParamPos, time, isStatic, ray.org, normalOnLight, shapePdf);
    buffer += GetMaxShapeSerializedSize();
    ADVector3 emission_;
    buffer = Deserialize(buffer, emission_);
    ADFloat iesVal1, iesVal2;
    buffer = Deserialize(buffer, iesVal1);
    Deserialize(buffer, iesVal2);

    ADVector3 d = SampleCosHemisphere(rndParamDir);
    ADVector3 b0;
    ADVector3 b1;
    CoordinateSystem(normalOnLight, b0, b1);

    ray.dir = d[0] * b0 + d[1] * b1 + d[2] * normalOnLight;
    emission = emission_ * (Float(M_PI) / shapePdf) * iesVal1;
    cosAtLight = d[2];
    emissionPdf = d[2] * c_INVPI * shapePdf;
    directPdf = shapePdf;
}
