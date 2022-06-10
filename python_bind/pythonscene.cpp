#include <limits>
#include<unistd.h>
#include <filesystem>
#ifdef __APPLE__
    #include <limits.h>
#else
    #include <linux/limits.h>   // PATH_MAX
#endif
#include <libgen.h>

#include "pythonscene.hpp"

#include "camera.h"
#include "parallel.h"
#include "progressreporter.h"
#include "image.h"
#include "parsescene.h"
#include "scene.h"
#include "shape.h"
#include "utils.h"

// #define Epsilon std::numeric_limits<float>::epsilon() / 2;
// #define RayEpsilon Epsilon * 1500;
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

PyScene::PyScene(nb::str &filename_nb, 
             nb::str &outFn_nb,
            nb::dict &subsDict)
	
{
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
	// return to the original executable dir after parsing scene
	if (chdir(exeDir.c_str()) != 0) {
			Error("chdir failed");
	}

	const Camera *camera = _scene->camera.get();
	pixelHeight = GetPixelHeight(camera);
	pixelWidth = GetPixelWidth(camera);

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