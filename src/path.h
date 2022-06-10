#pragma once

#include "commondef.h"
#include "scene.h"
#include "light.h"
#include "shape.h"
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

struct EnvLight;

struct SubpathContrib {
    int camDepth;       // camera depth 
    int lightDepth;     // light depth
    Vector2 screenPos;  // screen [x,y]
    Vector3 contrib;    // f/p 
    Float lsScore;      // large step
    Float ssScore;      // small step 
    Float lensScore;    // lens 
    Float misWeight;    // MIS 
};

inline int GetPathLength(const SubpathContrib &spContrib) {
    return spContrib.camDepth + spContrib.lightDepth - 1;
}

struct CameraVertex {
    Vector2 screenPos;
};

struct SurfaceVertex {
    ShapeInst shapeInst;
    Vector2 bsdfRndParam;
    Float bsdfDiscrete;
    Float useAbsoluteParam;
    LightInst directLightInst;
    Vector2 directLightRndParam;
    Float rrWeight;
    Vector3 dirToHitLight;
};

struct LightVertex {
    Vector2 rndParamPos;
    Vector2 rndParamDir;
    LightInst lightInst;
};

struct Path {
    Float time;
    CameraVertex camVertex;
    std::vector<SurfaceVertex> camSurfaceVertex;
    LightVertex lgtVertex;
    std::vector<SurfaceVertex> lgtSurfaceVertex;
    // if last camera vertex hits an environment light, envLightInst.light != nullptr
    LightInst envLightInst;
    // Only used by camera subpaths
    Vector3 lensVertexPos;

    bool isSubpath;
    // undefined for non subpath
    int camDepth;
    int lgtDepth;

    bool isMoving;

    void SetIsMoving(){
        this->isMoving = false;
        for (const auto& vertex : this->camSurfaceVertex) {
            if (vertex.shapeInst.obj!=nullptr && vertex.shapeInst.obj->IsMoving()) {
                this->isMoving = true;
                break;
            }
        }
        if (!this->isMoving) {
            for (const auto& vertex : this->lgtSurfaceVertex) {
                if (vertex.shapeInst.obj!=nullptr && vertex.shapeInst.obj->IsMoving()) {
                    this->isMoving = true;
                    break;
                }
            }
        }
    }

    // friend std::ostream& operator<<(std::ostream& os, const Path& p);

    // Path operator=(const Path &p) {
    //     std::cout << "camDepth:" << p.camDepth << ", lgtDepth:" << p.lgtDepth << std::endl;
    //     time = p.time;
    //     camVertex = p.camVertex;
    //     camSurfaceVertex = p.camSurfaceVertex;
    //     lgtVertex = p.lgtVertex;
    //     lgtSurfaceVertex = p.lgtSurfaceVertex;
    //     envLightInst = p.envLightInst;
    //     lensVertexPos = p.lensVertexPos;
    //     isSubpath = p.isSubpath;
    //     camDepth = p.camDepth;
    //     lgtDepth = p.lgtDepth;
    // }
};

// std::ostream& operator<<(std::ostream& os, const Path& p)
// {
//     os << "t:" << p.time << std::endl
//         << "camsurfvert:" << p.camSurfaceVertex.size() << std::endl
//         << "lgtSurfaceVert:" << p.lgtSurfaceVertex.size() << std::endl;
//     return os;
// }

struct SerializedSubpath {
    AlignedStdVector primary;
    AlignedStdVector vertParams;
};

void Clear(Path &path);
void GeneratePath(const Scene *scene,
                  const Vector2i screenPosi,
                  const int minDepth,
                  const int maxDepth,
                  Path &path,
                  std::vector<SubpathContrib> &contribs,
                  RNG &rng);
void GeneratePathBidir(const Scene *scene,
                       const Vector2i screenPosi,
                       const int minDepth,
                       const int maxDepth,
                       Path &path,
                       std::vector<SubpathContrib> &contribs,
                       RNG &rng);
void GenerateSubpath(const Scene *scene,
                     const Vector2i screenPosi,
                     const int camLength,
                     const int lgtLength,
                     const bool bidirMIS,
                     Path &path,
                     std::vector<SubpathContrib> &contribs,
                     RNG &rng);
void ToSubpath(const int camDepth, const int lightDepth, Path &path);
void PerturbPath(const Scene *scene,
                 const Vector &offset,
                 Path &path,
                 std::vector<SubpathContrib> &contribs,
                 RNG &rng);
void PerturbPathBidir(const Scene *scene,
                      const Vector &offset,
                      Path &path,
                      std::vector<SubpathContrib> &contribs,
                      RNG &rng);
size_t GetVertParamSize(const int maxCamDepth, const int maxLgtDepth);
size_t GetPrimaryParamSize(const int camDepth, const int lightDepth);
void Serialize(const Scene *scene, const Path &path, SerializedSubpath &subPath);
void GetPathPss(const Path &path, std::vector<Float> &pss);
Vector GetPathPos(const Path& path);

inline int GetDimension(const Path &path) {
    assert(path.isSubpath);
    size_t pps = GetPrimaryParamSize(path.camDepth, path.lgtDepth);
    // if (!path.isMoving) {
        pps--;
    // }
    return pps;
}

inline int GetPathLength(const int camLength, const int lgtLength) {
    return camLength + lgtLength - 1;
}

using PathFunc = void (*)(const Float *, const Float *, const Float *, const Float *, Float *);
using PathFuncDerv =
    void (*)(const Float *, const Float *, const Float *, const Float *, Float *, Float *);
using PathFuncMap = std::unordered_map<std::pair<int, int>, PathFunc>;
using PathFuncDervMap = std::unordered_map<std::pair<int, int>, PathFuncDerv>;

struct PathFuncLib {
    int maxDepth;
    std::shared_ptr<const Library> library;
    PathFuncMap staticFuncMap;
    PathFuncDervMap staticDervFuncMap;
};

std::shared_ptr<Library> CompilePathFuncLibrary(const bool bidirectional,
                                                const int maxDepth,
                                                std::shared_ptr<Library> *library = nullptr);

std::shared_ptr<Library> CompilePathFuncLibrary2(const int maxDepth, 
                                                std::shared_ptr<Library> *library = nullptr);

std::shared_ptr<const PathFuncLib> BuildPathFuncLibrary(const bool bidirectional,
                                                        const int maxDepth);

std::shared_ptr<const PathFuncLib> BuildPathFuncLibrary2(const int maxDepth,
             const std::string libpath=fs::current_path()); 
