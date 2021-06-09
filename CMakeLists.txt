cmake_minimum_required(VERSION 3.15)
set (CMAKE_CXX_STANDARD 11)

project(LMC CXX)

add_definitions(-DSINGLE_PRECISION)

find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package(TBB)

file(GLOB SRC_FILES
     "src/*.h"
     "src/*.cpp"
)

add_subdirectory(nanoflann)
# add_subdirectory(oiio)

add_executable(dpt ${SRC_FILES})
target_include_directories(dpt 
    PRIVATE
    ../embree3/include
    ../oiio/dist/linux64/include
)
target_link_directories(dpt
    PRIVATE
    ../embree3/lib
    ../oiio/dist/linux64/lib
)
target_link_libraries (dpt 
    Eigen3::Eigen
    tbb
    nanoflann::nanoflann
    OpenImageIO
    embree3
    boost_system
    dl 
    z
    pthread
)

configure_file(ispc/bin/ispc ispc COPYONLY)