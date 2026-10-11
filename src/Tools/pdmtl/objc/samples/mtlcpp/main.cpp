// metal-cpp on PureDarwin: Apple's C++ Metal bindings over PD's ObjC Metal, drawing a spinning triangle with the
// AAPLRenderer shaders (vertexShader/fragmentShader) in a GLFW window (X11 or Wayland) through CAMetalLayer
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern "C" void *PDMetalLayerCreateForGLFWWindow(void *window);

// the AAPLShaderTypes.h layout: vertex buffer at index 0, viewport size at index 1
struct Vertex {
  float position[2];
  float pad[2];
  float color[4];
};

static double now() {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  const char *lib = argc > 1 ? argv[1] : "/usr/local/share/metal-pd/Triangle.app/Contents/Resources/default.metallib";
  const char *limit = getenv("PD_MTK_FRAMES");
  long frames = 0, max = limit ? atol(limit) : 0, n0 = 0;
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  NS::Error *err = nullptr;
  double t0 = now(), start = t0;

  MTL::Device *device = MTL::CreateSystemDefaultDevice();
  if (!device) {
    fprintf(stderr, "mtlcpp: no Metal device\n");
    return 1;
  }
  if (!glfwInit())
    return 1;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window = glfwCreateWindow(800, 600, "Metal (metal-cpp): triangle", nullptr, nullptr);
  CA::MetalLayer *layer = static_cast<CA::MetalLayer *>(PDMetalLayerCreateForGLFWWindow(window));
  if (!window || !layer)
    return 1;
  layer->setDevice(device);
  layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

  NS::URL *url = NS::URL::fileURLWithPath(NS::String::string(lib, NS::UTF8StringEncoding));
  MTL::Library *library = device->newLibrary(url, &err);
  if (!library) {
    fprintf(stderr, "mtlcpp: cannot load %s\n", lib);
    return 1;
  }
  MTL::Function *vfn = library->newFunction(NS::String::string("vertexShader", NS::UTF8StringEncoding));
  MTL::Function *ffn = library->newFunction(NS::String::string("fragmentShader", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *desc = MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vfn);
  desc->setFragmentFunction(ffn);
  desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(desc, &err);
  if (!pipeline) {
    fprintf(stderr, "mtlcpp: no pipeline state\n");
    return 1;
  }
  desc->release();
  MTL::CommandQueue *queue = device->newCommandQueue();

  while (!glfwWindowShouldClose(window) && (!max || frames < max)) {
    NS::AutoreleasePool *frame = NS::AutoreleasePool::alloc()->init();
    int w, h;

    glfwPollEvents();
    glfwGetFramebufferSize(window, &w, &h);
    if (w != (int)layer->drawableSize().width || h != (int)layer->drawableSize().height) {
      CGSize size;

      size.width = w, size.height = h;
      layer->setDrawableSize(size);
    }

    CA::MetalDrawable *drawable = layer->nextDrawable();
    if (!drawable) {
      frame->release();
      break;
    }
    float a = (float)(now() - start), r = 0.4f * (w < h ? w : h);
    Vertex v[3];
    for (int i = 0; i < 3; i++) {
      float t = a + i * 2.0943951f;

      v[i] = { { r * cosf(t), r * sinf(t) }, { 0, 0 }, { i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f, 1 } };
    }
    uint32_t viewport[2] = { (uint32_t)w, (uint32_t)h };

    MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    pass->colorAttachments()->object(0)->setTexture(drawable->texture());
    pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.1, 0.1, 0.15, 1));

    MTL::CommandBuffer *cmd = queue->commandBuffer();
    MTL::RenderCommandEncoder *enc = cmd->renderCommandEncoder(pass);
    enc->setViewport(MTL::Viewport{ 0, 0, (double)w, (double)h, 0, 1 });
    enc->setRenderPipelineState(pipeline);
    enc->setVertexBytes(v, sizeof(v), 0);
    enc->setVertexBytes(viewport, sizeof(viewport), 1);
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    enc->endEncoding();
    cmd->presentDrawable(drawable);
    cmd->commit();
    frame->release();

    frames++;
    if (getenv("PD_MTK_FPS") && now() - t0 >= 1) {
      printf("mtlcpp: %.1f fps\n", (frames - n0) / (now() - t0));
      fflush(stdout);
      t0 = now(), n0 = frames;
    }
  }
  pool->release();
  return 0;
}
