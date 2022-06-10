# cmake_minimum_required(VERSION 3.15)
cmake_minimum_required(VERSION 3.18...3.22)
set (CMAKE_CXX_STANDARD 17)

project(LMC CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(LMC_OUTLIER_REMOVAL "Add outlier removal step in LMC" False)
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." True)
option(BUILD_PYTHON "BUILD python bindings" False)

message("PYTHON selection -- ${BUILD_PYTHON}")

if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

if (${LMC_OUTLIER_REMOVAL}) 
  message("++ Outlier removal added")
  add_definitions(-DREMOVE_OUTLIERS)
endif()

add_definitions(-DSINGLE_PRECISION)

find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package(TBB)
find_package(embree 3.8.0 REQUIRED
PATHS /home/arpita1/projects/embree/build-Release
)

message(STATUS "embree dirs - ${EMBREE_INCLUDE_DIRS}")
message(STATUS "embree libs - ${EMBREE_LIBRARY}")

file(GLOB SRC_FILES
     "src/*.h"
     "src/*.cpp"
)

set(SRC_FILES_NOMAIN ${SRC_FILES})

# message(INFO "${SRC_FILES}")
# list(REMOVE_ITEM SRC_FILES
# "${PROJECT_SOURCE_DIR}/src/iesarea.cpp"    
# "${PROJECT_SOURCE_DIR}/src/iesarea.h"
# )

list(REMOVE_ITEM SRC_FILES_NOMAIN
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

include_directories(/usr/local/include)

add_subdirectory(nanoflann)
# include(build_embree.cmake)
find_package(OpenImageIO REQUIRED)
find_package(TBB REQUIRED)

# libgoogle-perftools-dev
# find_library (PROFILE_LIB profiler)
# if (NOT PROFILE_LIB)
#   message (STATUS "Unable to find -lprofiler")
# else ()
#   message (STATUS "Found -lprofiler: ${PROFILE_LIB}")
# endif ()

# message(STATUS "oiio lib: "${OpenImageIO_LIBRARIES})
# message(STATUS "tbb lib: "${TBB_LIBRARIES})

add_library(dpt_lib 
# SHARED 
STATIC
${SRC_FILES_NOMAIN})
target_include_directories(dpt_lib  
    PRIVATE
    ${EMBREE_INCLUDE_DIRS}
    ${OpenImageIO_INCLUDES}
)

set_target_properties(dpt_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_directories(dpt_lib
    PRIVATE
    /usr/local/lib
)


target_link_libraries (dpt_lib 
    profiler
    Eigen3::Eigen
    nanoflann::nanoflann
    OpenImageIO
    ${EMBREE_LIBRARY}
    boost_system
    dl 
    z
    pthread
    tbb
)


add_executable(dpt src/main.cpp)
target_include_directories(dpt 
    PRIVATE
    ${EMBREE_INCLUDE_DIRS}
    ${OpenImageIO_INCLUDES}
)

# target_link_directories(dpt
#     PRIVATE
#     /usr/local/lib
# )

target_link_libraries (dpt 
    dpt_lib
)


configure_file(ispc/bin/ispc ispc COPYONLY)



if(BUILD_PYTHON)
    add_subdirectory(python_bind)
endif()

# tests
# add_executable(check_cond_der 
# tests/check_cond_der.cpp
# src/chad.cpp
# src/alignedallocator.cpp
# )

# target_include_directories(check_cond_der
# PRIVATE src
# )

# target_link_libraries(check_cond_der
# Eigen3::Eigen
# dl
# )

# add_executable(load_ply 
# tests/load_ply.cpp
# src/chad.cpp
# src/alignedallocator.cpp
# src/parseply.cpp
# )

# target_include_directories(load_ply
# PRIVATE src
# )

# target_link_libraries(load_ply
# Eigen3::Eigen
# dl
# )

# # brdf tests
# add_executable(brdf_test 
#     tests/sample_bsdf.cpp
#     ${SRC_FILES_NOMAIN}
# )

# target_include_directories(brdf_test
# PRIVATE src
# )

# target_link_libraries(brdf_test
#     Eigen3::Eigen
#     tbb
#     nanoflann::nanoflann
#     OpenImageIO
#     embree3
#     boost_system
#     dl 
#     z
#     pthread
# )