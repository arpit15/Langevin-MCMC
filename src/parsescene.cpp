#include "nanolog.hh"

#include "parsescene.h"
#include "pugixml.hpp"
#include "animatedtransform.h"
#include "transform.h"
#include "image.h"
#include "camera.h"
#include "trianglemesh.h"
#include "parseobj.h"
#include "parseply.h"
#include "loadserialized.h"
#include "pointlight.h"
#include "arealight.h"
#include "ieslight.h"
#include "iesarea.h"
#include "collimatedlight.h"
#include "envlight.h"
#include "lambertian.h"
#include "phong.h"
#include "blendbsdf.h"
#include "roughconductor.h"
#include "roughdielectric.h"
#include "constanttexture.h"
#include "bitmaptexture.h"

#include "spotlight.h"

#include <iostream>
#include <regex>

inline void replace_inplace(std::string &str, const std::string &source,
                            const std::string &target) {
    // std::cout << str << ", " << source << ", " << target << std::endl;
    size_t pos = 0;

    // to take care of not replacing in case of str=$z40, source=$z
    size_t dollar_pos = str.find('$');
    size_t search_size1 = str.length() - dollar_pos;
    size_t search_size2 = source.length();

    // std::cout << str << ", " << source << ", " << target << ", " << search_size1 << ", " << search_size2 << std::endl;

    if (search_size1 != search_size2) return;

    while ((pos = str.find(source.c_str(), pos, search_size1)) != std::string::npos) {
        str.replace(pos, source.length(), target);
        pos += target.length();
    }
}

using BSDFMap = std::map<std::string, std::shared_ptr<const BSDF>>;
// using TextureMap = std::map<std::string, std::shared_ptr<const TextureRGB>>;

Vector3 ParseVector3(const std::string &value);
Matrix4x4 ParseMatrix4x4(const std::string &value);
Matrix4x4 ParseTransform(pugi::xml_node node, SubsT &subs);
AnimatedTransform ParseAnimatedTransform(pugi::xml_node node, SubsT &subs);
std::unique_ptr<Scene> ParseScene(pugi::xml_node node, SubsT &subs);
std::shared_ptr<const Camera> ParseSensor(pugi::xml_node node, std::string &filename, SubsT &subs);
std::shared_ptr<Image3> ParseFilm(pugi::xml_node node, std::string &filename, SubsT &subs);
std::shared_ptr<const Shape> ParseShape(pugi::xml_node node,
                                        const BSDFMap &bsdfMap,
                                        const TextureMap &textureMap,
                                        std::shared_ptr<const Light> &areaLight, SubsT &subs);
// std::shared_ptr<const BSDF> ParseBSDF(pugi::xml_node node,
//                                       const TextureMap &textureMap,
//                                       SubsT &subs,
//                                       bool twoSided = false);
std::shared_ptr<const Light> ParseEmitter(pugi::xml_node node,
                                          std::shared_ptr<const EnvLight> &envLight, SubsT &subs);
std::shared_ptr<const TextureRGB> ParseTexture(pugi::xml_node node, SubsT &subs);
std::shared_ptr<const Texture1D> Parse1DMap(pugi::xml_node node, const TextureMap &textureMap, SubsT &subs);
std::shared_ptr<const TextureRGB> Parse3DMap(pugi::xml_node node, const TextureMap &textureMap, SubsT &subs);

void replace_func(SubsT &subs, pugi::xml_node &node) {
    // taken from mitsuba2
    // https://github.com/mitsuba-renderer/mitsuba2/blob/master/src/libcore/xml.cpp#L449
    if (!subs.empty()) {
        for (auto attr: node.attributes()) {
            std::string value = attr.value();
            if (value.find('$') == std::string::npos)
                continue;
            // std::cout << "Need subs " << value << std::endl;
            for (const auto &kv : subs) {
                // std::cout << "Found subs " << kv.first << " = " << kv.second << std::endl;
                replace_inplace(value, "$" + kv.first, kv.second);
            }
            attr.set_value(value.c_str());
        }
        // iterate over child attributes as well
        for (auto child : node.children()) {
            for (auto attr: child.attributes()) {
                std::string value = attr.value();
                if (value.find('$') == std::string::npos)
                    continue;
                // std::cout << "Need subs " << value << std::endl;
                for (const auto &kv : subs) {
                    // std::cout << "Found subs " << kv.first << " = " << kv.second << std::endl;
                    replace_inplace(value, "$" + kv.first, kv.second);
            }
            attr.set_value(value.c_str());
        }
        }

    }
}
Vector3 ParseVector3(const std::string &value) {
    std::regex rgx("(,| )+");
    std::sregex_token_iterator first{begin(value), end(value), rgx, -1}, last;
    std::vector<std::string> list{first, last};
    Vector3 v;
    if (list.size() == 1) {
        v[0] = stof(list[0]);
        v[1] = stof(list[0]);
        v[2] = stof(list[0]);
    } else if (list.size() == 3) {
        v[0] = stof(list[0]);
        v[1] = stof(list[1]);
        v[2] = stof(list[2]);
    } else {
        Error("ParseVector3 failed");
    }
    return v;
}

Matrix4x4 ParseMatrix4x4(const std::string &value) {
    std::regex rgx("(,| )+");
    std::sregex_token_iterator first{begin(value), end(value), rgx, -1}, last;
    std::vector<std::string> list{first, last};
    if (list.size() != 16) {
        Error("ParseMatrix4x4 failed");
    }

    Float m[4][4];
    int k = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m[i][j] = std::stof(list[k++]);
        }
    }

    Matrix4x4 mat;
    mat << m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1],
        m[2][2], m[2][3], m[3][0], m[3][1], m[3][2], m[3][3];

    return mat;
}

Matrix4x4 ParseTransform(pugi::xml_node node, SubsT &subs) {
    replace_func(subs, node);
    Matrix4x4 tform = Matrix4x4::Identity();
    for (auto child : node.children()) {
        std::string name = child.name();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == "scale") {
            if (!child.attribute("value").empty()) {
                Float s = std::stof(child.attribute("value").value());
                tform = Scale(Vector3(s, s, s)) * tform;
            } else {
                Float x = 1.0;
                Float y = 1.0;
                Float z = 1.0;
                if (!child.attribute("x").empty())
                    x = std::stof(child.attribute("x").value());
                if (!child.attribute("y").empty())
                    y = std::stof(child.attribute("y").value());
                if (!child.attribute("z").empty())
                    z = std::stof(child.attribute("z").value());
                tform = Scale(Vector3(x, y, z)) * tform;
            }
        } else if (name == "translate") {
            Float x = 0.0;
            Float y = 0.0;
            Float z = 0.0;
            if (!child.attribute("x").empty()){
                // std::cout << "x:" << child.attribute("x").value() << std::endl;
                x = std::stof(child.attribute("x").value());
            }
            if (!child.attribute("y").empty()){
                // std::cout << "y:" << child.attribute("y").value() << std::endl;
                y = std::stof(child.attribute("y").value());
            }
            if (!child.attribute("z").empty()){
                // std::cout << "z:" << child.attribute("z").value() << std::endl;
                z = std::stof(child.attribute("z").value());
            }
            tform = Translate(Vector3(x, y, z)) * tform;
        } else if (name == "rotate") {
            Float x = 0.0;
            Float y = 0.0;
            Float z = 0.0;
            if (!child.attribute("x").empty())
                x = std::stof(child.attribute("x").value());
            if (!child.attribute("y").empty())
                y = std::stof(child.attribute("y").value());
            if (!child.attribute("z").empty())
                z = std::stof(child.attribute("z").value());
            Float angle = 0.0;
            if (!child.attribute("angle").empty())
                angle = std::stof(child.attribute("angle").value());
            tform = Rotate(angle, Vector3(x, y, z)) * tform;
        } else if (name == "lookat") {
            Vector3 pos = ParseVector3(child.attribute("origin").value());
            Vector3 target = ParseVector3(child.attribute("target").value());
            Vector3 up = ParseVector3(child.attribute("up").value());
            tform = LookAt(pos, target, up) * tform;
        } else if (name == "matrix") {
            Matrix4x4 trans = ParseMatrix4x4(std::string(child.attribute("value").value()));
            tform = trans * tform;
        }
    }
    return tform;
}

AnimatedTransform ParseAnimatedTransform(pugi::xml_node node, SubsT &subs) {
    int transformCount = 0;
    Matrix4x4 m[2];
    for (auto child : node.children()) {
        if (std::string(child.name()) == "transform") {
            m[transformCount++] = ParseTransform(child, subs);
            if (transformCount >= 2)
                break;
        }
    }
    return MakeAnimatedTransform(m[0], m[1]);
}

Matrix4x4 remove_scale(Matrix4x4 trafo) {
    float scalex = 0.f, scaley = 0.f, scalez=0.f;
    for(int i=0; i<3; ++i) {
        scalex += trafo(0,i)*trafo(0,i);
        scaley += trafo(1,i)*trafo(1,i);
        scalez += trafo(2,i)*trafo(2,i);
    }
    scalex = sqrt(scalex);
    scaley = sqrt(scaley);
    scalez = sqrt(scalez);

    Matrix4x4 scaledT = Scale( Vector3(1.f/scalex, 1.f/scaley, 1.f/scalez));
    Matrix4x4 finalT = scaledT * trafo;
    // copy the translation back
    for(int i=0;i<3;i++)
        finalT(i,3) = trafo(i,3);
    return finalT;
}

std::shared_ptr<Image3> ParseFilm(pugi::xml_node node, std::string &filename, 
            int &pixelWidth, int &pixelHeight,
            int &cropOffsetX, int &cropOffsetY, int &cropWidth, int &cropHeight, SubsT &subs ) {
    int width = 512;
    int height = 512;

    replace_func(subs, node);
    for (auto child : node.children()) {
        std::string name = child.attribute("name").value();
        if (name == "width") {
            width = atoi(child.attribute("value").value());
        } else if (name == "height") {
            height = atoi(child.attribute("value").value());
        } else if (name == "filename") {
            filename = std::string(child.attribute("value").value());
        } 
    }

    pixelWidth = width;
    pixelHeight = height;

    cropOffsetX = 0;
    cropOffsetY = 0;
    cropWidth = width;
    cropHeight = height;

    for (auto child : node.children()) {
        std::string name = child.attribute("name").value(); 
        if (name == "cropOffsetX") {
            cropOffsetX = atoi(child.attribute("value").value());
        } else if (name == "cropOffsetY") {
            cropOffsetY = atoi(child.attribute("value").value());
        } else if (name == "cropWidth") {
            cropWidth = atoi(child.attribute("value").value());
        } else if (name == "cropHeight") {
            cropHeight = atoi(child.attribute("value").value());
        }
    }
    
    return std::make_shared<Image3>(cropWidth, cropHeight);
}

std::shared_ptr<const Camera> ParseSensor(pugi::xml_node node, std::string &filename, SubsT &subs) {
    AnimatedTransform toWorld = MakeAnimatedTransform(Matrix4x4::Identity(), Matrix4x4::Identity());
    Float nearClip = 1e-2;
    Float farClip = 1000.0;
    Float fov = 45.0;
    std::shared_ptr<Image3> film;
    int pixelWidth, pixelHeight;
    int cropOffsetX = 0, cropOffsetY = 0,
         cropWidth, cropHeight;

    replace_func(subs, node);

    for (auto child : node.children()) {
        std::string name = child.attribute("name").value();
        if (name == "nearClip") {
            nearClip = std::stof(child.attribute("value").value());
        } else if (name == "farClip") {
            farClip = std::stof(child.attribute("value").value());
        } else if (name == "fov") {
            fov = std::stof(child.attribute("value").value());
        } else if (name == "toWorld") {
            if (std::string(child.name()) == "transform") {
                Matrix4x4 m = ParseTransform(child, subs);
                toWorld = MakeAnimatedTransform(m, m);
            } else if (std::string(child.name()) == "animation") {
                toWorld = ParseAnimatedTransform(child, subs);
            }
        } else if (std::string(child.name()) == "film") {
            film = ParseFilm(child, filename, pixelWidth, pixelHeight, cropOffsetX, cropOffsetY, cropWidth, cropHeight, subs);
        }
    }
    if (film.get() == nullptr) {
        film = std::make_shared<Image3>(512, 512);
    }

    // std::cout << "Created Camera [" << std::endl
    //             << "cropX " << cropOffsetX << std::endl
    //             << "cropY " << cropOffsetY << std::endl
    //             << "cropW " << cropWidth << std::endl
    //             << "cropH " << cropHeight << std::endl
    //             << "]" << std::endl;
    NANOLOG_TRACE("Created Camera [\n cropX: {}\n cropY: {}\n cropW: {}\n cropH: {}",
            cropOffsetX, cropOffsetY, cropWidth, cropHeight);


    // Eigen alignment issue
    return std::allocate_shared<Camera>(
        Eigen::aligned_allocator<Camera>(), toWorld, fov, film, nearClip, farClip, pixelWidth, pixelHeight, cropOffsetX, cropOffsetY, cropWidth, cropHeight );
}

std::shared_ptr<const Shape> ParseShape(pugi::xml_node node,
                                        const BSDFMap &bsdfMap,
                                        const TextureMap &textureMap,
                                        std::shared_ptr<const Light> &areaLight, SubsT &subs) {
    std::shared_ptr<const BSDF> bsdf;

    replace_func(subs, node);
    for (auto child : node.children()) {
        std::string name = child.name();
        if (name == "bsdf") {
            bsdf = ParseBSDF(child, textureMap, subs);
            break;
        } else if (name == "ref") {
            pugi::xml_attribute idAttr = child.attribute("id");
            if (!idAttr.empty()) {
                auto bsdfIt = bsdfMap.find(idAttr.value());
                if (bsdfIt == bsdfMap.end()) {
                    printf("ref: %s, %s, %s\n", idAttr.name(), idAttr.value(), idAttr.as_string());
                    Error("ref not found");
                }
                bsdf = bsdfIt->second;
                break;
            } else {
                printf("ref: %s, %s, %s\n", idAttr.name(), idAttr.value(), idAttr.as_string());
                Error("ref not found");
            }
        }
    }
    std::shared_ptr<Shape> shape;
    // hack for emitter
    Matrix4x4 toWorldForEmitter;
    std::string type = node.attribute("type").value();
    if (type == "serialized") {
        std::string filename;
        int shapeIndex = 0;
        Matrix4x4 toWorld[2];
        toWorld[0] = toWorld[1] = Matrix4x4::Identity();
        bool isMoving = false;
        bool flipNormals = false;
        bool faceNormals = false;

        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "filename") {
                filename = child.attribute("value").value();
            } else if (name == "shapeIndex") {
                shapeIndex = atoi(child.attribute("value").value());
            } else if (name == "flipNormals") {
                flipNormals = child.attribute("value").value();
            } else if (name == "faceNormals") {
                faceNormals = child.attribute("value").value();
            } else if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    toWorld[0] = toWorld[1] = ParseTransform(child, subs);
                } else if (std::string(child.name()) == "animation") {
                    int transformCount = 0;
                    for (auto grandChild : child.children()) {
                        if (std::string(grandChild.name()) == "transform") {
                            toWorld[transformCount++] = ParseTransform(grandChild, subs);
                            if (transformCount >= 2)
                                break;
                        }
                    }
                    if (transformCount != 2) {
                        Error("Invalid animation");
                    }
                    isMoving = true;
                }
            }
        }
        // printf("load serialized fn: %s\n", filename.c_str());
        shape = std::make_shared<TriangleMesh>(
            bsdf, LoadSerialized(filename, shapeIndex, toWorld[0], toWorld[1], isMoving, flipNormals, faceNormals));
    } else if (type == "obj") {
        std::string filename;
        Matrix4x4 toWorld[2];
        toWorld[0] = toWorld[1] = Matrix4x4::Identity();
        bool isMoving = false;
        bool flipNormals = false;
        bool faceNormals = false;
        
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "filename") {
                filename = child.attribute("value").value();
            } else if (name == "flipNormals") {
                flipNormals = child.attribute("value").value();
            } else if (name == "faceNormals") {
                faceNormals = child.attribute("value").value();
            } else if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    toWorld[0] = toWorld[1] = ParseTransform(child, subs);
                } else if (std::string(child.name()) == "animation") {
                    int transformCount = 0;
                    for (auto grandChild : child.children()) {
                        if (std::string(grandChild.name()) == "transform") {
                            toWorld[transformCount++] = ParseTransform(grandChild, subs);
                            if (transformCount >= 2)
                                break;
                        }
                    }
                    if (transformCount != 2) {
                        Error("Invalid animation");
                    }
                    isMoving = true;
                }
            }
        }
        shape = std::make_shared<TriangleMesh>(
            bsdf, ParseObj(filename, toWorld[0], toWorld[1], isMoving, flipNormals, faceNormals));

        toWorldForEmitter = toWorld[0];
    } else if (type == "ply") {
        std::string filename;
        Matrix4x4 toWorld[2];
        toWorld[0] = toWorld[1] = Matrix4x4::Identity();
        bool isMoving = false;
        bool flipNormals = false;
        bool faceNormals = false;

        std::string id = node.attribute("id").value();;
        
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "filename") {
                filename = child.attribute("value").value();
            } else if (name == "flipNormals") {
                flipNormals = child.attribute("value").value();
            } else if (name == "faceNormals") {
                faceNormals = child.attribute("value").value();
            } else if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    toWorld[0] = toWorld[1] = ParseTransform(child, subs);
                } else if (std::string(child.name()) == "animation") {
                    int transformCount = 0;
                    for (auto grandChild : child.children()) {
                        if (std::string(grandChild.name()) == "transform") {
                            toWorld[transformCount++] = ParseTransform(grandChild, subs);
                            if (transformCount >= 2)
                                break;
                        }
                    }
                    if (transformCount != 2) {
                        Error("Invalid animation");
                    }
                    isMoving = true;
                }
            }
        }

        std::cout << "Created PLY with id: " << id << std::endl;
        shape = std::make_shared<TriangleMesh>(
            bsdf, ParsePly(filename, toWorld[0], toWorld[1], isMoving, flipNormals, faceNormals), id);

        toWorldForEmitter = toWorld[0];
    } else {
        printf("shape type: %s not found.\n", type.c_str());
    }

    if (shape.get() == nullptr) {
        Error("Invalid shape");
    }

    for (auto child : node.children()) {
        std::string name = child.name();
        std::string ies_fname("");
        if (name == "emitter") {
            Vector3 radiance(Float(1.0), Float(1.0), Float(1.0));
            for (auto grandChild : child.children()) {
                std::string name = grandChild.attribute("name").value();
                if (name == "radiance")
                    radiance = ParseVector3(grandChild.attribute("value").value());
                else if (name == "filename") {
                    ies_fname = grandChild.attribute("value").value();
                }
            }

            toWorldForEmitter = remove_scale(toWorldForEmitter);
            // std::cout << "Area light transform " << toWorldForEmitter << std::endl;
            std::string emitterType = child.attribute("type").value();
            if (emitterType == "area")
                areaLight = std::make_shared<const AreaLight>(Float(1.0), shape.get(), radiance);
            else
                areaLight = std::make_shared<const IESArea>(Float(1.0), shape.get(), radiance, ies_fname, toWorldForEmitter);
        }
    }

    return shape;
}

std::shared_ptr<const BSDF> ParseBSDF(pugi::xml_node node,
                                      const TextureMap &textureMap,
                                      SubsT &subs, bool twoSided) {
    
    replace_func(subs, node);

    std::string type = node.attribute("type").value();

    
    if (type == "diffuse") {
        std::shared_ptr<const TextureRGB> reflectance =
            std::make_shared<const ConstantTextureRGB>(Vector3(0.5, 0.5, 0.5));
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "reflectance") {
                reflectance = Parse3DMap(child, textureMap, subs);
            }
        }
        return std::make_shared<Lambertian>(twoSided, reflectance);
    } else if (type == "phong") {
        std::shared_ptr<const TextureRGB> diffuseReflectance =
            std::make_shared<const ConstantTextureRGB>(Vector3(0.5, 0.5, 0.5));
        std::shared_ptr<const TextureRGB> specularReflectance =
            std::make_shared<const ConstantTextureRGB>(Vector3(0.2, 0.2, 0.2));
        std::shared_ptr<const Texture1D> exponent =
            std::make_shared<const ConstantTexture1D>(Vector1(30.0));
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "diffuseReflectance") {
                diffuseReflectance = Parse3DMap(child, textureMap, subs);
            } else if (name == "specularReflectance") {
                std::string value = child.attribute("value").value();
                specularReflectance = Parse3DMap(child, textureMap, subs);
            } else if (name == "exponent") {
                exponent = Parse1DMap(child, textureMap, subs);
            }
        }
        return std::make_shared<Phong>(twoSided, diffuseReflectance, specularReflectance, exponent);
    } else if (type == "blendbsdf") {
        std::shared_ptr<const TextureRGB> weight =
            std::make_shared<const ConstantTextureRGB>(Vector3(0.5, 0.5, 0.5));
        std::vector<std::shared_ptr<const BSDF>> bsdfs;
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            std::string type = child.name();
            if (name == "weight") {
                weight = Parse3DMap(child, textureMap, subs);
            } else if (type == "bsdf") {
                bsdfs.push_back(ParseBSDF(child, textureMap, subs, twoSided));
            }
        }

        // std::cout << "BlendBSDF [" << std::endl
        //             << "weight : " << weight->Avg().transpose() << std::endl
        //             << "BSDF A type : " << bsdfs.at(0)->GetBSDFTypeString() << std::endl
        //             << "BSDF B type : " << bsdfs.at(1)->GetBSDFTypeString() << std::endl
        //             << "]" << std::endl;
        NANOLOG_TRACE("BlendBSDF [\nweight: {}\nBDSF A type: {}\n BDSF B type: {}\n]",
            weight->Avg().transpose(),
            bsdfs.at(0)->GetBSDFTypeString(),
            bsdfs.at(1)->GetBSDFTypeString()
        );

        return std::make_shared<BlendBSDF>(twoSided, weight, bsdfs.at(0), bsdfs.at(1));
    
    } else if (type == "roughdielectric") {
        // default params
        std::shared_ptr<const TextureRGB> specularReflectance =
            std::make_shared<const ConstantTextureRGB>(Vector3(1.0, 1.0, 1.0));
        std::shared_ptr<const TextureRGB> specularTransmittance =
            std::make_shared<const ConstantTextureRGB>(Vector3(1.0, 1.0, 1.0));
        Float intIOR = 1.5046;
        Float extIOR = 1.000277;
        Vector1 v(0.1);
        std::shared_ptr<const Texture1D> alpha = std::make_shared<const ConstantTexture1D>(v);

        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "intIOR") {
                intIOR = std::stof(child.attribute("value").value());
            } else if (name == "extIOR") {
                extIOR = std::stof(child.attribute("value").value());
            } else if (name == "alpha") {
                alpha = Parse1DMap(child, textureMap, subs);
            } else if (name == "specularReflectance") {
                std::string value = child.attribute("value").value();
                specularReflectance = Parse3DMap(child, textureMap, subs);
            } else if (name == "specularTransmittance") {
                std::string value = child.attribute("value").value();
                specularTransmittance = Parse3DMap(child, textureMap, subs);
            }
        }
        return std::make_shared<RoughDielectric>(
            twoSided, specularReflectance, specularTransmittance, intIOR, extIOR, alpha);
    } else if (type == "roughconductor") {
        // default params
        std::shared_ptr<const TextureRGB> specularReflectance =
            std::make_shared<const ConstantTextureRGB>(Vector3(1.0, 1.0, 1.0));
        
        Float intIOR = 1.5046, k = 1.0;
        Float extIOR = 1.000277;
        Vector1 v(0.1);
        std::shared_ptr<const Texture1D> alpha = std::make_shared<const ConstantTexture1D>(v);

        // std::cout << "parsing roughconductor" << std::endl;
        NANOLOG_DEBUG("parsing roughconductor");

        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            // std::cout << "name : " << name << std::endl;
            if (name == "eta") {
                intIOR = std::stof(child.attribute("value").value());
            } else if (name == "k") {
                k = std::stof(child.attribute("value").value());
            } else if (name == "extEta") {
                extIOR = std::stof(child.attribute("value").value());
            } else if (name == "alpha") {
                alpha = Parse1DMap(child, textureMap, subs);
            } else if (name == "specularReflectance") {
                std::string value = child.attribute("value").value();
                specularReflectance = Parse3DMap(child, textureMap, subs);
            }
        }

        // std::cout << "RoughConductor [" << std::endl
        //             << "eta : " << intIOR << std::endl
        //             << "extIOR : " << extIOR << std::endl
        //             << "alpha : " << alpha->Avg() << std::endl;
        return std::make_shared<RoughConductor>(
            twoSided, specularReflectance, intIOR, k, extIOR, alpha);
    } else if (type == "twosided") {
        for (auto child : node.children()) {
            if (std::string(child.name()) == "bsdf") {
                return ParseBSDF(child, textureMap, subs, true);
            }
        }
    } else {
        printf("BSDF: %s not found.\n", type.c_str());
    }
    Error("Unknown BSDF");
    return nullptr;
}

std::shared_ptr<const TextureRGB> ParseTexture(pugi::xml_node node, SubsT &subs) {

    replace_func(subs, node);
    std::string type = node.attribute("type").value();
    if (type == "bitmap") {
        std::string filename = "";
        Float sScale = 1.0;
        Float tScale = 1.0;
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "filename") {
                filename = child.attribute("value").value();
            } else if (name == "uvscale") {
                sScale = tScale = std::stof(child.attribute("value").value());
            }
        }
        return std::make_shared<BitmapTextureRGB>(filename, Vector2(sScale, tScale));
    }
    Error("Unknown texture type");
    return nullptr;
}

template <int N>
std::shared_ptr<const Texture<N>> ParseNDMap(pugi::xml_node node, const TextureMap &textureMap, SubsT &subs) {

    replace_func(subs, node);
    std::shared_ptr<const TextureRGB> reflectance;
    std::string nodeType = node.name();
    if (nodeType == "texture") {
        reflectance = ParseTexture(node, subs);
    } else if (nodeType == "ref") {
        pugi::xml_attribute idAttr = node.attribute("id");
        if (!idAttr.empty()) {
            auto texIt = textureMap.find(idAttr.value());
            if (texIt == textureMap.end()) {
                printf("ref: %s, %s, %s\n", idAttr.name(), idAttr.value(), idAttr.as_string());
                Error("ref not found");
            }
            reflectance = texIt->second;
        } else {
            printf("ref: %s, %s, %s\n", idAttr.name(), idAttr.value(), idAttr.as_string());
            Error("ref not found");
        }
    } else {
        std::string value = node.attribute("value").value();
        TVector<Float, N> v;
        if (N == 1) {
            v[0] = std::stof(value);
        } else if (N == 3) {
            Vector3 v3 = ParseVector3(value);
            v[0] = v3[0];
            v[1] = v3[1];
            v[2] = v3[2];
        } else {
            Error("unsupported texture dimension");
        }
        return std::make_shared<const ConstantTexture<N>>(v);
    }

    if (N == 1) {
        return std::dynamic_pointer_cast<const Texture<N>>(reflectance->ToTexture1D());
    } else if (N == 3) {
        return std::dynamic_pointer_cast<const Texture<N>>(reflectance->ToTexture3D());
    }
    Error("unsupported texture dimension");
    return nullptr;
}

std::shared_ptr<const TextureRGB> Parse3DMap(pugi::xml_node node, const TextureMap &textureMap, SubsT &subs) {
    return ParseNDMap<3>(node, textureMap, subs);
}

std::shared_ptr<const Texture1D> Parse1DMap(pugi::xml_node node, const TextureMap &textureMap, SubsT &subs) {
    return ParseNDMap<1>(node, textureMap, subs);
}

std::shared_ptr<const Light> ParseEmitter(pugi::xml_node node,
                                          std::shared_ptr<const EnvLight> &envLight, SubsT &subs) {
    replace_func(subs, node);

    std::string type = node.attribute("type").value();
    std::string id = node.attribute("id").value();
    NANOLOG_INFO("Light {}", id);

    if (type == "point") {
        Vector3 pos(Float(0.0), Float(0.0), Float(0.0));
        Vector3 intensity(Float(1.0), Float(1.0), Float(1.0));
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "position") {
                Float x = 0.0;
                Float y = 0.0;
                Float z = 0.0;
                if (!child.attribute("x").empty())
                    x = std::stof(child.attribute("x").value());
                if (!child.attribute("y").empty())
                    y = std::stof(child.attribute("y").value());
                if (!child.attribute("z").empty())
                    z = std::stof(child.attribute("z").value());
                pos = Vector3(x, y, z);
            } else if (name == "intensity") {
                intensity = ParseVector3(child.attribute("value").value());
            }
        }
        return std::make_shared<PointLight>(Float(1.0), pos, intensity);
    } 
    else if (type == "ies") {
        // AnimatedTransform toWorld =
        //     MakeAnimatedTransform(Matrix4x4::Identity(), Matrix4x4::Identity());
        Matrix4x4 toWorld = Matrix4x4::Identity();
        Vector3 intensity(Float(1.0), Float(1.0), Float(1.0));
        std::string ies_fname("");
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    Matrix4x4 m = ParseTransform(child, subs);
                    toWorld = ParseTransform(child, subs);
                } 
                // else if (std::string(child.name()) == "animation") {
                //     toWorld = ParseAnimatedTransform(child);
                // }
            } else if (name == "intensity") {
                intensity = ParseVector3(child.attribute("value").value());
            } else if (name == "filename") {
                ies_fname = child.attribute("value").value();
            }
        }

        // std::cout << "Loaded a IES light" << std::endl;
        return std::make_shared<IESLight>(Float(1.0), toWorld, intensity, ies_fname);
    } 
    else if (type == "spot") {
        AnimatedTransform toWorld =
            MakeAnimatedTransform(Matrix4x4::Identity(), Matrix4x4::Identity());
        Vector3 intensity(Float(1.0), Float(1.0), Float(1.0));
        Float cutoffAngle(20.f), beamWidth(cutoffAngle * 3.f/4.f);

        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    Matrix4x4 m = ParseTransform(child, subs);
                    toWorld = MakeAnimatedTransform(m, m);
                } else if (std::string(child.name()) == "animation") {
                    toWorld = ParseAnimatedTransform(child, subs);
                }
            } else if (name == "intensity") {
                intensity = ParseVector3(child.attribute("value").value());
            } else if (name == "cutoffAngle") {
                cutoffAngle = std::stof(child.attribute("value").value());
            } else if (name == "beamWidth") {
                beamWidth = std::stof(child.attribute("value").value());
            }
        }

        cutoffAngle = c_PI*cutoffAngle/180.f;
        beamWidth = c_PI*beamWidth/180.f;

        NANOLOG_INFO("SpotLight[\ncutOffAngle(rad): {}\nbeamWidth(rad): {}\nintensity: {}\ntransform: {}\n]", 
            cutoffAngle, beamWidth, intensity.transpose(),
            Interpolate(toWorld, 0.f));

        // cutoffAngle = c_PI*cutoffAngle/180.f;
        // beamWidth = c_PI*beamWidth/180.f;

        return std::make_shared<SpotLight>(Float(1.0), toWorld, intensity, 
            cutoffAngle, beamWidth);
    }
    else if (type == "collimatedbeam") {
        Matrix4x4 toWorld = Matrix4x4::Identity();
        Vector3 intensity(Float(1.0), Float(1.0), Float(1.0));
        Float radius(0.01f);
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    toWorld = ParseTransform(child, subs);  
                } 
            } else if (name == "intensity") {
                intensity = ParseVector3(child.attribute("value").value());
            } else if (name == "radius") {
                radius = std::stof(child.attribute("value").value());
            } 
        }
        // std::cout << "CollimatedLight[" << std::endl
        //         << "radius : " << radius << std::endl
        //         << "intensity : " << intensity << std::endl
        //         << "transform : " << toWorld << std::endl
        //         << "]" << std::endl;
        return std::make_shared<CollimatedLight>(Float(1.0), toWorld, radius, intensity);
    } 
    else if (type == "envmap") {
        std::string filename = "";
        AnimatedTransform toWorld =
            MakeAnimatedTransform(Matrix4x4::Identity(), Matrix4x4::Identity());
        for (auto child : node.children()) {
            std::string name = child.attribute("name").value();
            if (name == "filename") {
                filename = child.attribute("value").value();
            } else if (name == "toWorld") {
                if (std::string(child.name()) == "transform") {
                    Matrix4x4 m = ParseTransform(child, subs);
                    toWorld = MakeAnimatedTransform(m, m);
                } else if (std::string(child.name()) == "animation") {
                    toWorld = ParseAnimatedTransform(child, subs);
                }
            }
        }
        envLight = std::make_shared<EnvLight>(Float(1.0), toWorld, filename);
        return envLight;
    }

    Error("Unsupported emitter");
    return nullptr;
}

std::shared_ptr<DptOptions> ParseDptOptions(pugi::xml_node node, SubsT &subs) {
    replace_func(subs, node);

    std::shared_ptr<DptOptions> dptOptions = std::make_shared<DptOptions>();
    for (auto child : node.children()) {
        std::string name = child.attribute("name").value();
        // std::cout << "Parsing " << name << std::endl;
        NANOLOG_DEBUG("Parsing {}", name);
        
        if (name == "integrator") {
            dptOptions->integrator = child.attribute("value").value();
        } else if (name == "spp") {
            dptOptions->spp = std::stoi(child.attribute("value").value());
        } else if (name == "bidirectional") {
            dptOptions->bidirectional = child.attribute("value").value() == std::string("true");
        } else if (name == "numinitsamples") {
            dptOptions->numInitSamples = std::stoi(child.attribute("value").value());
        } else if (name == "largestepprob") {
            dptOptions->largeStepProbability = std::stof(child.attribute("value").value());
        } else if (name == "largestepscale") {
            dptOptions->largeStepProbScale = std::stof(child.attribute("value").value());
        } else if (name == "mindepth") {
            dptOptions->minDepth = std::stoi(child.attribute("value").value());
        } else if (name == "maxdepth") {
            dptOptions->maxDepth = std::stoi(child.attribute("value").value());
        } else if (name == "directspp") {
            dptOptions->directSpp = std::stoi(child.attribute("value").value());
        } else if (name == "perturbstddev") {
            dptOptions->perturbStdDev = std::stof(child.attribute("value").value());
        } else if (name == "roughnessthreshold") {
            dptOptions->roughnessThreshold = std::stof(child.attribute("value").value());
        } else if (name == "uniformmixprob") {
            dptOptions->uniformMixingProbability = std::stof(child.attribute("value").value());  
        } else if (name == "numchains") {
            dptOptions->numChains = std::stoi(child.attribute("value").value());
        } else if (name == "seedoffset") {
            dptOptions->seedOffset = std::stoi(child.attribute("value").value());
        } else if (name == "reportintervalspp") {
            dptOptions->reportIntervalSpp = std::stoi(child.attribute("value").value());
        } else if (name == "uselightcoordinatesampling") {
            dptOptions->useLightCoordinateSampling =
                child.attribute("value").value() == std::string("true");
        } else if (name == "largestepmultiplexed") {
            dptOptions->largeStepMultiplexed =
                child.attribute("value").value() == std::string("true");
        } else if (name == "h2mc") {
            dptOptions->h2mc = child.attribute("value").value() == std::string("true");
        } else if (name == "mala") {
            dptOptions->mala = child.attribute("value").value() == std::string("true");
        } else if (name == "mala-stepsize") {
            dptOptions->malaStepsize = std::stof(child.attribute("value").value());
        } else if (name == "mala-gn") {
            dptOptions->malaGN = std::stof(child.attribute("value").value());
        } else if (name == "samplecache") {
            dptOptions->sampleFromGlobalCache = child.attribute("value").value() == std::string("true");
        } else {
            std::cerr << "Unknown dpt option:" << name << std::endl;
        }
    }
    return dptOptions;
}

void ParseScene(pugi::xml_node node,
    std::shared_ptr<DptOptions> &options,
    std::shared_ptr<const Camera> &camera,
    std::vector<std::shared_ptr<const Shape>> &objs,
    std::vector<std::shared_ptr<const Light>> &lights,
    std::shared_ptr<const EnvLight> &envLight,
    std::map<std::string, std::shared_ptr<const BSDF>> &bsdfMap,
    std::map<std::string, std::shared_ptr<const TextureRGB>> &textureMap,
    std::string &outputName,
    SubsT &subs
    ) {


    replace_func(subs, node);

    for (auto child : node.children()) {
        std::string name = child.name();
        if (name == "sensor") {
            camera = ParseSensor(child, outputName, subs);
        } else if (name == "shape") {
            std::shared_ptr<const Light> areaLight;
            objs.push_back(ParseShape(child, bsdfMap, textureMap, areaLight, subs));
            if (areaLight.get() != nullptr) {
                lights.push_back(areaLight);
            }
        } else if (name == "bsdf") {
            std::string id = child.attribute("id").value();
            bsdfMap[id] = ParseBSDF(child, textureMap, subs);
        } else if (name == "emitter") {
            lights.push_back(ParseEmitter(child, envLight, subs));
        } else if (name == "texture") {
            std::string id = child.attribute("id").value();
            textureMap[id] = ParseTexture(child, subs);
        } else if (name == "dpt") {
            options = ParseDptOptions(child, subs);
            std::cout << 
                "spp : " << options->spp << std::endl <<
                "maxDepth : " << options->maxDepth << std::endl;
        } else if (name == "include") {
            std::string filename = child.attribute("filename").value();
            pugi::xml_document doc;
            pugi::xml_parse_result result = doc.load_file(filename.c_str());
            if (result) {
                ParseScene(doc.child("scene"),
                    options, camera, objs, lights, envLight, bsdfMap, textureMap, outputName, subs);
            }
        } else if (name == "default") {
            std::string defaultName = child.attribute("name").value();
            std::string defaultValue = child.attribute("value").value();
            // donot replace if the key is already found
            if (subs.find(defaultName) != subs.end()) {
                // std::cout << "Ignoring default for " << defaultName << std::endl;
                NANOLOG_WARN("Ignoring default for {}", defaultName);
                continue;
            }
            subs[defaultName] = defaultValue;
        }
    }
    // return std::unique_ptr<Scene>(
    //     new Scene(options, camera, objs, lights, envLight, outputName));
}

std::unique_ptr<Scene> ParseScene(const std::string &filename, const std::string &outFn, SubsT &subs) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    std::unique_ptr<Scene> scene;
    if (result) {
        std::shared_ptr<DptOptions> options = std::make_shared<DptOptions>();
        std::shared_ptr<const Camera> camera;
        std::vector<std::shared_ptr<const Shape>> objs;
        std::vector<std::shared_ptr<const Light>> lights;
        std::shared_ptr<const EnvLight> envLight;
        std::map<std::string, std::shared_ptr<const BSDF>> bsdfMap;
        std::map<std::string, std::shared_ptr<const TextureRGB>> textureMap;
        
        std::regex vowel_re("xml");
        std::string outputName = std::regex_replace(filename, vowel_re, "exr");
        if (outFn.size() != 0)
            outputName = outFn;
        ParseScene(doc.child("scene"), 
            options, camera, objs, lights, envLight, bsdfMap, textureMap, outputName, subs);

        scene = std::unique_ptr<Scene>(
            new Scene(options, camera, objs, lights, envLight, outputName));
    } else {
        std::cerr << "Error description: " << result.description() << std::endl;
        std::cerr << "Error offset: " << result.offset << std::endl;
        Error("Parse error");
    }
    return scene;
}
