#include "parsescene.h"
#include "pathtrace.h"
#include "mlt.h"
#include "image.h"
#include "camera.h"
#include "texturesystem.h"
#include "parallel.h"
#include "path.h"

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/resource.h>
#include <linux/limits.h>   // PATH_MAX
#include <libgen.h>
#include <filesystem>

namespace fs = std::filesystem;

std::string getCurrExeDir() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    const char *path;
    if (count != -1) {
        path = dirname(result);
    }
    return path;
}
void DptInit() {
    TextureSystem::Init();
    std::cout << "Langevin MCMC dpt version 1.0" << std::endl;
    std::cout << "Running with " << NumSystemCores() << " threads." << std::endl;
}

void DptCleanup() {
    TextureSystem::Destroy();
    TerminateWorkerThreads();
}

 

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        return 0;
    }

    try {
        DptInit();

        bool compilePathLib = false;
        bool compileBidirPathLib = false;
        bool compileBidirPathLib2 = false;
        int maxDervDepth = 8;
        std::vector<std::string> filenames;
        int seedoffset = 0;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--compile-pathlib") {
                compilePathLib = true;
            } else if (std::string(argv[i]) == "--compile-bidirpathlib") {
                compileBidirPathLib = true;
            } else if (std::string(argv[i]) == "--compile-bidirpathlib2") {
                compileBidirPathLib2 = true; 
            } else if (std::string(argv[i]) == "--max-derivatives-depth") {
                maxDervDepth = std::stoi(std::string(argv[++i]));
            } else if (std::string(argv[i]) == "--seedoffset") {
                seedoffset = std::stoi(std::string(argv[++i]));
            }
            else {
                filenames.push_back(std::string(argv[i]));
            }
        }

        if (compilePathLib) {
            CompilePathFuncLibrary(false, maxDervDepth);
        }
        if (compileBidirPathLib) {
            // hessians
            CompilePathFuncLibrary(true, maxDervDepth);
        }
        if (compileBidirPathLib2) {
            // mala
            CompilePathFuncLibrary2(maxDervDepth);
        }

        // call invoking dir
        std::string cwd = getcwd(NULL, 0);
        std::string exeDir = getCurrExeDir();

        std::cout << "current dir : " << cwd << std::endl;
        std::cout << "Exe dir : " << exeDir << std::endl;

        for (std::string filename : filenames) {
            if (filename.rfind('/') != std::string::npos &&
                chdir(filename.substr(0, filename.rfind('/')).c_str()) != 0) {
                Error("chdir failed");
            }
            std::string basename = filename;
            if (filename.rfind('/') != std::string::npos) {
                basename = filename.substr(filename.rfind('/') + 1);
            }

            std::unique_ptr<Scene> scene = ParseScene(basename);
            // use the xml dirname
            fs::path xmlPath(filename);
            scene->outputName =  xmlPath.parent_path() / (scene->outputName); 
            
            std::cout << "Output filename : " << scene->outputName << std::endl;
            // return to the original executable dir after parsing scene
            if (chdir(exeDir.c_str()) != 0) {
                Error("chdir failed");
            }

            std::string integrator = scene->options->integrator;
                
            scene->options->seedOffset = seedoffset;
            
            std::cout << "Scene parsing done !" << std::endl;
            if (integrator == "mc") {
                std::shared_ptr<const PathFuncLib> library =
                    BuildPathFuncLibrary(scene->options->bidirectional, maxDervDepth);
                
                // return to the original executable dir after parsing scene
                if (chdir(cwd.c_str()) != 0) {
                    Error("chdir failed");
                }
                PathTrace(scene.get(), library);
            } else if (integrator == "mcmc") {
                if (scene->options->mala) { // MALA builds only first-order derivatives
                    std::shared_ptr<const PathFuncLib> library = 
                        BuildPathFuncLibrary2(maxDervDepth);
                    
                    // return to the original executable dir after parsing scene
                    if (chdir(cwd.c_str()) != 0) {
                        Error("chdir failed");
                    }
                    MLT(scene.get(), library);
                } else {    // Hessian otherwise 
                    std::shared_ptr<const PathFuncLib> library =
                        BuildPathFuncLibrary(scene->options->bidirectional, maxDervDepth);
                    // return to the original executable dir after parsing scene
                    if (chdir(cwd.c_str()) != 0) {
                        Error("chdir failed");
                    }
                    MLT(scene.get(), library);
                }
            } else {
                Error("Unknown integrator");
            }
            
        }
        DptCleanup();
    } catch (std::exception &ex) {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
