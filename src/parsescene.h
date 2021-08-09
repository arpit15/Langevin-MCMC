#pragma once

#include "scene.h"

#include <string>
#include <memory>

std::unique_ptr<Scene> ParseScene(const std::string &filename, 
            const std::string &outFn,
            std::unordered_map<std::string, std::string> &subs);
