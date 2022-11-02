#pragma once

#include "commondef.h"
#include "utils.h"
#include "parallel.h"

#include <vector>
#include <array>
#include <memory>

struct Image3 {
    Image3() {
    }
    Image3(const std::string &filename);
    Image3(int w, int h) : pixelWidth(w), pixelHeight(h) {
        data.resize(w * h);
        fill(data.begin(), data.end(), Vector3::Zero());
    }

    Vector3 &At(int x) {
        return data[x];
    }
    const Vector3 &At(int x) const {
        return data[x];
    }
    Vector3 &At(int x, int y) {
        return data[y * pixelWidth + x];
    }
    const Vector3 &At(int x, int y) const {
        return data[y * pixelWidth + x];
    }
    Vector3 &RepAt(int x, int y) {
        return data[Modulo(y, pixelHeight) * pixelWidth + Modulo(x, pixelWidth)];
    }
    const Vector3 &RepAt(int x, int y) const {
        return data[Modulo(y, pixelHeight) * pixelWidth + Modulo(x, pixelWidth)];
    }
    int NumPixels() const {
        return pixelWidth * pixelHeight;
    }
    void Clear() {
        fill(data.begin(), data.end(), Vector3::Zero());
    }

    int pixelWidth;
    int pixelHeight;
    std::vector<Vector3, Eigen::aligned_allocator<Vector3>> data;
};

void WriteImage(const std::string &filename, const Image3 *image);

using Pixel = std::array<AtomicFloat, 3>;

struct SampleBuffer {
    SampleBuffer(const int pixelWidth, const int pixelHeight)
        : pixelWidth(pixelWidth), pixelHeight(pixelHeight), 
        cropOffsetX(0), cropOffsetY(0), cropWidth(pixelWidth), cropHeight(pixelHeight) {
        pixels = std::unique_ptr<Pixel[]>(new Pixel[pixelWidth * pixelHeight]);
    }

    SampleBuffer(const int _pixelWidth, const int _pixelHeight,
        const int _cropOffsetX, const int _cropOffsetY,
        const int _cropWidth, const int _cropHeight)
        : pixelWidth(_pixelWidth), pixelHeight(_pixelHeight),
        cropOffsetX(_cropOffsetX), cropOffsetY(_cropOffsetY),
        cropWidth(_cropWidth), cropHeight(_cropHeight) {

        pixels = std::unique_ptr<Pixel[]>(new Pixel[cropWidth * cropHeight]);
    }

    std::unique_ptr<Pixel[]> pixels;

    const int pixelWidth;
    const int pixelHeight;
    const int cropOffsetX;
    const int cropOffsetY;
    const int cropWidth;
    const int cropHeight;
};

inline void Splat(SampleBuffer &buffer, const Vector2 screenPos, const Vector3 &contrib) {
    int ix = Clamp(int(screenPos[0] * buffer.pixelWidth) - buffer.cropOffsetX, 0, buffer.cropWidth - 1);
    int iy = Clamp(int(screenPos[1] * buffer.pixelHeight) - buffer.cropOffsetY, 0, buffer.cropHeight - 1);

    Pixel &pixel = buffer.pixels[iy * buffer.cropWidth + ix];

    if (contrib.allFinite()) {
        for (int i = 0; i < int(pixel.size()); i++) {
            pixel[i].Add(contrib[i]);
        }
    }
}

inline void MergeBuffer(const SampleBuffer &buffer1,
                        const Float b1Weight,
                        const SampleBuffer &buffer2,
                        const Float b2Weight,
                        SampleBuffer &bufferOut) {
    assert(buffer1.pixelWidth == buffer2.pixelWidth);
    assert(buffer1.pixelHeight == buffer2.pixelHeight);
    assert(buffer2.pixelWidth == bufferOut.pixelWidth);
    assert(buffer2.pixelHeight == bufferOut.pixelHeight);
    // TODO: add checks for crop params
    for (int i = 0; i < bufferOut.cropWidth * bufferOut.cropHeight; i++) {
        const Pixel &pixel1 = buffer1.pixels[i];
        const Pixel &pixel2 = buffer2.pixels[i];
        Pixel &pixelOut = bufferOut.pixels[i];
        for (int j = 0; j < int(pixelOut.size()); j++) {
            pixelOut[j].Add(b1Weight * pixel1[j]);
            pixelOut[j].Add(b2Weight * pixel2[j]);
        }
    }
}

inline void BufferToFilm(const SampleBuffer &buffer,
                         Image3 *film,
                         const Float factor = Float(1.0)) {
    for (int i = 0; i < buffer.cropWidth * buffer.cropHeight; i++) {
        const Pixel &pixel = buffer.pixels[i];
        Vector3 color = factor * Vector3(Float(pixel[0]), Float(pixel[1]), Float(pixel[2]));
        film->At(i) = color;
    }
}
