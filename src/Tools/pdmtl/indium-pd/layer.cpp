// indium-pd: IndiumKit's CAMetalLayer stand-in over a pdmtl swapchain on the executor's device. With the
// Vulkan executor on, draws land straight in the swapchain image (zero-copy), otherwise frames are copied
#include "pd.hpp"

namespace IndiumKit {
	Drawable::~Drawable() {}
	Layer::~Layer() {}
};

namespace IndiumPD {
	Drawable::Drawable(std::shared_ptr<Device> device, struct pdmtl_drawable *drawable): _device(device), _drawable(drawable) {
		_texture = std::make_shared<Texture>(device, drawable);
	}

	void Drawable::present() {
		uint64_t t0 = profiling() ? nowUs() : 0;

		if (_presented)
			return;
		_presented = true;
		if (pdmtl_present(_drawable)) {
			static unsigned reported;
			if (reported++ < 8) fprintf(stderr, "indium-pd: frame presentation failed\n");
			return;
		}
		if (t0)
			profilePresent(nowUs() - t0);
	}

	class Layer: public IndiumKit::Layer {
	public:
		Layer(struct pdmtl_window *win, std::shared_ptr<Device> device): _win(win), _device(device) {}
		// a present still queued on the device thread uses the swapchain
		~Layer() override {
			_device->waitIdle();
			pdmtl_window_destroy(_win);
		}

		// one drawable in flight: the executor and the presenter share one queue and one lent image
		std::shared_ptr<IndiumKit::Drawable> nextDrawable() override {
			struct pdmtl_drawable *d;

			uint64_t t0 = profiling() ? nowUs() : 0, t1;

			_device->waitIdle();
			t1 = t0 ? nowUs() : 0;
			if (!(d = pdmtl_next_drawable(_win)))
				return nullptr;
			if (t0)
				profileAcquire(t1 - t0, nowUs() - t1);
			return std::make_shared<Drawable>(_device, d);
		}

		// the swapchain is BGRA8Unorm or BGRA8Unorm_sRGB, other formats keep BGRA8Unorm
		Indium::PixelFormat pixelFormat() override { return _srgb ? Indium::PixelFormat::BGRA8Unorm_sRGB : Indium::PixelFormat::BGRA8Unorm; }
		void setPixelFormat(Indium::PixelFormat format) override {
			_srgb = format == Indium::PixelFormat::BGRA8Unorm_sRGB;
			_device->waitIdle();
			pdmtl_window_set_srgb(_win, _srgb);
		}

	private:
		struct pdmtl_window *_win;
		std::shared_ptr<Device> _device;
		bool _srgb = false;
	};
};

std::shared_ptr<IndiumKit::Layer> IndiumKit::Layer::make(VkSurfaceKHR surface, std::shared_ptr<Indium::Device> device, size_t width, size_t height) {
	auto d = std::dynamic_pointer_cast<IndiumPD::Device>(device);
	struct pdmtl_window *win;

	if (!d || !(win = pdmtl_surface_attach((uint64_t)surface, (int)width, (int)height)))
		return nullptr;
	return std::make_shared<IndiumPD::Layer>(win, d);
}

std::shared_ptr<IndiumKit::Layer> PDMakeFrameLayer(std::shared_ptr<Indium::Device> device, void *context,
    int (*present)(void *, const void *, int, int, int), void *(*acquire)(void *, int, int, int *),
    int (*commit)(void *, void *, int, int), void (*cancel)(void *, void *), size_t width, size_t height) {
    auto d=std::dynamic_pointer_cast<IndiumPD::Device>(device);
    auto win=d?(acquire && commit && cancel
      ?pdmtl_frame_attach_buffers(context,present,acquire,commit,cancel,(int)width,(int)height)
      :pdmtl_frame_attach(context,present,(int)width,(int)height)):nullptr;
    return win?std::make_shared<IndiumPD::Layer>(win,d):nullptr;
}
std::shared_ptr<IndiumKit::Layer> PDMakeGLFWLayer(std::shared_ptr<Indium::Device> device, void *window) {
    auto d=std::dynamic_pointer_cast<IndiumPD::Device>(device);
    auto win=d?pdmtl_glfw_attach(window):nullptr;
    return win?std::make_shared<IndiumPD::Layer>(win,d):nullptr;
}
