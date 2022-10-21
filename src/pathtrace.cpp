#include "pathtrace.h"
#include "camera.h"
#include "image.h"
#include "path.h"
#include "progressreporter.h"
#include "parallel.h"
#include "timer.h"
#include "bsdf.h"
#include "nanolog.hh"

#include <algorithm>
#include <vector>
#include <unordered_map>

void PathTrace(const Scene *scene, const std::shared_ptr<const PathFuncLib> pathFuncLib, const bool tonemap) {
    SerializedSubpath ssubPath;

    int maxDervDepth = pathFuncLib->maxDepth;
    std::vector<Float> sceneParams(GetSceneSerializedSize());
    Serialize(scene, &sceneParams[0]);
    ssubPath.primary.resize(GetPrimaryParamSize(maxDervDepth, maxDervDepth));
    ssubPath.vertParams.resize(GetVertParamSize(maxDervDepth, maxDervDepth));

    const int spp = scene->options->spp;
    std::shared_ptr<const Camera> camera = scene->camera;
    std::shared_ptr<Image3> film = camera->film;
    film->Clear();
    const int pixelHeight = GetPixelHeight(camera.get());
    const int pixelWidth = GetPixelWidth(camera.get());
    const int cropHeight = GetCropHeight(camera.get());
    const int cropWidth = GetCropWidth(camera.get());
    const int tileSize = 16;
    // const int nXTiles = (pixelWidth + tileSize - 1) / tileSize;
    // const int nYTiles = (pixelHeight + tileSize - 1) / tileSize;
    const int nXTiles = (cropWidth + tileSize - 1) / tileSize;
    const int nYTiles = (cropHeight + tileSize - 1) / tileSize;

    const int cropOffsetX = GetCropOffsetX(camera.get());
    const int cropOffsetY = GetCropOffsetY(camera.get());

    ProgressReporter reporter(nXTiles * nYTiles);
    // SampleBuffer buffer(pixelWidth, pixelHeight);
    // SampleBuffer buffer(pixelWidth, pixelHeight, cropWidth, cropHeight, cropOffsetX, cropOffsetY);
    SampleBuffer buffer(cropWidth, cropHeight);

    auto pathFunc = scene->options->bidirectional ? GeneratePathBidir : GeneratePath;
    Timer timer;
    Tick(timer);

    ParallelFor([&](const Vector2i tile) {
        const int seed = tile[1] * nXTiles + tile[0];
        RNG rng(seed);
        const int x0 = tile[0] * tileSize;
        // const int x1 = std::min(x0 + tileSize, pixelWidth);
        const int x1 = std::min(x0 + tileSize, cropWidth);
        const int y0 = tile[1] * tileSize;
        // const int y1 = std::min(y0 + tileSize, pixelHeight);
        const int y1 = std::min(y0 + tileSize, cropHeight);
        Path path;
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                for (int s = 0; s < spp; s++) {
                    Clear(path);
                    std::vector<SubpathContrib> spContribs;
                    
                    // std::cout << "x:" << x << ", y:" << y << std::endl;
                    pathFunc(scene,
                             Vector2i(x, y),
                             scene->options->minDepth,
                             scene->options->maxDepth,
                             path,
                             spContribs,
                             rng);

                    // std::cout << "#contribs: " << spContribs.size() << std::endl;
                    for (const auto &spContrib : spContribs) {
                        // std::cout << "lum:" << Luminance(spContrib.contrib) << std::endl;
                        // NANOLOG_INFO("***individuals: {}", spContrib.contrib.transpose());
                        if (Luminance(spContrib.contrib) <= Float(1e-10)) {
                            continue;
                        }
                        Vector3 contrib = spContrib.contrib / Float(spp);
                        // NANOLOG_INFO("++++++++++++SPLAT contrib: {}", contrib.transpose());
                        Splat(buffer, spContrib.screenPos, contrib);
                    }
                }
            }
        }
        reporter.Update(1);
    }, Vector2i(nXTiles, nYTiles));
    TerminateWorkerThreads();
    reporter.Done();
    Float elapsed = Tick(timer);
    std::cout << "Elapsed time:" << elapsed << std::endl;

    BufferToFilm(buffer, film.get());
    
    std::string outputNameHDR = scene->outputName + ".exr";
    std::string outputNameLDR = scene->outputName + ".png";
    WriteImage(outputNameHDR, GetFilm(scene->camera.get()).get());
    
    std::string addRenderingTime = std::string("oiiotool ") + outputNameHDR + " --attrib 'RenderingTime' " + std::to_string(elapsed) + " -o " + outputNameHDR;
    system(addRenderingTime.c_str());

    if(tonemap){
        std::string hdr2ldr = std::string("hdrmanip --tonemap filmic -o ") + outputNameLDR + " " + outputNameHDR;
        system(hdr2ldr.c_str());

        std::string addRenderingTime2 = std::string("oiiotool ") + outputNameLDR + " --attrib 'RenderingTime' " + std::to_string(elapsed) + " -o " + outputNameLDR;
        system(addRenderingTime2.c_str());

    }
    
    std::cout << "Done!" << std::endl;
}
