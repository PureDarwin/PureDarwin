// indium-pd: Indium's abstract Metal-like C++ interface (darlinghq/indium include/indium, 0BSD) over
// airjitd's executor through pdmtl. Resources live in pdmtl's arena, encoders record PD requests
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <indium/indium.hpp>
#include <indium/pd-sync.hpp>
#include <indium-kit/indium-kit.hpp>

extern "C" {
#include "bridge.h"
#include "pdmtl.h"
#include "pdmtl_window.h"
}

namespace IndiumPD {
	// first-fit ranges of one big arena allocation, so buffers and textures can be freed
	class Heap {
	public:
		void init(size_t size);
		uint32_t alloc(size_t size);
		void free(uint32_t off);
		// per-frame bytes (setBytes, rewritten indices): a ring that wraps, only reused once idle
		uint32_t scratch(const void *bytes, size_t size);

	private:
		std::mutex _mu;
		uint32_t _base = 0, _size = 0, _ring = 0, _ringSize = 0, _ringHead = 0;
		std::map<uint32_t, uint32_t> _free, _used;
	};

	extern Heap heap;

	// airjitd's numbering for a Metal pixel format, -1 when it has none, and bytes per pixel
	int pdFormat(Indium::PixelFormat format);
	size_t formatBytes(Indium::PixelFormat format);
	size_t formatBlock(Indium::PixelFormat format);
	size_t formatRowBytes(Indium::PixelFormat format, size_t width);
	size_t formatRows(Indium::PixelFormat format, size_t height);

	class Device;

	class Buffer: public Indium::Buffer {
	public:
		Buffer(std::shared_ptr<Device> device, size_t length);
		// a placed buffer: length bytes at arena offset off, inside memory that backing keeps
		Buffer(std::shared_ptr<Device> device, size_t length, uint32_t off, std::shared_ptr<void> backing);
		~Buffer() override;
		std::shared_ptr<Indium::Device> device() override;
		size_t length() const override { return _length; }
		void *contents() override;
		void didModifyRange(Indium::Range<size_t>) override {}
		uint64_t gpuAddress() override { return _off; }
		uint32_t offset() const { return _off; }

	private:
		std::weak_ptr<Device> _device;
		size_t _length;
		uint32_t _off;
		bool _owned = true;
		std::shared_ptr<void> _backing;
	};

	class Texture: public Indium::Texture, public std::enable_shared_from_this<Texture> {
	public:
		Texture(std::shared_ptr<Device> device, const Indium::TextureDescriptor &desc);
		// a placed texture at arena offset off, inside memory that backing keeps (a heap or a buffer), one over a
		// buffer the CPU writes is uploaded again each time it is used
		Texture(std::shared_ptr<Device> device, const Indium::TextureDescriptor &desc, uint32_t off, std::shared_ptr<void> backing,
		        bool cpuWritten);
		// the arena bytes a texture of desc takes
		static size_t storageSize(const Indium::TextureDescriptor &desc);
		// a pdmtl drawable's texture: the arena range the CPU path draws into
		Texture(std::shared_ptr<Device> device, struct pdmtl_drawable *drawable);
		// a view: the parent's bytes as another format of the same size, type, or range of levels and slices
		Texture(std::shared_ptr<Texture> parent, Indium::PixelFormat format, Indium::TextureType type, size_t level0,
		        size_t levelCount, size_t slice0, size_t sliceCount, const Indium::TextureSwizzleChannels &swizzle);
		~Texture() override;
		std::shared_ptr<Indium::Device> device() override;
		Indium::TextureType textureType() const override { return _desc.textureType; }
		Indium::PixelFormat pixelFormat() const override { return _desc.pixelFormat; }
		size_t width() const override { return _desc.width; }
		size_t height() const override { return _desc.height; }
		size_t depth() const override { return _desc.depth; }
		size_t mipmapLevelCount() const override { return _desc.mipmapLevelCount; }
		size_t arrayLength() const override { return _desc.arrayLength; }
		size_t sampleCount() const override { return _desc.sampleCount; }
		bool framebufferOnly() const override { return false; }
		bool allowGPUOptimizedContents() const override { return true; }
		bool shareable() const override { return false; }
		Indium::TextureSwizzleChannels swizzle() const override { return _desc.swizzle; }
		std::shared_ptr<Indium::Texture> newTextureView(Indium::PixelFormat pixelFormat) override;
		std::shared_ptr<Indium::Texture> newTextureView(Indium::PixelFormat pixelFormat, Indium::TextureType textureType,
		                                                const Indium::Range<size_t> &levels, const Indium::Range<size_t> &layers) override;
		std::shared_ptr<Indium::Texture> newTextureView(Indium::PixelFormat pixelFormat, Indium::TextureType textureType,
		                                                const Indium::Range<size_t> &levels, const Indium::Range<size_t> &layers,
		                                                const Indium::TextureSwizzleChannels &swizzle) override;
		void replaceRegion(Indium::Region region, size_t mipmapLevel, const void *bytes, size_t bytesPerRow) override;
		void replaceRegion(Indium::Region region, size_t mipmapLevel, size_t slice, const void *bytes, size_t bytesPerRow,
		                   size_t bytesPerImage) override;

		// how a request names it, false when the executor cannot use it (multisample, 1D, buffer textures)
		bool describe(struct PDDrawTex *t) const;
		// copies an IOSurface's newer contents in, before the CPU or GPU reads the texture
		void pullSurface() const;
		// its levels and slices (bridge.h): tight rows, levels 16 bytes apart, slices 256
		void layout(struct PDTexLayout *l) const;
		uint8_t *bytes() const;
		size_t bytesPerRow() const { return _bpr; }
		// level k of slice s (a 3D texture's z), and its row pitch
		uint8_t *levelBytes(size_t level, size_t slice) const;
		size_t levelBytesPerRow(size_t level) const;
		size_t levels() const { return _levels; }
		// every byte it holds in the arena
		uint32_t offset() const { return _off; }
		uint32_t totalBytes() const { return (uint32_t)_total; }
		struct pdmtl_drawable *drawable() const { return _drawable; }
		// an IOSurface the texture mirrors, null for ordinary textures
		std::shared_ptr<SurfaceLink> surfaceLink() const { return _link; }
		void setSurfaceLink(std::shared_ptr<SurfaceLink> link) { _link = std::move(link); }

	private:
		std::shared_ptr<SurfaceLink> _link;
		std::weak_ptr<Device> _device;
		Indium::TextureDescriptor _desc;
		uint32_t _off = 0;
		size_t _bpr = 0;
		bool _owned = true;
		struct pdmtl_drawable *_drawable = nullptr;
		std::shared_ptr<Texture> _parent; // a view keeps the bytes it shows alive
		std::shared_ptr<void> _backing;
		bool _cpuWritten = false;
		size_t plan(const Indium::TextureDescriptor &desc);
		size_t _levels = 1, _slices = 1, _sliceBytes = 0, _total = 0;
		uint32_t _levelOff[PD_TEX_LEVELS] = {};
	};

	class SamplerState: public Indium::SamplerState {
	public:
		SamplerState(std::shared_ptr<Device> device, const Indium::SamplerDescriptor &desc);
		std::shared_ptr<Indium::Device> device() override;
		// the kext's sampler flags word (airjitd texture.c, bridge.h)
		uint32_t flags() const { return _flags; }

	private:
		std::weak_ptr<Device> _device;
		uint32_t _flags;
	};

	class DepthStencilState: public Indium::DepthStencilState {
	public:
		DepthStencilState(std::shared_ptr<Device> device, const Indium::DepthStencilDescriptor &desc): _device(device), _desc(desc) {}
		std::shared_ptr<Indium::Device> device() override;
		// the PDDrawExt depthState and stencil words (bridge.h)
		uint32_t depthWord() const;
		uint32_t stencilWord(bool back) const;

	private:
		std::weak_ptr<Device> _device;
		Indium::DepthStencilDescriptor _desc;
	};

	class Function: public Indium::Function {
	public:
		Function(std::shared_ptr<Device> device, struct pdmtl_fn *fn, const std::string &name);
		std::shared_ptr<Indium::Device> device() override;
		struct pdmtl_fn *fn() const { return _fn; }

	private:
		std::weak_ptr<Device> _device;
		struct pdmtl_fn *_fn;
	};

	class Library: public Indium::Library {
	public:
		Library(std::shared_ptr<Device> device, struct pdmtl_lib *lib): _device(device), _lib(lib) {}
		std::shared_ptr<Indium::Function> newFunction(const std::string &name) override;
		std::shared_ptr<Indium::Device> device() override;
		struct pdmtl_lib *lib() const { return _lib; }

	private:
		std::weak_ptr<Device> _device;
		struct pdmtl_lib *_lib;
	};

	class RenderPipelineState: public Indium::RenderPipelineState {
	public:
		RenderPipelineState(std::shared_ptr<Device> device, const Indium::RenderPipelineDescriptor &desc);
		std::shared_ptr<Indium::Device> device() override;
		const Indium::RenderPipelineDescriptor &descriptor() const { return _desc; }
		// the kext's packed blend state for colour attachment i
		uint32_t blend(size_t i) const;

	private:
		std::weak_ptr<Device> _device;
		Indium::RenderPipelineDescriptor _desc;
	};

	class ComputePipelineState: public Indium::ComputePipelineState {
	public:
		ComputePipelineState(std::shared_ptr<Device> device, std::shared_ptr<Function> function);
		std::shared_ptr<Indium::Device> device() override;
		size_t imageblockMemoryLength(Indium::Size) override { return 0; }
		std::shared_ptr<Indium::FunctionHandle> functionHandle(std::shared_ptr<Indium::Function>) override { return nullptr; }
		std::shared_ptr<Indium::ComputePipelineState> newComputePipelineState(const std::vector<std::shared_ptr<Indium::Function>> &) override { return nullptr; }
		std::shared_ptr<Indium::VisibleFunctionTable> newVisibleFunctionTable(const Indium::VisibleFunctionTableDescriptor &) override { return nullptr; }
		std::shared_ptr<Indium::IntersectionFunctionTable> newIntersectionFunctionTable(const Indium::IntersectionFunctionTableDescriptor &) override { return nullptr; }
		std::shared_ptr<Function> function() const { return _function; }

	private:
		std::weak_ptr<Device> _device;
		std::shared_ptr<Function> _function;
	};

	class CommandBuffer;

	// one executor thread per device: committed command buffers run there in order
	class Device: public Indium::Device, public std::enable_shared_from_this<Device> {
	public:
		Device();
		~Device() override;
		std::string name() const override { return "PureDarwin airjitd"; }
		std::shared_ptr<Indium::CommandQueue> newCommandQueue() override;
		std::shared_ptr<Indium::RenderPipelineState> newRenderPipelineState(const Indium::RenderPipelineDescriptor &descriptor) override;
		std::shared_ptr<Indium::ComputePipelineState> newComputePipelineState(const Indium::ComputePipelineDescriptor &descriptor, Indium::PipelineOption options,
		                                                                      std::shared_ptr<Indium::ComputePipelineReflection> reflection) override;
		std::shared_ptr<Indium::ComputePipelineState> newComputePipelineState(std::shared_ptr<Indium::Function> computeFunction, Indium::PipelineOption options,
		                                                                      std::shared_ptr<Indium::ComputePipelineReflection> reflection) override;
		std::shared_ptr<Indium::Buffer> newBuffer(size_t length, Indium::ResourceOptions options) override;
		std::shared_ptr<Indium::Buffer> newBuffer(const void *pointer, size_t length, Indium::ResourceOptions options) override;
		std::shared_ptr<Indium::Library> newLibrary(const void *data, size_t length) override;
		std::shared_ptr<Indium::Texture> newTexture(const Indium::TextureDescriptor &descriptor) override;
		std::shared_ptr<Indium::SamplerState> newSamplerState(const Indium::SamplerDescriptor &descriptor) override;
		std::shared_ptr<Indium::DepthStencilState> newDepthStencilState(const Indium::DepthStencilDescriptor &descriptor) override;
		void pollEvents(uint64_t timeoutNanoseconds) override;
		void wakeupEventLoop() override;

		void submit(std::shared_ptr<CommandBuffer> cb);
		// waits for every committed command buffer
		void waitIdle();

	private:
		void run();

		std::mutex _mu;
		std::condition_variable _cv, _idle, _poll;
		std::deque<std::shared_ptr<CommandBuffer>> _queue;
		bool _busy = false, _stop = false, _woken = false;
		std::thread _worker;
	};

	class CommandQueue: public Indium::CommandQueue, public std::enable_shared_from_this<CommandQueue> {
	public:
		CommandQueue(std::shared_ptr<Device> device): _device(device) {}
		std::shared_ptr<Indium::CommandBuffer> commandBuffer() override;
		std::shared_ptr<Indium::Device> device() override { return _device.lock(); }

	private:
		std::weak_ptr<Device> _device;
	};

	// work recorded by encoders, run on the device's thread
	using Job = std::function<void()>;

	class Drawable;

	class CommandBuffer: public Indium::CommandBuffer, public std::enable_shared_from_this<CommandBuffer> {
	public:
		CommandBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandQueue> queue): _device(device), _queue(queue) {}
		std::shared_ptr<Indium::RenderCommandEncoder> renderCommandEncoder(const Indium::RenderPassDescriptor &descriptor) override;
		std::shared_ptr<Indium::BlitCommandEncoder> blitCommandEncoder() override;
		std::shared_ptr<Indium::BlitCommandEncoder> blitCommandEncoder(const Indium::BlitPassDescriptor &descriptor) override;
		std::shared_ptr<Indium::ComputeCommandEncoder> computeCommandEncoder() override;
		std::shared_ptr<Indium::ComputeCommandEncoder> computeCommandEncoder(const Indium::ComputePassDescriptor &descriptor) override;
		std::shared_ptr<Indium::ComputeCommandEncoder> computeCommandEncoder(Indium::DispatchType dispatchType) override;
		void commit() override;
		void presentDrawable(std::shared_ptr<Indium::Drawable> drawable) override;
		void addScheduledHandler(Handler handler) override;
		void addCompletedHandler(Handler handler) override;
		void waitUntilCompleted() override;
		std::shared_ptr<Indium::CommandQueue> commandQueue() override { return _queue.lock(); }
		std::shared_ptr<Indium::Device> device() override { return _device.lock(); }

		void add(Job job) { _jobs.push_back(std::move(job)); }
		// a surface-backed texture this command buffer renders into, copied back to its surface once it ran
		void noteSurfaceWrite(std::shared_ptr<Texture> texture);
		// on the device's thread
		void execute();

	private:
		std::vector<std::shared_ptr<Texture>> _surfaceWrites;
		std::weak_ptr<Device> _device;
		std::weak_ptr<CommandQueue> _queue;
		std::vector<Job> _jobs;
		std::vector<std::shared_ptr<Indium::Drawable>> _present;
		std::vector<Handler> _scheduled, _completed;
		std::mutex _mu;
		std::condition_variable _cv;
		bool _done = false;
	};

	// INDIUM_PD_PROFILE=1: once a second, frames and where their time went (jobs by part, waits, presents)
	bool profiling();
	void profileJob(uint64_t us, bool vulkan);
	void profileAcquire(uint64_t idleUs, uint64_t acquireUs);
	void profilePresent(uint64_t us);
	uint64_t nowUs();

	// pdmtl's request plumbing with a job's failure reported once
	void runDraw(struct PDDrawRequest &r);
	void runCompute(struct PDComputeRequest &r);

	class Drawable: public IndiumKit::Drawable {
	public:
		Drawable(std::shared_ptr<Device> device, struct pdmtl_drawable *drawable);
		std::shared_ptr<Indium::Texture> texture() override { return _texture; }
		// on the device's thread when a command buffer presents it, else right away
		void present() override;
		bool presented() const { return _presented; }

	private:
		std::shared_ptr<Device> _device;
		std::shared_ptr<Texture> _texture;
		struct pdmtl_drawable *_drawable;
		bool _presented = false;
	};
};
