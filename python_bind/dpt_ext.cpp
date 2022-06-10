#include <nanobind/nanobind.h>
#include <nanobind/tensor.h>
#include <Eigen/Dense>

#include "commondef.h"
#include "pythonscene.hpp"

using namespace nb::literals;

NB_MODULE(dpt_ext, m) {
	
		// scene
		nb::class_<PyScene>(m, "PyScene")
			.def(
				nb::init<nb::str&,  nb::str&, nb::dict&>(),
				"filename"_a, "outFn"_a, "subs"_a
				)
			.def_readonly("pixelHeight", &PyScene::pixelHeight)
			.def_readonly("pixelWidth", &PyScene::pixelWidth)
			.def("camera_rays", &PyScene::camera_rays,
				"rayorg"_a, "raydir"_a
				)
			.def("ray_intersect", &PyScene::ray_intersect, 
				"rayorg"_a, "raydir"_a, "normal"_a, "pos"_a
				)
			.def("refract", &PyScene::refract,
				"inrayorg"_a, "inraydir"_a,
				 "normal"_a, "eta"_a, 
				 "outrayorg"_a, "outraydir"_a
				);
}