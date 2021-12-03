#pragma once

#include "scene.h"
#include "bsdf.h"
#include "pugixml.hpp"
#include "texture.h"

#include <string>
#include <memory>
#include <map>

using TextureMap = std::map<std::string, std::shared_ptr<const TextureRGB>>;
typedef std::unordered_map<std::string, std::string> SubsT;

std::shared_ptr<const BSDF> ParseBSDF(pugi::xml_node node,
                                      const TextureMap &textureMap,
                                      SubsT &subs,
                                      bool twoSided = false);
                                      
std::unique_ptr<Scene> ParseScene(const std::string &filename, 
            const std::string &outFn,
            std::unordered_map<std::string, std::string> &subs);
