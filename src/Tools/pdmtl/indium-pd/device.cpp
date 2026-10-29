// indium-pd: the device and its resources. Indium's interface headers are 0BSD (darlinghq/indium),
// this implementation is ours
#include "pd.hpp"

#include <indium/pd-sync.hpp>

#include <algorithm>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <indium/indium.private.hpp>

extern "C" {
#include "vulkan/vk.h"
}

namespace Indium {
	VkInstance globalInstance = VK_NULL_HANDLE;
	PFN_vkGetInstanceProcAddr globalGetInstanceProcAddr = nullptr;

	// indium's interface classes declare pure virtual destructors: they still need bodies
	Device::~Device() {}
	Function::~Function() {}
	Library::~Library() {}
	Resource::~Resource() {}
	Buffer::~Buffer() {}
	Texture::~Texture() {}
	SamplerState::~SamplerState() {}
	DepthStencilState::~DepthStencilState() {}
	RenderPipelineState::~RenderPipelineState() {}
	ComputePipelineState::~ComputePipelineState() {}
	CommandQueue::~CommandQueue() {}
	CommandBuffer::~CommandBuffer() {}
	CommandEncoder::~CommandEncoder() {}
	RenderCommandEncoder::~RenderCommandEncoder() {}
	ComputeCommandEncoder::~ComputeCommandEncoder() {}
	BlitCommandEncoder::~BlitCommandEncoder() {}
	Drawable::~Drawable() {}
	CounterSampleBuffer::~CounterSampleBuffer() {}

	std::shared_ptr<Texture> Texture::parentTexture() const { return nullptr; }
	size_t Texture::parentRelativeLevel() const { return 0; }
	size_t Texture::parentRelativeSlice() const { return 0; }
	std::shared_ptr<Buffer> Texture::buffer() const { return nullptr; }
	size_t Texture::bufferOffset() const { return 0; }
	size_t Texture::bufferBytesPerRow() const { return 0; }

	static const size_t arenaBytes = (size_t)3 << 30;

	// Metal loads into every process that links it (Wine's Mac driver, to list GPUs): the arena, the Vulkan executor
	// and its fault handling start with the first resource or queue, as a Mac device costs nothing until used
	static const char **wantExtensions;
	static size_t wantCount;
	static std::once_flag started;

	static void startExecutor() {
		const char **extensions = wantExtensions;
		size_t count = wantCount;
		static const char *const swapchain[] = { "VK_KHR_swapchain" };
		struct pd_vk_handles h;

		if (pdmtl_init(arenaBytes)) {
			fprintf(stderr, "indium-pd: no memory for the arena\n");
			abort();
		}
		IndiumPD::heap.init(arenaBytes - ((size_t)64 << 20));

		// a window system's extensions mean presenting: the executor's device is the presenter's
		if (count && !(getenv("AIRJITD_OPENGL") && strcmp(getenv("AIRJITD_OPENGL"),"0"))) {
			pd_vk_want_extensions(extensions, (unsigned)count, swapchain, 1);
			if (!pd_vk_start(&h)) {
				globalInstance = (VkInstance)h.instance;
				globalGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)h.gipa;
			}
			// resources stay on the executor's device between jobs (PDMTL_RESIDENT=0: the per-job path)
			if (!getenv("PDMTL_RESIDENT") || strcmp(getenv("PDMTL_RESIDENT"), "0"))
				pdmtl_resident(1);
			else
				fprintf(stderr, "indium-pd: no Vulkan to present with: %s\n", pd_vk_reason());
		}
        if (getenv("AIRJITD_OPENGL") && strcmp(getenv("AIRJITD_OPENGL"),"0") &&
            (!getenv("PDMTL_RESIDENT") || strcmp(getenv("PDMTL_RESIDENT"),"0"))) pdmtl_resident(1);
	}

	void init(const char **extensions, size_t count, bool) {
		wantExtensions = extensions;
		wantCount = count;
	}

	void ensureStarted() {
		std::call_once(started, startExecutor);
	}

	void finit() {}

	std::shared_ptr<Device> createSystemDefaultDevice() {
		static std::weak_ptr<IndiumPD::Device> one;
		auto d = one.lock();

		if (!d)
			one = d = std::make_shared<IndiumPD::Device>();
		return d;
	}
};

namespace IndiumPD {
	Heap heap;

	void Heap::init(size_t size) {
		_ringSize = 32u << 20;
		_size = (uint32_t)(size - _ringSize);
		_base = pdmtl_alloc(size);
		_ring = _base + _size;
		_free[_base] = _size;
	}

	uint32_t Heap::alloc(size_t size) {
		std::lock_guard<std::mutex> lock(_mu);
		uint32_t want = (uint32_t)((size + 255) & ~(size_t)255);

		if (!want)
			want = 256;
		for (auto it = _free.begin(); it != _free.end(); ++it) {
			if (it->second < want)
				continue;
			uint32_t off = it->first, left = it->second - want;
			_free.erase(it);
			if (left)
				_free[off + want] = left;
			_used[off] = want;
			return off;
		}
		fprintf(stderr, "indium-pd: arena full (%zu bytes wanted)\n", size);
		abort();
	}

	void Heap::free(uint32_t off) {
		std::lock_guard<std::mutex> lock(_mu);
		auto u = _used.find(off);

		if (u == _used.end())
			return;
		uint32_t len = u->second;
		_used.erase(u);
		auto next = _free.find(off + len);
		if (next != _free.end()) {
			len += next->second;
			_free.erase(next);
		}
		auto it = _free.lower_bound(off);
		if (it != _free.begin()) {
			--it;
			if (it->first + it->second == off) {
				it->second += len;
				return;
			}
		}
		_free[off] = len;
	}

	uint32_t Heap::scratch(const void *bytes, size_t size) {
		std::lock_guard<std::mutex> lock(_mu);
		uint32_t want = (uint32_t)((size + 255) & ~(size_t)255), off;

		if (want > _ringSize) {
			fprintf(stderr, "indium-pd: %zu bytes of setBytes data\n", size);
			abort();
		}
		if (_ringHead + want > _ringSize)
			_ringHead = 0;
		off = _ring + _ringHead;
		_ringHead += want;
		if (bytes)
			memcpy(pdmtl_arena() + off, bytes, size);
		return off;
	}

	// metal pixel formats airjitd knows (texture.h), with bytes per pixel
	static const struct {
		Indium::PixelFormat metal;
		int pd;
		size_t bpp;
	} formats[] = {
		{ Indium::PixelFormat::BGRA8Unorm, 0, 4 },       { Indium::PixelFormat::RGBA16Float, 1, 8 },
		{ Indium::PixelFormat::A8Unorm, 2, 1 },          { Indium::PixelFormat::R8Unorm, 3, 1 },
		{ Indium::PixelFormat::RGBA8Unorm, 4, 4 },       { Indium::PixelFormat::RG8Unorm, 5, 2 },
		{ Indium::PixelFormat::RG16Uint, 6, 4 },         { Indium::PixelFormat::R32Uint, 7, 4 },
		{ Indium::PixelFormat::RG32Uint, 8, 8 },         { Indium::PixelFormat::RGBA16Uint, 9, 8 },
		{ Indium::PixelFormat::R16Uint, 10, 2 },         { Indium::PixelFormat::RGBA8Uint, 11, 4 },
		{ Indium::PixelFormat::R16Float, 12, 2 },        { Indium::PixelFormat::RG16Float, 13, 4 },
		{ Indium::PixelFormat::R32Float, 14, 4 },        { Indium::PixelFormat::RG32Float, 15, 8 },
		{ Indium::PixelFormat::RGBA32Float, 16, 16 },    { Indium::PixelFormat::RGBA8Unorm_sRGB, 17, 4 },
		{ Indium::PixelFormat::BGRA8Unorm_sRGB, 18, 4 }, { Indium::PixelFormat::RGB10A2Unorm, 19, 4 },
		{ Indium::PixelFormat::BGR10A2Unorm, 20, 4 },    { Indium::PixelFormat::R16Unorm, 21, 2 },
		{ Indium::PixelFormat::RG16Unorm, 22, 4 },       { Indium::PixelFormat::RGBA16Unorm, 23, 8 },
		{ Indium::PixelFormat::Depth32Float, 24, 4 },    { Indium::PixelFormat::Depth16Unorm, 25, 2 },
		{ Indium::PixelFormat::Depth32Float_Stencil8, 26, 8 }, { Indium::PixelFormat::Depth24Unorm_Stencil8, 27, 4 },
		{ Indium::PixelFormat::Stencil8, 28, 1 },
		// block compressed: bytes per 4x4 block
		{ Indium::PixelFormat::BC1_RGBA, 29, 8 },        { Indium::PixelFormat::BC1_RGBA_sRGB, 30, 8 },
		{ Indium::PixelFormat::BC2_RGBA, 31, 16 },       { Indium::PixelFormat::BC2_RGBA_sRGB, 32, 16 },
		{ Indium::PixelFormat::BC3_RGBA, 33, 16 },       { Indium::PixelFormat::BC3_RGBA_sRGB, 34, 16 },
		{ Indium::PixelFormat::BC4_RUnorm, 35, 8 },      { Indium::PixelFormat::BC4_RSnorm, 36, 8 },
		{ Indium::PixelFormat::BC5_RGUnorm, 37, 16 },    { Indium::PixelFormat::BC5_RGSnorm, 38, 16 },
		{ Indium::PixelFormat::BC6H_RGBFloat, 39, 16 },  { Indium::PixelFormat::BC6H_RGBUfloat, 40, 16 },
		{ Indium::PixelFormat::BC7_RGBAUnorm, 41, 16 },  { Indium::PixelFormat::BC7_RGBAUnorm_sRGB, 42, 16 },
		// integer formats (texel buffers, integer render targets): airjitd/texture.h 43-54
		{ Indium::PixelFormat::R8Uint, 43, 1 },          { Indium::PixelFormat::RG8Uint, 44, 2 },
		{ Indium::PixelFormat::RGBA32Uint, 45, 16 },     { Indium::PixelFormat::R8Sint, 46, 1 },
		{ Indium::PixelFormat::RG8Sint, 47, 2 },         { Indium::PixelFormat::RGBA8Sint, 48, 4 },
		{ Indium::PixelFormat::R16Sint, 49, 2 },         { Indium::PixelFormat::RG16Sint, 50, 4 },
		{ Indium::PixelFormat::RGBA16Sint, 51, 8 },      { Indium::PixelFormat::R32Sint, 52, 4 },
		{ Indium::PixelFormat::RG32Sint, 53, 8 },        { Indium::PixelFormat::RGBA32Sint, 54, 16 },
	};

	int pdFormat(Indium::PixelFormat format) {
		for (auto &f: formats) {
			if (f.metal == format)
				return f.pd;
		}
		return -1;
	}

	size_t formatBytes(Indium::PixelFormat format) {
		for (auto &f: formats) {
			if (f.metal == format)
				return f.bpp;
		}
		return 4;
	}

	// texels a block spans each way: 4 for the block compressed formats, else 1 (formatBytes is then per block)
	size_t formatBlock(Indium::PixelFormat format) {
		int f = pdFormat(format);
		return f >= 29 && f <= 42 ? 4 : 1;
	}

	size_t formatRowBytes(Indium::PixelFormat format, size_t width) {
		size_t b = formatBlock(format);
		return (width + b - 1) / b * formatBytes(format);
	}

	size_t formatRows(Indium::PixelFormat format, size_t height) {
		size_t b = formatBlock(format);
		return (height + b - 1) / b;
	}

	Buffer::Buffer(std::shared_ptr<Device> device, size_t length): _device(device), _length(length) {
		_off = heap.alloc(length);
		memset(pdmtl_arena() + _off, 0, length);
	}

	Buffer::Buffer(std::shared_ptr<Device> device, size_t length, uint32_t off, std::shared_ptr<void> backing):
		_device(device), _length(length), _off(off), _owned(false), _backing(std::move(backing)) {}

	Buffer::~Buffer() {
		if (_owned)
			heap.free(_off);
	}

	std::shared_ptr<Indium::Device> Buffer::device() {
		return _device.lock();
	}

	void *Buffer::contents() {
		return pdmtl_arena() + _off;
	}

	static size_t mipDim(size_t n, size_t k) {
		n >>= k;
		return n ? n : 1;
	}

	// levels one after another with tight rows, each slice a whole chain (a 3D level holds its depth slices)
	size_t Texture::plan(const Indium::TextureDescriptor &desc) {
		size_t off = 0;
		bool three = desc.textureType == Indium::TextureType::e3D;

		_levels = desc.mipmapLevelCount ? (desc.mipmapLevelCount < PD_TEX_LEVELS ? desc.mipmapLevelCount : PD_TEX_LEVELS) : 1;
		switch (desc.textureType) {
		case Indium::TextureType::e2DArray: case Indium::TextureType::e2DMultisampleArray: _slices = desc.arrayLength; break;
		case Indium::TextureType::eCube: _slices = 6; break;
		case Indium::TextureType::eCubeArray: _slices = 6 * desc.arrayLength; break;
		case Indium::TextureType::e3D: _slices = desc.depth; break;
		default: _slices = 1; break;
		}
		if (!_slices)
			_slices = 1;
		for (size_t k = 0; k < _levels; k++) {
			_levelOff[k] = (uint32_t)off;
			off += formatRowBytes(desc.pixelFormat, mipDim(desc.width, k)) * formatRows(desc.pixelFormat, mipDim(desc.height, k)) *
			       (three ? mipDim(_slices, k) : 1);
			off = (off + 15) & ~(size_t)15;
		}
		_sliceBytes = three ? 0 : (off + 255) & ~(size_t)255;
		_bpr = formatRowBytes(desc.pixelFormat, desc.width);
		return three ? off : _sliceBytes * _slices;
	}

	size_t Texture::storageSize(const Indium::TextureDescriptor &desc) {
		Texture t(nullptr, desc, 0, nullptr, false);

		return t._total;
	}

	Texture::Texture(std::shared_ptr<Device> device, const Indium::TextureDescriptor &desc): _device(device), _desc(desc) {
		size_t total = plan(desc);

		_off = heap.alloc(total);
		_total = total;
		memset(pdmtl_arena() + _off, 0, total);
		// bytes a texture freed before may still sit on the GPU
		pdmtl_dirty(_off, (uint32_t)total);
	}

	Texture::Texture(std::shared_ptr<Device> device, const Indium::TextureDescriptor &desc, uint32_t off, std::shared_ptr<void> backing,
	                 bool cpuWritten): _device(device), _desc(desc), _off(off), _owned(false), _backing(std::move(backing)),
	                 _cpuWritten(cpuWritten) {
		_total = plan(desc);
		if (device)
			pdmtl_dirty(_off, (uint32_t)_total);
	}

	Texture::Texture(std::shared_ptr<Device> device, struct pdmtl_drawable *drawable): _device(device), _owned(false), _drawable(drawable) {
		// format 18: an sRGB swapchain (pdmtl_window_set_srgb)
		_desc = Indium::TextureDescriptor::texture2DDescriptor(drawable->texture.fmt == 18 ? Indium::PixelFormat::BGRA8Unorm_sRGB :
		                                                       Indium::PixelFormat::BGRA8Unorm, drawable->texture.w,
		                                                       drawable->texture.h, false);
		_desc.usage = Indium::TextureUsage::RenderTarget;
		_off = drawable->texture.off;
		_bpr = drawable->texture.bpr;
		_total = _bpr * drawable->texture.h;
	}

	Texture::~Texture() {
		if (_owned) {
			pdmtl_dirty(_off, (uint32_t)_total);
			heap.free(_off);
		}
	}

	std::shared_ptr<Indium::Device> Texture::device() {
		return _device.lock();
	}

	uint8_t *Texture::bytes() const {
		return pdmtl_arena() + _off;
	}

	// multisample textures hold one sample here: drawn into like a 2D texture and resolved by copying
	static Indium::TextureType storageType(Indium::TextureType ty) {
		if (ty == Indium::TextureType::e2DMultisample)
			return Indium::TextureType::e2D;
		if (ty == Indium::TextureType::e2DMultisampleArray)
			return Indium::TextureType::e2DArray;
		return ty;
	}

	// a surface's new contents are copied in when the surface moved on since the last use
	void Texture::pullSurface() const {
		if (_link && _link->pull && _link->pull(bytes(), _bpr))
			pdmtl_dirty(_off, (uint32_t)_total);
	}

	bool Texture::describe(struct PDDrawTex *t) const {
		int f = pdFormat(_desc.pixelFormat);
		auto ty = storageType(_desc.textureType);

		// a texture buffer and a 1D texture are one row of texels
		if (f < 0 || (ty != Indium::TextureType::e2D && ty != Indium::TextureType::e2DArray && ty != Indium::TextureType::eCube &&
		              ty != Indium::TextureType::eCubeArray && ty != Indium::TextureType::e3D &&
		              ty != Indium::TextureType::eTextureBuffer && ty != Indium::TextureType::e1D))
			return false;
		// the CPU writes such a texture through its buffer, which tells nobody
		if (_cpuWritten)
			pdmtl_dirty(_off, (uint32_t)_total);
		pullSurface();
		t->id = 1, t->off = _off, t->w = (uint32_t)_desc.width, t->h = (uint32_t)_desc.height;
		t->bpr = (uint32_t)_bpr, t->fmt = (uint32_t)f;
		return true;
	}

	void Texture::layout(struct PDTexLayout *l) const {
		memset(l, 0, sizeof(*l));
		// a plain 2D texture with one level needs none, as with the kext
		if (_drawable || (storageType(_desc.textureType) == Indium::TextureType::e2D && _levels <= 1))
			return;
		// a texture buffer or a 1D texture is a single row, bound like a plain 2D texture
		if (_desc.textureType == Indium::TextureType::eTextureBuffer || _desc.textureType == Indium::TextureType::e1D)
			return;
		l->flags = PD_TEXLAYOUT_SET;
		l->type = (uint8_t)storageType(_desc.textureType), l->levels = (uint8_t)_levels, l->slices = (uint32_t)_slices;
		l->sliceBytes = (uint32_t)_sliceBytes;
		for (size_t k = 0; k < _levels; k++) {
			l->level[k].off = _levelOff[k], l->level[k].bpr = (uint32_t)levelBytesPerRow(k);
		}
	}

	size_t Texture::levelBytesPerRow(size_t level) const {
		return level ? formatRowBytes(_desc.pixelFormat, mipDim(_desc.width, level)) : _bpr;
	}

	uint8_t *Texture::levelBytes(size_t level, size_t slice) const {
		if (_desc.textureType == Indium::TextureType::e3D)
			return bytes() + _levelOff[level] + slice * levelBytesPerRow(level) * formatRows(_desc.pixelFormat, mipDim(_desc.height, level));
		return bytes() + slice * _sliceBytes + _levelOff[level];
	}

	Texture::Texture(std::shared_ptr<Texture> parent, Indium::PixelFormat format, Indium::TextureType type, size_t level0,
	                 size_t levelCount, size_t slice0, size_t sliceCount, const Indium::TextureSwizzleChannels &swizzle)
	    : _device(parent->_device), _desc(parent->_desc), _owned(false), _drawable(parent->_drawable), _parent(parent) {
		_desc.pixelFormat = format, _desc.textureType = type, _desc.swizzle = swizzle;
		_desc.width = mipDim(parent->_desc.width, level0), _desc.height = mipDim(parent->_desc.height, level0);
		_desc.mipmapLevelCount = levelCount;
		_levels = levelCount, _slices = sliceCount, _sliceBytes = parent->_sliceBytes;
		switch (type) {
		case Indium::TextureType::e2DArray: _desc.arrayLength = sliceCount; break;
		case Indium::TextureType::eCubeArray: _desc.arrayLength = sliceCount / 6; break;
		case Indium::TextureType::e3D:
			// from level0 on, a 3D level's depth is its slices
			_slices = mipDim(sliceCount, level0), _desc.depth = _slices;
			break;
		default: _desc.arrayLength = 1; break;
		}
		// level0 of slice0 is where the view starts, its other levels sit at the same distances as in the parent
		_off = parent->_off + (uint32_t)(slice0 * parent->_sliceBytes) + parent->_levelOff[level0];
		for (size_t k = 0; k < levelCount; k++)
			_levelOff[k] = parent->_levelOff[level0 + k] - parent->_levelOff[level0];
		_bpr = parent->levelBytesPerRow(level0);
		_total = parent->_total - (_off - parent->_off);
	}

	std::shared_ptr<Indium::Texture> Texture::newTextureView(Indium::PixelFormat format) {
		return newTextureView(format, _desc.textureType, { 0, _levels }, { 0, _slices }, _desc.swizzle);
	}

	std::shared_ptr<Indium::Texture> Texture::newTextureView(Indium::PixelFormat format, Indium::TextureType type,
	                                                         const Indium::Range<size_t> &levels, const Indium::Range<size_t> &slices) {
		return newTextureView(format, type, levels, slices, _desc.swizzle);
	}

	// the same bytes reinterpreted: formats of one size, and a 3D texture only whole
	std::shared_ptr<Indium::Texture> Texture::newTextureView(Indium::PixelFormat format, Indium::TextureType type,
	                                                         const Indium::Range<size_t> &levels, const Indium::Range<size_t> &slices,
	                                                         const Indium::TextureSwizzleChannels &swizzle) {
		bool three = type == Indium::TextureType::e3D || _desc.textureType == Indium::TextureType::e3D;

		if (formatBytes(format) != formatBytes(_desc.pixelFormat) || formatBlock(format) != formatBlock(_desc.pixelFormat) ||
		    !levels.length || !slices.length || levels.start + levels.length > _levels)
			return nullptr;
		// a 3D texture is one slice to Metal (its depth is not slices): a view of it is 3D too, slice range {0, 1}
		if (three) {
			if (type != _desc.textureType || slices.start || (slices.length != 1 && slices.length != _slices))
				return nullptr;
			return std::make_shared<Texture>(std::static_pointer_cast<Texture>(shared_from_this()), format, type, levels.start,
			                                 levels.length, 0, _slices, swizzle);
		}
		if (slices.start + slices.length > _slices)
			return nullptr;
		return std::make_shared<Texture>(std::static_pointer_cast<Texture>(shared_from_this()), format, type, levels.start,
		                                 levels.length, slices.start, slices.length, swizzle);
	}

	void Texture::replaceRegion(Indium::Region region, size_t level, const void *bytes, size_t bytesPerRow) {
		replaceRegion(region, level, 0, bytes, bytesPerRow, 0);
	}

	void Texture::replaceRegion(Indium::Region region, size_t level, size_t slice, const void *src, size_t bytesPerRow, size_t bytesPerImage) {
		// in blocks for a compressed format: the source's rows are then rows of blocks
		size_t bpp = formatBytes(_desc.pixelFormat), blk = formatBlock(_desc.pixelFormat);
		size_t row = formatRowBytes(_desc.pixelFormat, region.size.width), h = formatRows(_desc.pixelFormat, mipDim(_desc.height, level));
		size_t pitch = levelBytesPerRow(level), depth = region.size.depth ? region.size.depth : 1;
		size_t ox = region.origin.x / blk, oy = region.origin.y / blk, rows = formatRows(_desc.pixelFormat, region.size.height);

		if (level >= _levels)
			return;
		// the GPU may hold newer bytes of this texture: they come back first, and the write goes up next time
		pdmtl_sync(_off, (uint32_t)_total);
		// a 3D region's depth slices are the slices here, bytesPerImage apart in the source
		for (size_t z = 0; z < depth; z++) {
			size_t s = _desc.textureType == Indium::TextureType::e3D ? region.origin.z + z : slice;
			uint8_t *dst = levelBytes(level, s);

			for (size_t y = 0; y < rows && oy + y < h; y++) {
				memcpy(dst + (oy + y) * pitch + ox * bpp, (const uint8_t *)src + z * bytesPerImage + y * bytesPerRow, row);
			}
		}
		pdmtl_dirty(_off, (uint32_t)_total);
	}

	SamplerState::SamplerState(std::shared_ptr<Device> device, const Indium::SamplerDescriptor &d): _device(device) {
		_flags = (d.minFilter == Indium::SamplerMinMagFilter::Linear ? 1u : 0) |
		         (d.magFilter == Indium::SamplerMinMagFilter::Linear ? 4u : 0) | (uint32_t)d.sAddressMode << 8 |
		         (uint32_t)d.tAddressMode << 12 | (uint32_t)d.rAddressMode << 16 | (uint32_t)d.mipFilter << 4 |
		         (d.normalizedCoordinates ? 0x80000000u : 0) | PD_SAMP_VALID;
	}

	std::shared_ptr<Indium::Device> SamplerState::device() {
		return _device.lock();
	}

	std::shared_ptr<Indium::Device> DepthStencilState::device() {
		return _device.lock();
	}

	uint32_t DepthStencilState::depthWord() const {
		return PD_DEPTH_SET | (uint32_t)_desc.depthCompareFunction | (_desc.depthWriteEnabled ? PD_DEPTH_WRITE : 0);
	}

	uint32_t DepthStencilState::stencilWord(bool back) const {
		auto &o = back ? _desc.backFaceStencil : _desc.frontFaceStencil;

		if (!o)
			return 7u | 0xffu << 12 | 0xffu << 20;
		return PD_STENCIL_ON | (uint32_t)o->stencilCompareFunction | (uint32_t)o->stencilFailureOperation << 3 |
		       (uint32_t)o->depthFailureOperation << 6 | (uint32_t)o->depthStencilPassOperation << 9 | (o->readMask & 0xff) << 12 |
		       (o->writeMask & 0xff) << 20;
	}

	Function::Function(std::shared_ptr<Device> device, struct pdmtl_fn *fn, const std::string &name): _device(device), _fn(fn) {
		_name = name;
	}

	std::shared_ptr<Indium::Device> Function::device() {
		return _device.lock();
	}

	std::shared_ptr<Indium::Function> Library::newFunction(const std::string &name) {
		struct pdmtl_fn *fn = pdmtl_fn_get(_lib, name.c_str());

		if (!fn)
			return nullptr;
		// constants not given are undefined, static initialisers get folded either way
		return std::make_shared<Function>(_device.lock(), pdmtl_fn_specialize(fn, nullptr, 0), name);
	}

	std::vector<FunctionConstant> functionConstants(std::shared_ptr<Indium::Function> function) {
		auto f = std::dynamic_pointer_cast<Function>(function);
		struct pdmtl_constant_info info[64];
		std::vector<FunctionConstant> out;

		if (!f)
			return out;
		unsigned n = pdmtl_fn_constants(f->fn(), info, 64);
		for (unsigned i = 0; i < n; i++)
			out.push_back({ info[i].name, info[i].type, info[i].index, info[i].required != 0 });
		return out;
	}

	std::shared_ptr<Indium::Function> specializeFunction(std::shared_ptr<Indium::Library> library, const std::string &name,
	                                                     const std::vector<ConstantValue> &values) {
		auto l = std::dynamic_pointer_cast<Library>(library);
		std::vector<struct pdmtl_constant> c(values.size());

		if (!l)
			return nullptr;
		struct pdmtl_fn *fn = pdmtl_fn_get(l->lib(), name.c_str());
		if (!fn)
			return nullptr;
		for (size_t i = 0; i < values.size(); i++) {
			memset(&c[i], 0, sizeof(c[i]));
			c[i].index = values[i].index;
			snprintf(c[i].name, sizeof(c[i].name), "%s", values[i].name.c_str());
			memcpy(c[i].bytes, values[i].bytes.data(), std::min(values[i].bytes.size(), sizeof(c[i].bytes)));
		}
		return std::make_shared<Function>(std::dynamic_pointer_cast<Device>(l->device()),
		                                  pdmtl_fn_specialize(fn, c.data(), (unsigned)c.size()), name);
	}

	std::shared_ptr<Indium::Device> Library::device() {
		return _device.lock();
	}

	RenderPipelineState::RenderPipelineState(std::shared_ptr<Device> device, const Indium::RenderPipelineDescriptor &desc)
		: _device(device), _desc(desc) {
		_maxTotalThreadsPerThreadgroup = 1024;
	}

	std::shared_ptr<Indium::Device> RenderPipelineState::device() {
		return _device.lock();
	}

	uint32_t RenderPipelineState::blend(size_t i) const {
		uint32_t s = 1;

		if (i >= _desc.colorAttachments.size())
			return s | 15u << 28;
		auto &c = _desc.colorAttachments[i];
		s |= (uint32_t)c.writeMask << 28;
		// dual-source factors have no packed form: those attachments store unblended
		if (c.blendingEnabled && (size_t)c.sourceRGBBlendFactor <= 14 && (size_t)c.destinationRGBBlendFactor <= 14 &&
		    (size_t)c.sourceAlphaBlendFactor <= 14 && (size_t)c.destinationAlphaBlendFactor <= 14) {
			s |= 2u | (uint32_t)c.sourceRGBBlendFactor << 2 | (uint32_t)c.destinationRGBBlendFactor << 7;
			s |= (uint32_t)c.sourceAlphaBlendFactor << 12 | (uint32_t)c.destinationAlphaBlendFactor << 17;
			s |= (uint32_t)c.rgbBlendOperation << 22 | (uint32_t)c.alphaBlendOperation << 25;
		}
		return s;
	}

	ComputePipelineState::ComputePipelineState(std::shared_ptr<Device> device, std::shared_ptr<Function> function)
		: _device(device), _function(function) {
		_maxTotalThreadsPerThreadgroup = 1024;
		_threadExecutionWidth = 32;
		_staticThreadgroupMemoryLength = 0;
	}

	std::shared_ptr<Indium::Device> ComputePipelineState::device() {
		return _device.lock();
	}

	Device::Device() {
		_worker = std::thread([this] { run(); });
	}

	Device::~Device() {
		{
			std::lock_guard<std::mutex> lock(_mu);
			_stop = true;
		}
		_cv.notify_all();
		_worker.join();
	}

	std::shared_ptr<Indium::CommandQueue> Device::newCommandQueue() {
		Indium::ensureStarted();
		return std::make_shared<CommandQueue>(shared_from_this());
	}

	std::shared_ptr<Indium::RenderPipelineState> Device::newRenderPipelineState(const Indium::RenderPipelineDescriptor &desc) {
		Indium::ensureStarted();
		if (!desc.vertexFunction || !desc.fragmentFunction)
			return nullptr;
		return std::make_shared<RenderPipelineState>(shared_from_this(), desc);
	}

	std::shared_ptr<Indium::ComputePipelineState> Device::newComputePipelineState(const Indium::ComputePipelineDescriptor &desc, Indium::PipelineOption options,
	                                                                              std::shared_ptr<Indium::ComputePipelineReflection> reflection) {
		return newComputePipelineState(desc.computeFunction, options, reflection);
	}

	std::shared_ptr<Indium::ComputePipelineState> Device::newComputePipelineState(std::shared_ptr<Indium::Function> function, Indium::PipelineOption,
	                                                                              std::shared_ptr<Indium::ComputePipelineReflection>) {
		Indium::ensureStarted();
		auto f = std::dynamic_pointer_cast<Function>(function);

		if (!f)
			return nullptr;
		return std::make_shared<ComputePipelineState>(shared_from_this(), f);
	}

	std::shared_ptr<Indium::Buffer> Device::newBuffer(size_t length, Indium::ResourceOptions) {
		Indium::ensureStarted();
		return std::make_shared<Buffer>(shared_from_this(), length);
	}

	std::shared_ptr<Indium::Buffer> Device::newBuffer(const void *pointer, size_t length, Indium::ResourceOptions) {
		Indium::ensureStarted();
		auto b = std::make_shared<Buffer>(shared_from_this(), length);

		memcpy(b->contents(), pointer, length);
		return b;
	}

	std::shared_ptr<Indium::Library> Device::newLibrary(const void *data, size_t length) {
		Indium::ensureStarted();
		struct pdmtl_lib *lib = pdmtl_lib_load_data(data, length);

		if (!lib)
			return nullptr;
		return std::make_shared<Library>(shared_from_this(), lib);
	}

	std::shared_ptr<Indium::Texture> Device::newTexture(const Indium::TextureDescriptor &desc) {
		Indium::ensureStarted();
		return std::make_shared<Texture>(shared_from_this(), desc);
	}

	std::shared_ptr<Indium::SamplerState> Device::newSamplerState(const Indium::SamplerDescriptor &desc) {
		Indium::ensureStarted();
		return std::make_shared<SamplerState>(shared_from_this(), desc);
	}

	std::shared_ptr<Indium::DepthStencilState> Device::newDepthStencilState(const Indium::DepthStencilDescriptor &desc) {
		Indium::ensureStarted();
		return std::make_shared<DepthStencilState>(shared_from_this(), desc);
	}

	// handlers already run on the device's thread: polling only has to sleep until woken
	void Device::pollEvents(uint64_t timeoutNanoseconds) {
		std::unique_lock<std::mutex> lock(_mu);

		if (timeoutNanoseconds == UINT64_MAX)
			_poll.wait(lock, [this] { return _woken; });
		else
			_poll.wait_for(lock, std::chrono::nanoseconds(timeoutNanoseconds), [this] { return _woken; });
		_woken = false;
	}

	void Device::wakeupEventLoop() {
		std::lock_guard<std::mutex> lock(_mu);

		_woken = true;
		_poll.notify_all();
	}

	void Device::submit(std::shared_ptr<CommandBuffer> cb) {
		{
			std::lock_guard<std::mutex> lock(_mu);
			_queue.push_back(std::move(cb));
		}
		_cv.notify_all();
	}

	void Device::waitIdle() {
		std::unique_lock<std::mutex> lock(_mu);

		_idle.wait(lock, [this] { return _queue.empty() && !_busy; });
	}

	void Device::run() {
		std::unique_lock<std::mutex> lock(_mu);

		for (;;) {
			_cv.wait(lock, [this] { return _stop || !_queue.empty(); });
			if (_queue.empty())
				break;
			auto cb = std::move(_queue.front());
			_queue.pop_front();
			_busy = true;
			lock.unlock();
			cb->execute();
			cb.reset();
			lock.lock();
			_busy = false;
			if (_queue.empty())
				_idle.notify_all();
		}
	}

	struct ArenaRange {
		uint32_t off;
		explicit ArenaRange(size_t size): off(heap.alloc(size)) {}
		~ArenaRange() { heap.free(off); }
	};

	std::shared_ptr<void> newArenaRange(size_t size) {
		return std::make_shared<ArenaRange>(size);
	}

	std::shared_ptr<Indium::Buffer> newPlacedBuffer(std::shared_ptr<Indium::Device> device, std::shared_ptr<void> range, size_t offset,
	                                                size_t length) {
		auto r = std::static_pointer_cast<ArenaRange>(range);

		return std::make_shared<Buffer>(std::dynamic_pointer_cast<Device>(device), length, r->off + (uint32_t)offset, range);
	}

	std::shared_ptr<Indium::Texture> newPlacedTexture(std::shared_ptr<Indium::Device> device, std::shared_ptr<void> range, size_t offset,
	                                                  const Indium::TextureDescriptor &descriptor) {
		auto r = std::static_pointer_cast<ArenaRange>(range);

		return std::make_shared<Texture>(std::dynamic_pointer_cast<Device>(device), descriptor, r->off + (uint32_t)offset, range, false);
	}

	std::shared_ptr<Indium::Texture> newBufferTexture(std::shared_ptr<Indium::Buffer> buffer, size_t offset,
	                                                  const Indium::TextureDescriptor &descriptor) {
		auto b = std::dynamic_pointer_cast<Buffer>(buffer);

		if (!b)
			return nullptr;
		return std::make_shared<Texture>(std::dynamic_pointer_cast<Device>(b->device()), descriptor, b->offset() + (uint32_t)offset,
		                                 b, true);
	}

	bool linkSurface(std::shared_ptr<Indium::Texture> texture, std::shared_ptr<SurfaceLink> link) {
		auto t = std::dynamic_pointer_cast<Texture>(texture);

		// a view or a drawable has no bytes of its own to mirror
		if (!t || t->drawable() || t->levels() != 1)
			return false;
		t->setSurfaceLink(std::move(link));
		return true;
	}

	size_t textureStorageSize(const Indium::TextureDescriptor &descriptor) {
		return Texture::storageSize(descriptor);
	}

	size_t tightRowBytes(Indium::PixelFormat format, size_t width) {
		return formatRowBytes(format, width);
	}
};
