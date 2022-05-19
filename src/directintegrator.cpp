#include "directintegrator.h"

void Direct(const Scene *scene){
  std::shared_ptr<const Camera> camera = scene->camera;
  std::shared_ptr<Image3> film = camera->film;
  film->Clear();
  const int pixelHeight = GetPixelHeight(camera.get());
  const int pixelWidth = GetPixelWidth(camera.get());
  SampleBuffer directBuffer(pixelWidth, pixelHeight);
  DirectLighting(scene, directBuffer);

  BufferToFilm(directBuffer, film.get());

  std::string outputNameHDR = scene->outputName + ".exr";
  WriteImage(outputNameHDR, GetFilm(scene->camera.get()).get());
}