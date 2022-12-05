#pragma once

#include <tuple>
#include <nanobind/nanobind.h>
#include <nanobind/tensor.h>
#include <Eigen/Dense>

#include "commondef.h"
#include "image.h"
#include "scene.h"

namespace nb = nanobind;

typedef nb::tensor<nb::numpy, float, nb::shape<4, 4>, nb::device::cpu> trafo;
typedef nb::tensor<float, nb::shape<6>, nb::device::cpu> nbvec6;
typedef nb::tensor<float, nb::shape<3>, nb::device::cpu> nbvec3;
typedef nb::tensor<float, nb::shape<nb::any, nb::any, 3>, nb::device::cpu> tensorhw3;

typedef nb::tensor<nb::numpy, float, nb::shape<nb::any, nb::any, 3>, nb::device::cpu> nphw3;

class PyScene
{
public:
	PyScene(nb::str &filename,
					nb::str &outFn,
					nb::dict &subsDict);

	void set_log_level(const std::string &loglevel);

	trafo camera_trafo();

	void camera_rays(tensorhw3 &rayorg, tensorhw3 &raydir);
	void camera_rays2(tensorhw3 &rayorg, tensorhw3 &raydir, tensorhw3 &rayminmax);

	bool ray_intersect(
			nbvec3 &rayorg,
			nbvec3 &raydir,
			nbvec3 &normal,
			nbvec3 &pos);

	bool ray_intersect1(
			Vector3 &rayorg,
			Vector3 &raydir,
			Vector3 &shNormal,
			Vector3 &pos);

	bool ray_intersect3(
			Vector3 &rayorg,
			Vector3 &raydir,
			Vector3 &rayminmax,
			Vector3 &shNormal,
			Vector3 &pos);

	nb::tuple
	ray_intersect_all(
			tensorhw3 &rayorg,
			tensorhw3 &raydir);

	nb::tuple
	ray_intersect_all2(
			tensorhw3 &rayorg,
			tensorhw3 &raydir,
			tensorhw3 &rayminmax);

	void render(nb::str &_outfn, const bool tonemap, nb::str &_libpath);

	void refract(
			nbvec3 &inrayorg,
			nbvec3 &inraydir,
			nbvec3 &n,
			Float eta,
			nbvec3 &outrayorg,
			nbvec3 &outraydir);

	int pixelHeight, pixelWidth;

private:
	std::unique_ptr<Scene> _scene;
};
