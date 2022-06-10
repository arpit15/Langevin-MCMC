#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/tensor.h>
#include <Eigen/Dense>

#include "commondef.h"
#include "image.h"
#include "scene.h"

namespace nb = nanobind;

typedef nb::tensor<float, nb::shape<6>, nb::device::cpu> nbvec6;
typedef nb::tensor<float, nb::shape<3>, nb::device::cpu> nbvec3;
typedef nb::tensor<float, nb::shape<nb::any, nb::any, 3>, nb::device::cpu> tensorhw3;

class PyScene
{
public:
	PyScene(nb::str &filename, 
           nb::str &outFn,
            nb::dict &subsDict);
	
	void camera_rays(tensorhw3 &rayorg, tensorhw3 &raydir);

	bool ray_intersect(
			nbvec3 &rayorg,
			nbvec3 &raydir,
			nbvec3 &normal,
			nbvec3 &pos
			);

	void refract(
		nbvec3 &inrayorg, 
		nbvec3 &inraydir, 
		nbvec3 &n, 
		Float eta, 
		nbvec3 &outrayorg,
		nbvec3 &outraydir
		);

	int pixelHeight, pixelWidth;
private:
	std::unique_ptr<Scene> _scene;
};

