#include <limits>
#include<unistd.h>
#include <filesystem>
#ifdef __APPLE__
    #include <limits.h>
#else
    #include <linux/limits.h>   // PATH_MAX
#endif
#include <libgen.h>

#include "nanolog.hh"

#include "pythonscene.hpp"

#include "camera.h"
#include "parallel.h"
#include "progressreporter.h"
#include "image.h"
#include "parsescene.h"
#include "scene.h"
#include "shape.h"
#include "texturesystem.h"
#include "utils.h"

#include "path.h"
#include "pathtrace.h"
#include "directintegrator.h"
#include "mlt.h"

template <typename T> constexpr auto Epsilon         = std::numeric_limits<T>::epsilon() / 2;
template <typename T> constexpr auto RayEpsilon      = Epsilon<T> * 1500;

std::string getCurrExeDir() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    const char *path;
    if (count != -1) {
        path = dirname(result);
    }
    return path;
}

// PyScene::~PyScene(){
	// TextureSystem::Destroy();
 //    TerminateWorkerThreads();
// }

PyScene::PyScene(nb::str &filename_nb, 
             nb::str &outFn_nb,
            nb::dict &subsDict)
	
{
	nanolog::set_level(nanolog::kERROR);

	TextureSystem::Init();
	std::string filename = std::string(filename_nb.c_str());
	std::string outFn = std::string(outFn_nb.c_str());

	std::unordered_map<std::string, std::string> _subs;
	if(subsDict){
		for(auto [k,v]: subsDict){
			_subs[std::string(nb::str(k).c_str())] = std::string(nb::str(v).c_str());
		}
	}

	std::string cwd = getcwd(NULL, 0);
	std::string exeDir = getCurrExeDir();
	if (filename.rfind('/') != std::string::npos &&
			chdir(filename.substr(0, filename.rfind('/')).c_str()) != 0) {
			Error("chdir failed");
	}
	std::string basename = filename;
	if (filename.rfind('/') != std::string::npos) {
			basename = filename.substr(filename.rfind('/') + 1);
	}
	_scene = ParseScene(basename, outFn, _subs);
	// return to the calling dir after parsing scene
	if (chdir(cwd.c_str()) != 0) {
			Error("chdir failed");
	}

	const Camera *camera = _scene->camera.get();
	pixelHeight = GetPixelHeight(camera);
	pixelWidth = GetPixelWidth(camera);

}

void PyScene::set_log_level(const std::string &loglevel){
	if (loglevel == "trace"){
        nanolog::set_level(nanolog::kTRACE);
    }
    else if (loglevel == "debug") {
        nanolog::set_level(nanolog::kDEBUG);
    }
    else if (loglevel == "err") {
        nanolog::set_level(nanolog::kERROR);
    }
    else if (loglevel == "warn") {
        nanolog::set_level(nanolog::kWARN);
    }
    else {
        nanolog::set_level(nanolog::kINFO);
    }
}

trafo PyScene::camera_trafo(){
	const Camera *camera = _scene->camera.get();

	size_t shape[2] = {4,4};
	float *trafodata = new float[shape[0]*shape[1]];
	// Delete 'data' when the 'owner' capsule expires
    nb::capsule trafoowner(trafodata, [](void *p) noexcept {
       delete[] (float *) p;
    });
	trafo outmat(trafodata, 2, shape, trafoowner); 

	Matrix4x4 currtrafo = Interpolate(camera->worldToCamera, 0.f);
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			outmat(i,j) = currtrafo(i,j);
		}
	}

	// std::cout << "camera trafo copied!" << std::endl;

	return outmat;
}

void PyScene::camera_rays(tensorhw3 &rayorg, tensorhw3 &raydir){
	// std::cout << "Num cores: " << NumSystemCores() << std::endl;
	const Camera *camera = _scene->camera.get();

	const int tileSize = 16;
	const int nXTiles = (pixelWidth + tileSize - 1) / tileSize;
	const int nYTiles = (pixelHeight + tileSize - 1) / tileSize;
	ProgressReporter reporter(nXTiles * nYTiles);

	ParallelFor(
		[&](const Vector2i tile) {
		const int x0 = tile[0] * tileSize;
		const int x1 = std::min(x0 + tileSize, pixelWidth);
		const int y0 = tile[1] * tileSize;
		const int y1 = std::min(y0 + tileSize, pixelHeight);

		for (size_t y = y0; y < y1; y++) {
			for (size_t x = x0; x < x1; x++) {
				auto screenPos = Vector2( 
					(float)x/(float)pixelWidth, 
					(float)y/(float)pixelHeight
				);
				RaySegment raySeg;
				SamplePrimary(camera,
                   screenPos,
                   0.f,
                   raySeg);

				for (size_t i = 0; i < 3; ++i)
				{
					rayorg(y, x, i) = raySeg.ray.org(i);
					raydir(y, x, i) = raySeg.ray.dir(i);
				} 
			}
		}
		reporter.Update(1);
	}, 
		Vector2i(nXTiles, nYTiles)
	);

	std::cout << std::endl;
	TerminateWorkerThreads();
}

void PyScene::render(nb::str &outfn, const bool tonemap, nb::str &_libpath){

	_scene->outputName = std::string(outfn.c_str());
	std::string libpath = std::string(_libpath.c_str());
	std::cout << "libpath: " << libpath << std::endl;

	std::string integrator = _scene->options->integrator;
	if (integrator == "direct"){
        Direct(_scene.get());
    }
    else if (integrator == "mc") {
        std::shared_ptr<const PathFuncLib> library =
            BuildPathFuncLibrary(_scene->options->bidirectional, 8);
        PathTrace(_scene.get(), library, tonemap);
    } else if (integrator == "mcmc") {
        if (_scene->options->mala) { // MALA builds only first-order derivatives
            std::shared_ptr<const PathFuncLib> library = 
                BuildPathFuncLibrary2(8, libpath);
            MLT(_scene.get(), library, tonemap);
        } else {    // Hessian otherwise 
            std::shared_ptr<const PathFuncLib> library =
                BuildPathFuncLibrary(_scene->options->bidirectional, 8);
            MLT(_scene.get(), library, tonemap);
        }
    } else {
        Error("Unknown integrator");
    }

}

nb::tuple PyScene::ray_intersect_all(
			tensorhw3 &rayorg,
			tensorhw3 &raydir){
	
	// std::cout << "ray_intersect_all started!" << std::endl;
	ProgressReporter reporter(rayorg.shape(1)*rayorg.shape(0));
	
	size_t shape[3] = { rayorg.shape(0), rayorg.shape(1), rayorg.shape(2) };
	
	float *normdata = new float[shape[0]*shape[1]*shape[2]];
	float *posdata = new float[shape[0]*shape[1]*shape[2]];
	
	// Delete 'data' when the 'owner' capsule expires
    nb::capsule normowner(normdata, [](void *p) noexcept {
       delete[] (float *) p;
    });
    // Delete 'data' when the 'owner' capsule expires
    nb::capsule posowner(posdata, [](void *p) noexcept {
       delete[] (float *) p;
    });

	nphw3 normals(new float[shape[0]*shape[1]*shape[2]], rayorg.ndim(), shape, normowner), 
		pos(new float[shape[0]*shape[1]*shape[2]], rayorg.ndim(), shape, posowner);

	// std::cout << "Going in par" << std::endl;
	ParallelFor(
	[&](const Vector2i tile) {

		const size_t x = tile[0];
		const size_t y = tile[1];

		Vector3 currrayorg, currraydir, currnorm, currpos;
		// // -----
		for (int i = 0; i < 3; ++i)
		{
			currrayorg(i) = rayorg(y,x,i);
			currraydir(i) = raydir(y,x,i);
		}
		
		ray_intersect1(
			currrayorg, currraydir,
			currnorm, currpos
		);
		
		for (int i = 0; i < 3; ++i)
		{
			normals(y,x,i) = currnorm(i);
			pos(y,x,i) = currpos(i);
		}

		reporter.Update(1);
	}, 
		Vector2i(rayorg.shape(1), rayorg.shape(0))
	);

	TerminateWorkerThreads();
	return nb::make_tuple(normals, pos);
}

bool PyScene::ray_intersect1(
			Vector3 &rayorg,
			Vector3 &raydir,	
			Vector3 &shNormal,
			Vector3 &pos 
			){

	// no copy change data ptr
	Ray _ray;
	for (size_t i = 0; i < 3; ++i)
	{
		_ray.org(i) = rayorg(i);
		_ray.dir(i) = raydir(i);
	}

	Vector3 _normal, _pos;

	ShapeInst shapeInst;
	RaySegment raySeg;
	raySeg.ray = _ray;
	auto tmpval = raySeg.ray.org.array().abs(); 
	float tmpval2 = std::max(tmpval(0), std::max(tmpval(1), tmpval(2)));
	raySeg.minT = (1.f + tmpval2)*RayEpsilon<float>;
	raySeg.maxT = std::numeric_limits<Float>::infinity();

	Intersection isect;

	bool hit = false;
	Float time = 0.f;
    if (Intersect(_scene.get(), time, raySeg, shapeInst)) {
    	// std::cout << "Hit something" << std::endl;
        if (shapeInst.obj->Intersect(shapeInst.primID, time, raySeg, isect, shapeInst.st)) {
            // copy data
			_normal = isect.shadingNormal;
			_pos = isect.position; 
			// std::cout << "n:" << _normal.transpose() 
			// 		<< ", pos:" << _pos.transpose() << std::endl;
            hit = true;
        }
    }

    for (size_t i = 0; i < 3; ++i)
	{
		shNormal(i) = _normal(i);
		pos(i) = _pos(i);
	}

  	return hit;

}

bool PyScene::ray_intersect(
			nbvec3 &rayorg,
			nbvec3 &raydir,	
			nbvec3 &shNormal,
			nbvec3 &pos 
			){

	// no copy change data ptr
	Ray _ray;
	for (size_t i = 0; i < 3; ++i)
	{
		_ray.org(i,0) = rayorg(i);
		_ray.dir(i,0) = raydir(i);
	}

	Vector3 _normal, _pos;

	ShapeInst shapeInst;
	RaySegment raySeg;
	raySeg.ray = _ray;
	auto tmpval = raySeg.ray.org.array().abs(); 
	float tmpval2 = std::max(tmpval(0), std::max(tmpval(1), tmpval(2)));
	raySeg.minT = (1.f + tmpval2)*RayEpsilon<float>;
	raySeg.maxT = std::numeric_limits<Float>::infinity();

	Intersection isect;

	bool hit = false;
	Float time = 0.f;
    if (Intersect(_scene.get(), time, raySeg, shapeInst)) {
    	// std::cout << "Hit something" << std::endl;
        if (shapeInst.obj->Intersect(shapeInst.primID, time, raySeg, isect, shapeInst.st)) {
            // copy data
			_normal = isect.shadingNormal;
			_pos = isect.position; 
			// std::cout << "n:" << _normal.transpose() 
			// 		<< ", pos:" << _pos.transpose() << std::endl;
            hit = true;
        }
    }

    for (size_t i = 0; i < 3; ++i)
	{
		shNormal(i) = _normal(i);
		pos(i) = _pos(i);
	}

  	return hit;

}

void PyScene::refract(
	nbvec3 &inrayorg, nbvec3 &inraydir, 
	nbvec3 &n, const Float eta, 
	nbvec3 &outrayorg, nbvec3 &outraydir){
	Ray _inray, _outray;
	for (size_t i = 0; i < 3; ++i)
	{
		_inray.org(i,0) = inrayorg(i);
		_inray.dir(i,0) = inraydir(i);
	}

	Vector3 _n;
	for (size_t i = 0; i < 3; ++i)
	{
		_n(i) = n(i);
	}

	_outray.org = _inray.org;
	// _outray.dir = Refract(_inray.dir, _n, Dot(_inray.dir, _n), eta, 1.f/eta);
	// https://physics.stackexchange.com/questions/435512/snells-law-in-vector-form
	// refraction in 3d presuming 
	// i: vector towards surface
	// n: from surface into the material
	// r: from surface into the material
	_inray.dir = Normalize(_inray.dir);
	_n = Normalize(_n);
	float cos_theta = Dot(_inray.dir, _n);
	float sin_theta2 = 1.f - cos_theta*cos_theta;
	float sin_thetaT2 = 1 - eta*eta*sin_theta2;
	// std::cout << "sin(T): " << sin_thetaT2 << std::endl;
	Vector3 term1 = std::sqrt(std::max(0.f, sin_thetaT2))*_n;
	_outray.dir = term1 + eta*(_inray.dir - cos_theta*_n);
	_outray.dir = Normalize(_outray.dir);

	// std::cout << "out dir: " << _outray.dir.transpose() << std::endl;


	for (size_t i = 0; i < 3; ++i)
	{
		outrayorg(i) = _outray.org(i,0);
		outraydir(i) = _outray.dir(i,0);
	}
}