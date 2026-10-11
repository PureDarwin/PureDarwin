#pragma once
#include <indium-kit/indium-kit.hpp>
std::shared_ptr<IndiumKit::Layer> PDMakeFrameLayer(std::shared_ptr<Indium::Device> device, void *context,
    int (*present)(void *, const void *, int, int, int), void *(*acquire)(void *, int, int, int *),
    int (*commit)(void *, void *, int, int), void (*cancel)(void *, void *), size_t width, size_t height);
std::shared_ptr<IndiumKit::Layer> PDMakeGLFWLayer(std::shared_ptr<Indium::Device> device, void *window);
