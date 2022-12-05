#include <nanobind/nanobind.h>
#include <nanobind/tensor.h>
#include <Eigen/Dense>

#include "commondef.h"
#include "pythonscene.hpp"

using namespace nb::literals;

NB_MODULE(dpt_ext, m)
{

	// scene
	nb::class_<PyScene>(m, "PyScene")
			.def(
					nb::init<nb::str &, nb::str &, nb::dict &>(),
					"filename"_a, "outFn"_a, "subs"_a)
			.def("set_log_level", &PyScene::set_log_level, "loglevel"_a)
			.def_readonly("pixelHeight", &PyScene::pixelHeight)
			.def_readonly("pixelWidth", &PyScene::pixelWidth)
			.def("camera_trafo", &PyScene::camera_trafo)
			.def("camera_rays", &PyScene::camera_rays,
					 "rayorg"_a, "raydir"_a)
			.def("camera_rays2", &PyScene::camera_rays2,
					 "rayorg"_a, "raydir"_a, "rayminmax"_a)
			.def("ray_intersect", &PyScene::ray_intersect,
					 "rayorg"_a, "raydir"_a, "normal"_a, "pos"_a)
			.def("ray_intersect_all", &PyScene::ray_intersect_all,
					 "rayorg"_a, "raydir"_a)
			.def("ray_intersect_all2", &PyScene::ray_intersect_all2,
					 "rayorg"_a, "raydir"_a, "rayminmax"_a)
			.def("render", &PyScene::render, "outfn"_a, "tonemap"_a, "libpath"_a)
			.def("refract", &PyScene::refract,
					 "inrayorg"_a, "inraydir"_a,
					 "normal"_a, "eta"_a,
					 "outrayorg"_a, "outraydir"_a);
}