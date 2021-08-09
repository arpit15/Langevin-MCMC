cmake_minimum_required(VERSION 3.15)
set (CMAKE_CXX_STANDARD 17)

project(LMC CXX)

add_definitions(-DSINGLE_PRECISION)

find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package(TBB)

file(GLOB SRC_FILES
     "src/*.h"
     "src/*.cpp"
)

# message(INFO "${SRC_FILES}")
# list(REMOVE_ITEM SRC_FILES
# "${PROJECT_SOURCE_DIR}/src/iesarea.cpp"    
# "${PROJECT_SOURCE_DIR}/src/iesarea.h"
# )

add_subdirectory(nanoflann)
# add_subdirectory(oiio)

# libgoogle-perftools-dev
find_library (PROFILE_LIB profiler)
if (NOT PROFILE_LIB)
  message (STATUS "Unable to find -lprofiler")
else ()
  message (STATUS "Found -lprofiler: ${PROFILE_LIB}")
endif ()

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
    profiler
)

configure_file(ispc/bin/ispc ispc COPYONLY)

# tests
add_executable(check_cond_der 
tests/check_cond_der.cpp
src/chad.cpp
src/alignedallocator.cpp
)

target_include_directories(check_cond_der
PRIVATE src
)

target_link_libraries(check_cond_der
Eigen3::Eigen
dl
)

add_executable(load_ply 
tests/load_ply.cpp
src/chad.cpp
src/alignedallocator.cpp
src/parseply.cpp
)

target_include_directories(load_ply
PRIVATE src
)

target_link_libraries(load_ply
Eigen3::Eigen
dl
)