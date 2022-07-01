#include "args.hxx"
// #include "spdlog/spdlog.h"
#include "nanolog.hh"

#include "parsescene.h"
#include "pathtrace.h"
#include "directintegrator.h"
#include "mlt.h"
#include "image.h"
#include "camera.h"
#include "texturesystem.h"
#include "parallel.h"
#include "path.h"
#include "timer.h"

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/resource.h>
#ifdef __APPLE__
    #include <limits.h>
#else
    #include <linux/limits.h>   // PATH_MAX
#endif
#include <libgen.h>
#include <filesystem>
#include <regex>

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
    NANOLOG_INFO("Langevin MCMC dpt version 1.1");
    NANOLOG_INFO("Running with {} threads.", NumSystemCores());
}

void DptCleanup() {
    TextureSystem::Destroy();
    TerminateWorkerThreads();
}

// https://www.geeksforgeeks.org/tokenizing-a-string-cpp/
std::vector<std::string> tokenize(const std::string &line) {
    // std::string delimiter = "=";
    std::stringstream check1(line);
    std::vector<std::string> tokens;
    std::string intermediate;
      
    // Tokenizing w.r.t. space ' '
    while(getline(check1, intermediate, '='))
    {
        tokens.push_back(intermediate);
    }
      
    // Printing the token vector
    // for(int i = 0; i < tokens.size(); i++)
    //     cout << tokens[i] << '\n';
    return tokens;
}

std::vector<std::string> tokenize2(const std::string &value) {
    std::regex rgx("=");
    std::sregex_token_iterator first{begin(value), end(value), rgx, -1}, last;
    std::vector<std::string> list{first, last};
    return list;
}

void parseExtraArgs(std::unordered_map<std::string, std::string> &subs, args::ArgumentParser &parser, args::ValueFlagList<std::string> &subsArgs) {
    
    if (subsArgs) { 
        for (const auto ch: args::get(subsArgs)) { 
            // split at =
            // std::cout << "D: " << ch << std::endl; 
            NANOLOG_DEBUG("D: {}", ch);
            const std::vector<std::string> tokens = tokenize2(ch);
            if (tokens.size() == 2) {
                subs[tokens[0]] = tokens[1];
            } else {
                std::cout << "Invalid substitution arguments : " << ch << std::endl;
                exit(0);
            }
            

        } 
    }
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        return 0;
    }

    args::ArgumentParser parser("This is a test program.", "This goes after the options.");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlagList<std::string> subsArgs(parser, "subsArgs", "The substitution Args", {'D'});
    args::Group group(parser, "This group is all exclusive:", args::Group::Validators::DontCare);
    args::Flag pathlib(group, "pathlib", "Compile pathlib", {'p', "pathlib"});
    args::Flag hessians(group, "hessians", "Compile hessians", {"h2mc", "bidirpathlib"});
    args::Flag mala(group, "mala", "Compile mala", {'m', "mala"});
    args::ValueFlag<int> maxDervDepth(parser, "maxDervDepth", "Maximum Derivative Depth", {"maxD"}, 8);
    args::ValueFlag<int> seedoffset(parser, "seedOffset", "Seed Offset", {"seedOffset"}, 0);
    args::PositionalList<std::string> filenamesArgs(parser, "filenamesArgs", "Filename args");

    args::Flag tonemap(parser, "tonemap", "tonemap", {'t'}, false);

    args::ValueFlag<std::string> outFn(parser, "outFn", "Output Filename", {'o'}, "");

    args::ValueFlag<std::string> logLevel(parser, "logLevel", "Logging level", {"L"}, "info");

    std::unordered_map<std::string, std::string> subs;

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
        std::cout << parser;
        return 0;
    }
    catch (args::ParseError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // default 
    // spdlog::set_level(nanolog::info);
    // // get log level
    // if (logLevel.Get() == "info"){
    //     spdlog::set_level(nanolog::info);
    // }
    // else if (logLevel.Get() == "debug") {
    //     spdlog::set_level(nanolog::debug);
    // }
    // else if (logLevel.Get() == "err") {
    //     spdlog::set_level(nanolog::err);
    // }
    // else if (logLevel.Get() == "warn") {
    //     spdlog::set_level(nanolog::warn);
    // }
    // else if (logLevel.Get() == "trace") {
    //     spdlog::set_level(nanolog::trace);
    // }

    // order 
    // trace < debug < info < warn < error < fatal
    nanolog::set_level(nanolog::kINFO);
    // // get log level
    if (logLevel.Get() == "info"){
        nanolog::set_level(nanolog::kINFO);
    }
    else if (logLevel.Get() == "debug") {
        nanolog::set_level(nanolog::kDEBUG);
    }
    else if (logLevel.Get() == "err") {
        nanolog::set_level(nanolog::kERROR);
    }
    else if (logLevel.Get() == "warn") {
        nanolog::set_level(nanolog::kWARN);
    }
    else if (logLevel.Get() == "trace") {
        nanolog::set_level(nanolog::kTRACE);
    }

    try {
        DptInit();
        
        std::vector<std::string> filenames;

        parseExtraArgs(subs, parser, subsArgs);

        if (pathlib) {
            CompilePathFuncLibrary(false, maxDervDepth.Get());
        }
        if (hessians) {
            // hessians
            CompilePathFuncLibrary(true, maxDervDepth.Get());
        }
        if (mala) {
            // mala
            Timer timer;
            Tick(timer);
            CompilePathFuncLibrary2(maxDervDepth.Get());
            Float elapsed = Tick(timer);
            std::cout << "MALA compile time:" << elapsed << std::endl;
        }

        if ( !(pathlib || hessians || mala) &&
            !filenamesArgs) {
            std::cout << "No fname were found" << std::endl;
            return 0;
        } else {
            for (const std::string fname: args::get(filenamesArgs)) { 
                // std::cout << "fname: " << fname << std::endl; 
                filenames.push_back(fname);
            }
        }

        // call invoking dir
        std::string cwd = getcwd(NULL, 0);
        std::string exeDir = getCurrExeDir();
        
        for (std::string filename : filenames) {
            if (filename.rfind('/') != std::string::npos &&
                chdir(filename.substr(0, filename.rfind('/')).c_str()) != 0) {
                Error("chdir failed");
            }
            std::string basename = filename;
            if (filename.rfind('/') != std::string::npos) {
                basename = filename.substr(filename.rfind('/') + 1);
            }

            std::unique_ptr<Scene> scene = ParseScene(basename, outFn.Get(), subs);
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
            if (integrator == "direct"){
                // return to the original executable dir after parsing scene
                if (chdir(cwd.c_str()) != 0) {
                    Error("chdir failed");
                }
                Direct(scene.get());
            }
            else if (integrator == "mc") {
                std::shared_ptr<const PathFuncLib> library =
                    BuildPathFuncLibrary(scene->options->bidirectional, maxDervDepth);
                
                // return to the original executable dir after parsing scene
                if (chdir(cwd.c_str()) != 0) {
                    Error("chdir failed");
                }
                PathTrace(scene.get(), library, tonemap);
            } else if (integrator == "mcmc") {
                if (scene->options->mala) { // MALA builds only first-order derivatives
                    std::shared_ptr<const PathFuncLib> library = 
                        BuildPathFuncLibrary2(maxDervDepth);
                    
                    // return to the original executable dir after parsing scene
                    if (chdir(cwd.c_str()) != 0) {
                        Error("chdir failed");
                    }
                    MLT(scene.get(), library, tonemap);
                } else {    // Hessian otherwise 
                    std::shared_ptr<const PathFuncLib> library =
                        BuildPathFuncLibrary(scene->options->bidirectional, maxDervDepth);
                    // return to the original executable dir after parsing scene
                    if (chdir(cwd.c_str()) != 0) {
                        Error("chdir failed");
                    }
                    MLT(scene.get(), library, tonemap);
                }
            } else {
                Error("Unknown integrator");
            }
            
        }
        DptCleanup();
    } 
    catch (std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        TerminateWorkerThreads();
        return 1;
    }
    
    return 0;
}
