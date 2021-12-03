#include <iostream>

#include "pugixml.hpp"
#include "parsescene.h"
#include "image.h"
#include "array2d.h"
#include "commondef.h"

const int ImageW = 512;
const int ImageH = 256;
const int NumSamples = 50*ImageW*ImageH;

Vector2 sphericalToUv(Vector3 dir) 
{
    float theta = std::acos(std::clamp(dir[2], -1.0f, 1.0f));
    float phi = std::atan2(dir[1], dir[0]);
    if (phi < 0.0f) phi += 2.0f*c_PI;
    return Vector2(phi/(2.0f*c_PI), theta/c_PI);
}

Vector2i sphericalToPixel(Vector3 dir) 
{
    Vector2 pixelF = sphericalToUv(dir).array()*Vector2(ImageW, ImageH).array();
    return Vector2i(pixelF[0], pixelF[1]);
}


Vector3 uvToSpherical(Vector2 uv) 
{
    float phi = uv[0]*2.0f*c_PI;
    float theta = uv[1]*c_PI;
    return Vector3(std::cos(phi)*std::sin(theta), std::sin(phi)*std::sin(theta), std::cos(theta));
}

Vector3 pixelToSpherical(Vector2 pixel) 
{
    pixel.array() += 0.5f;
    return uvToSpherical((pixel).array()/Vector2(ImageW, ImageH).array());
}

Vector3 colorRamp(float value)
{
    Vector3 colors[] = {
        Vector3( 20.0f,  11.0f,  52.0f)/256.0f,
        Vector3(132.0f,  32.0f, 107.0f)/256.0f,
        Vector3(229.0f,  91.0f,  48.0f)/256.0f,
        Vector3(246.0f, 215.0f,  70.0f)/256.0f
    };
    value *= 3.0f;
    int i = std::clamp(int(value), 0, 2);
    float u = value - i;
    
    return colors[i] + (colors[i + 1] - colors[i])*u;
}

Image3 generateHeatmap(const Array2d<float> &density, float maxValue)
{
    Image3 result(density.width(), density.height());
    
    for (int y = 0; y < density.height(); ++y)
        for (int x = 0; x < density.width(); ++x)
            result.At(x, y) = colorRamp(density(x, y)/maxValue);

    return std::move(result);
}

Vector3 normalize(Vector3 vec){
    float norm = std::sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    Vector3 out = vec;
    out.array() /= norm;
    return out;
}

int main() {
    std::string filename = "/home/arpit/projects/Langevin-MCMC/tests/test_al.xml";
    std::string name="diffuseCoating";
    // std::string filename = "/home/arpit/projects/Langevin-MCMC/tests/test_br.xml";
    // std::string name="semispecularCoating";
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());

    // parse vals and create blend bsdf
    std::map<std::string, std::shared_ptr<const TextureRGB>> textureMap;
    SubsT subs;
    bool twoSided = false;
    auto bsdf = ParseBSDF(doc.child("scene").child("bsdf"), textureMap, subs, twoSided);

    // sample values and pdf
    Vector3 normal1 = Vector3(0.0f, 0.0f, 1.0f);
    Vector3 normal2 = normalize(Vector3(0.25f, 0.5f, 1.0f));
    // init
    Vector3 wi(0.25f, 0.0f, -1.0f); 
    wi = normalize(wi);
    Vector3 normal, wo, contrib;
    Vector2 st;
    Float uDiscrete, cosWo, pdfVal, revPdf;
    Array2d<float> pdf(ImageW, ImageH);
    for (int y = 0; y < ImageH; y++)
    {
        for (int x = 0; x < ImageW; x++)
        {
            wo = pixelToSpherical(Vector2(x,y)); 
            bsdf->Evaluate(wi, normal1, wo, st, 
                        contrib, cosWo, pdfVal, revPdf);
            pdf(x, y) = pdfVal;
        }
        
    }
    
    // Step 2: Generate histogram of samples
    const int HistoSubsample = 4; // Merge adjacent pixels to decrease noise
    Array2d<float> histogram(ImageW/HistoSubsample, ImageH/HistoSubsample);

    int validSamples = 0;
    bool isSpecularSet = false;
    bool belowHemisphere = false;
    bool nanOrInf = false;

    Vector2 rndParam;
    const int seed = 0;
    RNG rng(seed);
    std::uniform_real_distribution<Float> uniDist(Float(0.0), Float(1.0));
    for (int i = 0; i < NumSamples; ++i) {
        // Sample material
        rndParam = Vector2(uniDist(rng), uniDist(rng));
        bsdf->Sample(wi, normal1, st, rndParam, uDiscrete,
                        wo, contrib, cosWo, pdfVal, revPdf
                );

        // if (record.isSpecular)
        //     isSpecularSet = true;
        
        // Sanity check to make sure directions are valid
        // Vector3 dirOut = Normalize(record.scattered);
        // if (Dot(wo, hit.sn) < 0.0f) {
        //     belowHemisphere = true;
        //     continue;
        // }
        if (std::isnan(wo[0] + wo[1] + wo[2]) || std::isinf(wo[0] + wo[1] + wo[2])) {
            nanOrInf = true;
            continue;
        }

        // Map scattered direction to pixel in our sample histogram
        Vector2i pixel = sphericalToPixel(wo)/HistoSubsample;
        if (pixel[0] < 0 || pixel[1] < 0 || pixel[0] >= histogram.width() || pixel[1] >= histogram.height())
            continue;

        // Incorporate Jacobian of spherical mapping and bin area into the sample weight
        float sinTheta = std::sqrt(std::max(1.0f - wo[2]*wo[2], 0.0f));
        float weight = (histogram.width()*histogram.height())/(c_PI*(2.0f*c_PI)*NumSamples*sinTheta);
        // Accumulate into histogram
        histogram(pixel[0], pixel[1]) += weight;
        validSamples++;
    }

    // Now upscale our histogram
    Array2d<float> histoFullres(ImageW, ImageH);
    for (int y = 0; y < ImageH; ++y)
        for (int x = 0; x < ImageW; ++x)
            histoFullres(x, y) = histogram(x/HistoSubsample, y/HistoSubsample);

    // Step 3: Compute pdf statistics: Maximum value and integral
    // int validSamples = 0;
    // bool nanOrInf = false;
    float maxValue = 0.0f;
    for (int y = 0; y < ImageH; ++y) {
        for (int x = 0; x < ImageW; ++x) {
            maxValue = std::max(maxValue, pdf(x, y));
            if (std::isnan(pdf(x, y)) || std::isinf(pdf(x, y)))
                nanOrInf = true;
        }
    }

    double integral = 0.0f;
    for (int y = 0; y < ImageH; ++y) {
        for (int x = 0; x < ImageW; ++x) {
            Vector3 dir = pixelToSpherical(Vector2(x, y));
            float sinTheta = std::sqrt(std::max(1.0f - dir[2]*dir[2], 0.0f));
            float pixelArea = (c_PI/ImageW)*(c_PI*2.0f/ImageH)*sinTheta;
            integral += pixelArea*pdf(x, y);
        }
    }

    // Generate heat maps
    Image3 pdfImage = generateHeatmap(pdf, maxValue);
    WriteImage(name + "-pdf.png", &pdfImage);
    
    Image3 sampleImage = generateHeatmap(histoFullres, maxValue);
    WriteImage(name + "-sampled.png", &sampleImage);

    // Output statistics
    std::cout << "Integral of PDF (should be close to 1): " << integral << std::endl;
    std::cout << (validSamples*100)/NumSamples << "% of samples were valid (this should be close to 100%)" << std::endl;
    // if (isSpecularSet)
    //     std::cout << "WARNING: isSpecular is set. It should not be." << std::endl;
    // if (belowHemisphere)
    //     std::cout << "WARNING: Some generated directions were below the hemisphere. "
    //                  "You should check for this case and return false from sample instead" << std::endl;
    if (nanOrInf)
        std::cout << "WARNING: Some directions/PDFs contained invalid values (NaN or infinity). This should not happen. Make sure you catch all corner cases in your code" << std::endl;

    return 0;
}