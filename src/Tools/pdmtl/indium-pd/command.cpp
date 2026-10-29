// indium-pd: command queues, buffers and encoders. Encoders keep Metal's bound state and snapshot it
// into a PD request per draw or dispatch, which runs on the device's thread at commit
#include "pd.hpp"

#include <indium/pd-sync.hpp>
#include <indium/pd-tile.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

extern "C" {
#include "vulkan/vk.h"
}

namespace IndiumPD {
	static std::shared_ptr<Buffer> asBuffer(const std::shared_ptr<Indium::Buffer> &b) {
		return std::dynamic_pointer_cast<Buffer>(b);
	}

	static std::shared_ptr<Texture> asTexture(const std::shared_ptr<Indium::Texture> &t) {
		return std::dynamic_pointer_cast<Texture>(t);
	}

	static void report(const char *what, int rc) {
		static int told;

		if (rc && told++ < 8)
			fprintf(stderr, "indium-pd: %s failed: %d\n", what, rc);
	}

	// INDIUM_PD_TRACE=1: one line per job
	static bool trace() {
		static int on = -1;

		if (on < 0)
			on = getenv("INDIUM_PD_TRACE") != nullptr;
		return on;
	}

	uint64_t nowUs() {
		struct timespec ts;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
	}

	bool profiling() {
		static int on = -1;

		if (on < 0)
			on = getenv("INDIUM_PD_PROFILE") != nullptr;
		return on;
	}

	// sums since the last line, all in microseconds
	static struct {
		std::mutex mu;
		uint64_t start, jobs_us, idle, acquire, present, vk, gl, cpu, frames;
		struct pd_vk_stats s;
	} prof;

	void profileJob(uint64_t us, bool vulkan) {
		std::lock_guard<std::mutex> lock(prof.mu);
		auto &s = prof.s;
		const auto &l = pd_vk_last;

		prof.jobs_us += us;
		(vulkan ? prof.vk : pdmtl_last_on_opengl() ? prof.gl : prof.cpu)++;
		if (!vulkan)
			return;
		s.translate += l.translate, s.pipeline += l.pipeline, s.upload += l.upload, s.run += l.run, s.readback += l.readback;
		s.wait += l.wait, s.up_bytes += l.up_bytes, s.down_bytes += l.down_bytes, s.teardown += l.teardown;
		s.submits += l.submits, s.allocs += l.allocs, s.pipeline_cached += l.pipeline_cached;
	}

	void profileAcquire(uint64_t idleUs, uint64_t acquireUs) {
		std::lock_guard<std::mutex> lock(prof.mu);

		prof.idle += idleUs, prof.acquire += acquireUs;
	}

	void profilePresent(uint64_t us) {
		std::lock_guard<std::mutex> lock(prof.mu);
		uint64_t t = nowUs();

		prof.present += us, prof.frames++;
		if (!prof.start)
			prof.start = t;
		if (t - prof.start < 1000000)
			return;
		double sec = (t - prof.start) / 1e6, f = prof.frames ? (double)prof.frames : 1;
		auto &s = prof.s;
		fprintf(stderr,
		        "indium-pd: %.1f fps, per frame ms: jobs %.2f (%llu vk %llu gl %llu cpu) [translate %.2f pipeline %.2f upload %.2f run %.2f "
		        "readback %.2f teardown %.2f; waits %.2f in %.1f submits, %.1f allocs, %.2f MB up %.2f MB back, %.1f pipelines cached] "
		        "idle-wait %.2f acquire %.2f present %.2f\n",
		        prof.frames / sec, prof.jobs_us / f / 1e3, (unsigned long long)prof.vk, (unsigned long long)prof.gl, (unsigned long long)prof.cpu, s.translate / f / 1e3,
		        s.pipeline / f / 1e3, s.upload / f / 1e3, s.run / f / 1e3, s.readback / f / 1e3, s.teardown / f / 1e3, s.wait / f / 1e3,
		        s.submits / f, s.allocs / f, s.up_bytes / f / 1e6, s.down_bytes / f / 1e6, s.pipeline_cached / f, prof.idle / f / 1e3,
		        prof.acquire / f / 1e3, prof.present / f / 1e3);
		prof.jobs_us = prof.idle = prof.acquire = prof.present = prof.vk = prof.gl = prof.cpu = prof.frames = 0;
		memset(&prof.s, 0, sizeof(prof.s));
		prof.start = t;
	}

	void runDraw(struct PDDrawRequest &r) {
		uint64_t t0 = profiling() ? nowUs() : 0;
		int rc = pdmtl_exec_draw(&r);

		if (t0)
			profileJob(nowUs() - t0, pdmtl_last_on_vulkan());

		if (trace())
			fprintf(stderr, "indium-pd: draw prim %u count %u%s inst %u target %ux%u fmt %u vp %g,%g %gx%g -> %d (%s)\n", r.prim, r.count,
			        r.indexed ? " indexed" : "", r.instances, r.tw, r.th, r.tfmt, r.viewport[0], r.viewport[1], r.viewport[2], r.viewport[3], rc,
			        pdmtl_last_on_opengl() ? "opengl" : pdmtl_last_on_vulkan() ? "vulkan" : "cpu");
		report("draw", rc);
	}

	// an indirect draw's arguments (MTLDraw[Indexed]PrimitivesIndirectArguments) as the GPU left them when the draw
	// runs, false when they draw nothing
	static bool indirectDraw(struct PDDrawRequest &r, uint32_t off) {
		const uint32_t *a;

		pdmtl_sync(off, 20);
		a = (const uint32_t *)(pdmtl_arena() + off);
		if (!a[0] || !a[1])
			return false;
		r.count = a[0], r.instances = a[1];
		if (r.indexed) {
			r.indexOff += a[2] * r.indexBytes;
			r.ext.baseVertex = (int32_t)a[3], r.ext.baseInstance = a[4];
		} else {
			r.ext.firstVertex = a[2], r.ext.baseInstance = a[3];
		}
		return true;
	}

	// an indirect dispatch's threadgroupsPerGrid (MTLDispatchThreadgroupsIndirectArguments) when it runs
	static bool indirectDispatch(struct PDComputeRequest &r, uint32_t off) {
		const uint32_t *a;

		pdmtl_sync(off, 12);
		a = (const uint32_t *)(pdmtl_arena() + off);
		if (!a[0] || !a[1] || !a[2])
			return false;
		r.grid[0] = a[0], r.grid[1] = a[1], r.grid[2] = a[2];
		return true;
	}

	void runCompute(struct PDComputeRequest &r) {
		uint64_t t0 = profiling() ? nowUs() : 0;
		int rc = pdmtl_exec_compute(&r);

		if (t0)
			profileJob(nowUs() - t0, pdmtl_last_on_vulkan());
		report("dispatch", rc);
	}

	void runTile(struct PDTileRequest &r) {
		uint64_t t0 = profiling() ? nowUs() : 0;
		int rc = pdmtl_exec_tile(&r);

		if (t0)
			profileJob(nowUs() - t0, pdmtl_last_on_vulkan());
		report("tile dispatch", rc);
	}

	// metal's buffer slots per stage, requests carry PD_DRAW_BUFS (the kext's layout too)
	enum { PD_METAL_BUFS = 31 };

	// bound buffers: arena offset and length, or bytes copied into the scratch ring
	struct Binding {
		std::shared_ptr<Buffer> buffer;
		uint32_t off = 0, size = 0;
	};

	static void bindBuffer(Binding *slots, size_t n, std::shared_ptr<Indium::Buffer> buffer, size_t offset, size_t index) {
		if (index >= n)
			return;
		auto b = asBuffer(buffer);
		slots[index].buffer = b;
		slots[index].off = b ? b->offset() + (uint32_t)offset : 0;
		slots[index].size = b ? (uint32_t)(b->length() - offset) : 0;
	}

	static void bindBytes(Binding *slots, size_t n, const void *bytes, size_t length, size_t index) {
		if (index >= n)
			return;
		slots[index].buffer = nullptr;
		slots[index].off = heap.scratch(bytes, length);
		slots[index].size = (uint32_t)length;
	}

	static void setOffset(Binding *slots, size_t n, size_t offset, size_t index) {
		if (index >= n || !slots[index].buffer)
			return;
		slots[index].off = slots[index].buffer->offset() + (uint32_t)offset;
		slots[index].size = (uint32_t)(slots[index].buffer->length() - offset);
	}

	// metal leaves a buffer the shader reads but the encoder never bound undefined, here it reads 64 KB of zeros,
	// so a draw that forgets one (Apple's OpenGL-to-Metal sample, its reflection pass) still runs
	static void fillBuffers(struct PDDrawBuffer *out, const Binding *slots, size_t n) {
		for (size_t i = 0; i < n; i++) {
			if (!slots[i].size)
				continue;
			out[i].id = (uint32_t)i + 1, out[i].off = slots[i].off, out[i].size = slots[i].size;
		}
	}

	class RenderEncoder: public Indium::RenderCommandEncoder {
	public:
		RenderEncoder(std::shared_ptr<CommandBuffer> cb, const Indium::RenderPassDescriptor &pass): _cb(cb), _pass(pass) {
			auto &c = pass.colorAttachments;
			for (size_t i = 0; i < c.size() && i < 4; i++) {
				auto t = asTexture(c[i].texture);
				// a pass that only clears a surface still changes it
				if (t && t->surfaceLink())
					cb->noteSurfaceWrite(t);
				if (!t || c[i].loadAction != Indium::LoadAction::Clear)
					continue;
				Indium::ClearColor col = c[i].clearColor;
				cb->add([t, col] { clear(t, col); });
			}
			if (!c.empty() && c[0].texture) {
				_viewport = { 0, 0, (double)c[0].texture->width(), (double)c[0].texture->height(), 0, 1 };
			}
			// the depth and stencil attachments are one texture here: the depth one, else the stencil one
			if (pass.depthAttachment && pass.depthAttachment->texture)
				_depth = asTexture(pass.depthAttachment->texture);
			else if (pass.stencilAttachment && pass.stencilAttachment->texture)
				_depth = asTexture(pass.stencilAttachment->texture);
			{
				int which = (pass.depthAttachment && pass.depthAttachment->loadAction == Indium::LoadAction::Clear ? PDMTL_CLEAR_DEPTH : 0) |
				            (pass.stencilAttachment && pass.stencilAttachment->loadAction == Indium::LoadAction::Clear ? PDMTL_CLEAR_STENCIL : 0);
				float d = pass.depthAttachment ? (float)pass.depthAttachment->clearDepth : 1.0f;
				unsigned s = pass.stencilAttachment ? pass.stencilAttachment->clearStencil : 0;
				auto z = _depth;
				struct PDDrawTex zt;

				if (z && which && z->describe(&zt))
					cb->add([zt, d, s, which] { pdmtl_fill_depth(&zt, d, s, which); });
			}
		}

		// a resolving attachment's single sample goes to its resolve texture once the pass's draws have run
		void endEncoding() override {
			auto &c = _pass.colorAttachments;
			auto cb = _cb.lock();
			if (!cb)
				return;
			for (size_t i = 0; i < c.size(); i++) {
				auto act = c[i].storeAction;
				if (act != Indium::StoreAction::MultisampleResolve && act != Indium::StoreAction::StoreAndMultisampleResolve)
					continue;
				auto s = asTexture(c[i].texture), d = asTexture(c[i].resolveTexture);
				size_t level = c[i].resolveLevel, slice = c[i].resolveSlice;
				if (!s || !d || s == d || formatBytes(s->pixelFormat()) != formatBytes(d->pixelFormat()) || level >= d->levels())
					continue;
				cb->add([s, d, level, slice] {
					size_t bpp = formatBytes(s->pixelFormat());
					size_t w = std::min(s->width(), std::max<size_t>(d->width() >> level, 1));
					size_t h = std::min(s->height(), std::max<size_t>(d->height() >> level, 1));
					s->pullSurface();
					pdmtl_sync(s->offset(), s->totalBytes());
					pdmtl_sync(d->offset(), d->totalBytes());
					for (size_t y = 0; y < h; y++)
						memcpy(d->levelBytes(level, slice) + y * d->levelBytesPerRow(level), s->levelBytes(0, 0) + y * s->levelBytesPerRow(0), w * bpp);
					pdmtl_dirty(d->offset(), d->totalBytes());
				});
			}
		}

		// metal's deferred store action: the one given at the pass's end replaces the descriptor's
		void setStoreAction(int attachment, Indium::StoreAction action) {
			if (attachment >= 0 && (size_t)attachment < _pass.colorAttachments.size())
				_pass.colorAttachments[attachment].storeAction = action;
			else if (attachment == storeDepth && _pass.depthAttachment)
				_pass.depthAttachment->storeAction = action;
			else if (attachment == storeStencil && _pass.stencilAttachment)
				_pass.stencilAttachment->storeAction = action;
		}

		void setRenderPipelineState(std::shared_ptr<Indium::RenderPipelineState> s) override {
			_pipeline = std::dynamic_pointer_cast<RenderPipelineState>(s);
		}
		void setFrontFacingWinding(Indium::Winding w) override { _ccw = w == Indium::Winding::CounterClockwise; }
		void setCullMode(Indium::CullMode m) override { _cull = (uint32_t)m & 3; }
		void setDepthBias(float bias, float slope, float clamp) override { _bias[0] = bias, _bias[1] = slope, _bias[2] = clamp; }
		void setDepthClipMode(Indium::DepthClipMode m) override { _clamp = m == Indium::DepthClipMode::Clamp; }
		void setViewport(const Indium::Viewport &v) override { _viewport = v; }
		void setViewports(const Indium::Viewport *v, size_t count) override {
			if (count)
				_viewport = v[0];
		}
		void setViewports(const std::vector<Indium::Viewport> &v) override {
			if (!v.empty())
				_viewport = v[0];
		}
		void setScissorRect(const Indium::ScissorRect &r) override {
			_scissor[0] = (uint32_t)r.x, _scissor[1] = (uint32_t)r.y, _scissor[2] = (uint32_t)r.width, _scissor[3] = (uint32_t)r.height;
			_scissorOn = true;
		}
		void setScissorRects(const Indium::ScissorRect *r, size_t count) override {
			if (count)
				setScissorRect(r[0]);
		}
		void setScissorRects(const std::vector<Indium::ScissorRect> &r) override {
			if (!r.empty())
				setScissorRect(r[0]);
		}
		void setBlendColor(float r, float g, float b, float a) override { _blendColor[0] = r, _blendColor[1] = g, _blendColor[2] = b, _blendColor[3] = a; }
		void setDepthStencilState(std::shared_ptr<Indium::DepthStencilState> s) override { _ds = std::dynamic_pointer_cast<DepthStencilState>(s); }
		void setTriangleFillMode(Indium::TriangleFillMode m) override { _lines = m == Indium::TriangleFillMode::Lines; }
		void setStencilReferenceValue(uint32_t v) override { _sref[0] = _sref[1] = v; }
		void setStencilReferenceValue(uint32_t front, uint32_t back) override { _sref[0] = front, _sref[1] = back; }
		void setVisibilityResultMode(Indium::VisibilityResultMode, size_t) override {}

		void drawPrimitives(Indium::PrimitiveType type, size_t start, size_t count, size_t instances, size_t baseInstance) override {
			draw(type, count, instances, 0, false, 2, (uint32_t)start, 0, (uint32_t)baseInstance);
		}
		void drawPrimitives(Indium::PrimitiveType type, size_t start, size_t count, size_t instances) override {
			drawPrimitives(type, start, count, instances, 0);
		}
		void drawPrimitives(Indium::PrimitiveType type, size_t start, size_t count) override {
			drawPrimitives(type, start, count, 1, 0);
		}

		void drawIndexedPrimitives(Indium::PrimitiveType type, size_t count, Indium::IndexType indexType, std::shared_ptr<Indium::Buffer> indexBuffer,
		                           size_t offset, size_t instances, int64_t baseVertex, size_t baseInstance) override {
			auto b = asBuffer(indexBuffer);

			if (!b)
				return;
			draw(type, count, instances, b->offset() + (uint32_t)offset, true, indexType == Indium::IndexType::UInt32 ? 4 : 2, 0,
			     (int32_t)baseVertex, (uint32_t)baseInstance, nullptr, 0, b);
		}
		void drawIndexedPrimitives(Indium::PrimitiveType type, size_t count, Indium::IndexType indexType, std::shared_ptr<Indium::Buffer> indexBuffer,
		                           size_t offset, size_t instances) override {
			drawIndexedPrimitives(type, count, indexType, indexBuffer, offset, instances, 0, 0);
		}
		void drawIndexedPrimitives(Indium::PrimitiveType type, size_t count, Indium::IndexType indexType, std::shared_ptr<Indium::Buffer> indexBuffer,
		                           size_t offset) override {
			drawIndexedPrimitives(type, count, indexType, indexBuffer, offset, 1, 0, 0);
		}

		void setVertexBytes(const void *bytes, size_t length, size_t index) override { bindBytes(_vbuf, PD_METAL_BUFS, bytes, length, index); }
		void setVertexBuffer(std::shared_ptr<Indium::Buffer> buffer, size_t offset, size_t index) override {
			bindBuffer(_vbuf, PD_METAL_BUFS, buffer, offset, index);
		}
		void setVertexBuffers(const std::vector<std::shared_ptr<Indium::Buffer>> &b, const std::vector<size_t> &o, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				bindBuffer(_vbuf, PD_METAL_BUFS, b[i], o[i], r.start + i);
			}
		}
		void setVertexBufferOffset(size_t offset, size_t index) override { setOffset(_vbuf, PD_METAL_BUFS, offset, index); }
		void setVertexSamplerState(std::shared_ptr<Indium::SamplerState> s, size_t index) override {
			auto p = std::dynamic_pointer_cast<SamplerState>(s);
			if (index < 16)
				_vsamp[index] = p ? p->flags() : 0;
		}
		void setVertexSamplerState(std::shared_ptr<Indium::SamplerState> s, float, float, size_t index) override { setVertexSamplerState(s, index); }
		void setVertexSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setVertexSamplerState(s[i], r.start + i);
			}
		}
		void setVertexSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, const std::vector<float> &, const std::vector<float> &,
		                            Indium::Range<size_t> r) override {
			setVertexSamplerStates(s, r);
		}
		void setVertexTexture(std::shared_ptr<Indium::Texture> t, size_t index) override {
			if (index < PD_DRAW_TEXS)
				_vtex[index] = asTexture(t);
		}
		void setVertexTextures(const std::vector<std::shared_ptr<Indium::Texture>> &t, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setVertexTexture(t[i], r.start + i);
			}
		}

		void setFragmentBytes(const void *bytes, size_t length, size_t index) override { bindBytes(_fbuf, PD_METAL_BUFS, bytes, length, index); }
		void setFragmentBuffer(std::shared_ptr<Indium::Buffer> buffer, size_t offset, size_t index) override {
			bindBuffer(_fbuf, PD_METAL_BUFS, buffer, offset, index);
		}
		void setFragmentBuffers(const std::vector<std::shared_ptr<Indium::Buffer>> &b, const std::vector<size_t> &o, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				bindBuffer(_fbuf, PD_METAL_BUFS, b[i], o[i], r.start + i);
			}
		}
		void setFragmentBufferOffset(size_t offset, size_t index) override { setOffset(_fbuf, PD_METAL_BUFS, offset, index); }
		void setFragmentSamplerState(std::shared_ptr<Indium::SamplerState> s, size_t index) override {
			auto p = std::dynamic_pointer_cast<SamplerState>(s);
			if (index < 16)
				_samp[index] = p ? p->flags() : 0;
		}
		void setFragmentSamplerState(std::shared_ptr<Indium::SamplerState> s, float, float, size_t index) override { setFragmentSamplerState(s, index); }
		void setFragmentSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setFragmentSamplerState(s[i], r.start + i);
			}
		}
		void setFragmentSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, const std::vector<float> &, const std::vector<float> &,
		                              Indium::Range<size_t> r) override {
			setFragmentSamplerStates(s, r);
		}
		void setFragmentTexture(std::shared_ptr<Indium::Texture> t, size_t index) override {
			if (index < PD_DRAW_TEXS)
				_ftex[index] = asTexture(t);
		}
		void setFragmentTextures(std::vector<std::shared_ptr<Indium::Texture>> &t, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setFragmentTexture(t[i], r.start + i);
			}
		}
		void useResource(std::shared_ptr<Indium::Resource>, Indium::ResourceUsage, Indium::RenderStages) override {}
		void useResources(const std::vector<std::shared_ptr<Indium::Resource>> &, Indium::ResourceUsage, Indium::RenderStages) override {}
		void useResource(std::shared_ptr<Indium::Resource>, Indium::ResourceUsage) override {}
		void useResources(const std::vector<std::shared_ptr<Indium::Resource>> &, Indium::ResourceUsage) override {}

	private:
		// LoadAction::Clear, before the pass's first draw
		static void clear(std::shared_ptr<Texture> t, Indium::ClearColor c) {
			struct PDDrawTex d;
			float v[4] = { (float)c.red, (float)c.green, (float)c.blue, (float)c.alpha };

			if (t->drawable()) {
				pdmtl_clear(t->drawable(), v[0], v[1], v[2], v[3]);
				return;
			}
			if (!t->describe(&d))
				return;
			pdmtl_fill(&d, v);
		}

	public:
		// arguments read from args at argOff when the draw runs, the index buffer as for drawIndexedPrimitives
		void drawIndirect(Indium::PrimitiveType type, bool indexed, Indium::IndexType indexType, std::shared_ptr<Indium::Buffer> indexBuffer,
		                  size_t indexOffset, std::shared_ptr<Indium::Buffer> args, size_t argOff) {
			auto a = asBuffer(args), b = indexed ? asBuffer(indexBuffer) : nullptr;

			if (!a || (indexed && !b))
				return;
			draw(type, 1, 1, indexed ? b->offset() + (uint32_t)indexOffset : 0, indexed,
			     indexType == Indium::IndexType::UInt32 ? 4 : 2, 0, 0, 0, a, a->offset() + (uint32_t)argOff, b);
		}

	private:
		void draw(Indium::PrimitiveType type, size_t count, size_t instances, uint32_t indexOff, bool indexed, uint32_t indexBytes,
		          uint32_t firstVertex, int32_t baseVertex, uint32_t baseInstance, std::shared_ptr<Buffer> args = nullptr,
		          uint32_t argOff = 0, std::shared_ptr<Buffer> indices = nullptr) {
			struct PDDrawRequest r;
			auto cb = _cb.lock();

			if (!_pipeline || !cb || !count)
				return;
			auto &desc = _pipeline->descriptor();
			auto vf = std::dynamic_pointer_cast<Function>(desc.vertexFunction), ff = std::dynamic_pointer_cast<Function>(desc.fragmentFunction);
			if (!vf || !ff)
				return;

			memset(&r, 0, sizeof(r));
			pdmtl_fn_ref(vf->fn(), &r.vfn, &r.vmOff, &r.vmSize);
			pdmtl_fn_ref(ff->fn(), &r.ffn, &r.fmOff, &r.fmSize);
			r.prim = (uint32_t)type, r.count = (uint32_t)count, r.instances = (uint32_t)instances;
			r.indexed = indexed, r.indexOff = indexOff, r.indexBytes = indexBytes;
			fillBuffers(r.vbuf, _vbuf, PD_DRAW_BUFS);
			fillBuffers(r.fbuf, _fbuf, PD_DRAW_BUFS);
			for (size_t i = 0; i < PD_DRAW_TEXS; i++) {
				if (_ftex[i] && _ftex[i]->describe(&r.ftex[i]))
					_ftex[i]->layout(&r.ext.ftexLayout[i]);
				if (_vtex[i] && _vtex[i]->describe(&r.ext.vtex[i]))
					_vtex[i]->layout(&r.ext.vtexLayout[i]);
			}
			memcpy(r.samp, _samp, sizeof(r.samp));
			memcpy(r.ext.vsamp, _vsamp, sizeof(r.ext.vsamp));

			// what Metal keeps on the encoder and the pipeline, in the request's extension
			{
				struct PDDrawExt *x = &r.ext;

				x->magic = PD_EXT_MAGIC, x->size = sizeof(*x);
				if (_depth)
					_depth->describe(&x->depth);
				x->depthState = _ds ? _ds->depthWord() : 0;
				x->stencil[0] = _ds ? _ds->stencilWord(false) : 0, x->stencil[1] = _ds ? _ds->stencilWord(true) : 0;
				x->stencilRef = (_sref[0] & 0xff) | (_sref[1] & 0xff) << 8;
				x->raster = PD_RASTER_SET | _cull | (_ccw ? PD_RASTER_CCW : 0) | (_lines ? PD_RASTER_LINES : 0) | (_clamp ? PD_RASTER_CLAMP : 0) |
				            (_scissorOn ? PD_RASTER_SCISSOR : 0);
				memcpy(x->scissor, _scissor, sizeof(x->scissor));
				memcpy(x->blendColor, _blendColor, sizeof(x->blendColor));
				x->depthBias = _bias[0], x->depthSlope = _bias[1], x->depthBiasClamp = _bias[2];
				x->firstVertex = firstVertex, x->baseInstance = baseInstance, x->baseVertex = baseVertex;
				if (desc.vertexDescriptor) {
					for (auto &a: desc.vertexDescriptor->attributes) {
						if (a.first >= PD_VERTEX_ATTRS)
							continue;
						x->attr[a.first].format = (uint8_t)a.second.format, x->attr[a.first].buffer = (uint8_t)a.second.bufferIndex;
						x->attr[a.first].offset = (uint16_t)a.second.offset;
					}
					for (auto &l: desc.vertexDescriptor->layouts) {
						if (l.first >= PD_VERTEX_ATTRS)
							continue;
						x->layout[l.first].stride = (uint32_t)l.second.stride, x->layout[l.first].step = (uint8_t)l.second.stepFunction;
						x->layout[l.first].rate = (uint32_t)l.second.stepRate;
					}
					remapVertexBuffers(r, x);
				}
			}

			// colour attachment 0 is the target, 1-3 ride in rt[]
			auto &c = _pass.colorAttachments;
			for (size_t i = 0; i < c.size() && i < 4; i++) {
				struct PDDrawTex d;
				auto t = asTexture(c[i].texture);
				if (!t || !t->describe(&d))
					continue;
				if (t->surfaceLink())
					if (auto cb = _cb.lock())
						cb->noteSurfaceWrite(t);
				if (i == 0) {
					r.targetOff = d.off, r.tw = d.w, r.th = d.h, r.tbpr = d.bpr, r.tfmt = d.fmt;
					r.blend = _pipeline->blend(0);
				} else {
					r.rt[i] = d;
					r.rtBlend[i] = _pipeline->blend(i);
				}
			}
			// a depth-only pass: the depth texture sizes the draw and nothing takes colour
			if (!r.tw && r.ext.depth.w) {
				r.targetOff = r.ext.depth.off, r.tw = r.ext.depth.w, r.th = r.ext.depth.h;
				r.tbpr = r.ext.depth.bpr, r.tfmt = r.ext.depth.fmt;
				r.ext.raster |= PD_RASTER_NOCOLOR;
			}
			if (!r.tw) {
				auto t0 = c.empty() ? nullptr : c[0].texture;
				auto p0 = asTexture(t0);
				fprintf(stderr, "indium-pd: colour attachment 0: %s, type %d, format %d, %zux%zu, %zu samples\n",
				        !t0 ? "none" : !p0 ? "not ours" : "ours", t0 ? (int)t0->textureType() : -1, t0 ? (int)t0->pixelFormat() : -1,
				        t0 ? t0->width() : 0, t0 ? t0->height() : 0, t0 ? t0->sampleCount() : 0);
				return report("draw without a usable colour attachment 0", -1);
			}
			r.viewport[0] = _viewport.originX, r.viewport[1] = _viewport.originY;
			r.viewport[2] = _viewport.width, r.viewport[3] = _viewport.height;
			r.viewport[4] = _viewport.znear, r.viewport[5] = _viewport.zfar;
			// the index and argument buffers live until the draw has run, as a command buffer keeps what it uses
			if (args)
				cb->add([r, args, argOff, indices]() mutable {
					if (indirectDraw(r, argOff))
						runDraw(r);
				});
			else
				cb->add([r, indices]() mutable { runDraw(r); });
		}

	public:
		// tile shading (pd-tile.hpp): its own bindings, run in order with the draws
		void setTileFunction(std::shared_ptr<Function> f) { _tileFn = f; }
		void setTileBuffer(std::shared_ptr<Indium::Buffer> b, size_t offset, size_t index) { bindBuffer(_tbuf, PD_CS_BUFS, b, offset, index); }
		void setTileBytes(const void *bytes, size_t length, size_t index) { bindBytes(_tbuf, PD_CS_BUFS, bytes, length, index); }
		void setTileTexture(std::shared_ptr<Indium::Texture> t, size_t index) {
			if (index < PD_DRAW_TEXS)
				_ttex[index] = asTexture(t);
		}
		void setTileSamplerState(std::shared_ptr<Indium::SamplerState> s, size_t index) {
			auto p = std::dynamic_pointer_cast<SamplerState>(s);
			if (index < 16)
				_tsamp[index] = p ? p->flags() : 0;
		}
		void setTileThreadgroupMemoryLength(size_t length, size_t index) {
			if (index < 4)
				_ttgmem[index] = (uint32_t)length;
		}
		void dispatchThreadsPerTile(Indium::Size tpt) {
			struct PDTileRequest r;
			auto cb = _cb.lock();
			auto &c = _pass.colorAttachments;

			if (!_tileFn || !cb)
				return;
			memset(&r, 0, sizeof(r));
			pdmtl_fn_ref(_tileFn->fn(), &r.fn, &r.mOff, &r.mSize);
			r.tpt[0] = (uint32_t)tpt.width, r.tpt[1] = (uint32_t)tpt.height, r.tpt[2] = (uint32_t)(tpt.depth ? tpt.depth : 1);
			memcpy(r.tgmem, _ttgmem, sizeof(r.tgmem));
			fillBuffers(r.buf, _tbuf, PD_CS_BUFS);
			r.ext.magic = PD_EXT_MAGIC, r.ext.size = sizeof(r.ext);
			for (size_t i = 0; i < PD_DRAW_TEXS; i++) {
				if (_ttex[i] && _ttex[i]->describe(&r.tex[i]))
					_ttex[i]->layout(&r.ext.texLayout[i]);
			}
			memcpy(r.samp, _tsamp, sizeof(r.samp));
			// the implicit imageblock: the pass's colour attachments, tiled from attachment 0's size
			for (size_t i = 0; i < c.size() && i < 4; i++) {
				auto t = asTexture(c[i].texture);
				if (t && t->describe(&r.rt[i]) && i == 0) {
					r.tilesX = (uint32_t)((t->width() + PD_TILE_SIZE - 1) / PD_TILE_SIZE);
					r.tilesY = (uint32_t)((t->height() + PD_TILE_SIZE - 1) / PD_TILE_SIZE);
				}
			}
			cb->add([r]() mutable { runTile(r); });
		}

	private:
		std::shared_ptr<Function> _tileFn;
		Binding _tbuf[PD_CS_BUFS];
		std::shared_ptr<Texture> _ttex[PD_DRAW_TEXS];
		uint32_t _tsamp[16] = {}, _ttgmem[4] = {};

		std::weak_ptr<CommandBuffer> _cb;
		Indium::RenderPassDescriptor _pass;
		std::shared_ptr<RenderPipelineState> _pipeline;
		Indium::Viewport _viewport {};
		// vertex buffers only the vertex descriptor reads (stage_in) may sit above the request's slots (sokol puts
		// them at 23 up): each moves to a free low slot, with its attributes and layout
		void remapVertexBuffers(struct PDDrawRequest &r, struct PDDrawExt *x) {
			for (unsigned b = PD_DRAW_BUFS; b < PD_METAL_BUFS && b < PD_VERTEX_ATTRS; b++) {
				bool used = false;
				for (unsigned a = 0; a < PD_VERTEX_ATTRS; a++)
					used |= x->attr[a].format && x->attr[a].buffer == b;
				if (!used || !_vbuf[b].size)
					continue;
				unsigned s = 0;
				while (s < PD_DRAW_BUFS && (r.vbuf[s].size || x->layout[s].stride))
					s++;
				if (s == PD_DRAW_BUFS)
					return;
				r.vbuf[s].id = s + 1, r.vbuf[s].off = _vbuf[b].off, r.vbuf[s].size = _vbuf[b].size;
				x->layout[s] = x->layout[b];
				memset(&x->layout[b], 0, sizeof(x->layout[b]));
				for (unsigned a = 0; a < PD_VERTEX_ATTRS; a++) {
					if (x->attr[a].format && x->attr[a].buffer == b)
						x->attr[a].buffer = (uint8_t)s;
				}
			}
		}

		Binding _vbuf[PD_METAL_BUFS], _fbuf[PD_METAL_BUFS];
		std::shared_ptr<Texture> _ftex[PD_DRAW_TEXS], _vtex[PD_DRAW_TEXS], _depth;
		uint32_t _samp[16] = {}, _vsamp[16] = {};
		std::shared_ptr<DepthStencilState> _ds;
		uint32_t _cull = 0, _scissor[4] = {}, _sref[2] = {};
		bool _ccw = false, _clamp = false, _lines = false, _scissorOn = false;
		float _blendColor[4] = {}, _bias[3] = {};
	};

	class ComputeEncoder: public Indium::ComputeCommandEncoder {
	public:
		ComputeEncoder(std::shared_ptr<CommandBuffer> cb, Indium::DispatchType type): _cb(cb), _type(type) {}

		void endEncoding() override {}
		void setComputePipelineState(std::shared_ptr<Indium::ComputePipelineState> s) override {
			_pipeline = std::dynamic_pointer_cast<ComputePipelineState>(s);
		}
		void setBuffer(std::shared_ptr<Indium::Buffer> buffer, size_t offset, size_t index) override { bindBuffer(_buf, PD_CS_BUFS, buffer, offset, index); }
		void setBuffers(const std::vector<std::shared_ptr<Indium::Buffer>> &b, const std::vector<size_t> &o, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				bindBuffer(_buf, PD_CS_BUFS, b[i], o[i], r.start + i);
			}
		}
		void setBufferOffset(size_t offset, size_t index) override { setOffset(_buf, PD_CS_BUFS, offset, index); }
		void setBytes(const void *bytes, size_t length, size_t index) override { bindBytes(_buf, PD_CS_BUFS, bytes, length, index); }
		void setSamplerState(std::shared_ptr<Indium::SamplerState> s, size_t index) override {
			auto p = std::dynamic_pointer_cast<SamplerState>(s);
			if (index < 16)
				_samp[index] = p ? p->flags() : 0;
		}
		void setSamplerState(std::shared_ptr<Indium::SamplerState> s, float, float, size_t index) override { setSamplerState(s, index); }
		void setSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setSamplerState(s[i], r.start + i);
			}
		}
		void setSamplerStates(const std::vector<std::shared_ptr<Indium::SamplerState>> &s, const std::vector<float> &, const std::vector<float> &,
		                      Indium::Range<size_t> r) override {
			setSamplerStates(s, r);
		}
		void setTexture(std::shared_ptr<Indium::Texture> t, size_t index) override {
			if (index < PD_DRAW_TEXS)
				_tex[index] = asTexture(t);
		}
		void setTextures(const std::vector<std::shared_ptr<Indium::Texture>> &t, Indium::Range<size_t> r) override {
			for (size_t i = 0; i < r.length; i++) {
				setTexture(t[i], r.start + i);
			}
		}
		void setThreadgroupMemoryLength(size_t length, size_t index) override {
			if (index < 4)
				_tgmem[index] = (uint32_t)length;
		}
		void dispatchThreadgroups(Indium::Size groups, Indium::Size tpg) override { dispatch(groups, tpg, false); }
		// threadgroupsPerGrid read from args at argOff when the dispatch runs
		void dispatchIndirect(std::shared_ptr<Indium::Buffer> args, size_t argOff, Indium::Size tpg) {
			if (auto a = asBuffer(args))
				dispatch({ 1, 1, 1 }, tpg, false, a, a->offset() + (uint32_t)argOff);
		}
		void dispatchThreads(Indium::Size threads, Indium::Size tpg) override { dispatch(threads, tpg, true); }
		void setImageblockSize(size_t width, size_t height) override { _ib[0] = (uint32_t)width, _ib[1] = (uint32_t)height; }
		void setStageInRegion(Indium::Region) override {}
		Indium::DispatchType dispatchType() override { return _type; }

	private:
		void dispatch(Indium::Size grid, Indium::Size tpg, bool threads, std::shared_ptr<Buffer> args = nullptr, uint32_t argOff = 0) {
			struct PDComputeRequest r;
			auto cb = _cb.lock();

			if (!_pipeline || !cb)
				return;
			memset(&r, 0, sizeof(r));
			pdmtl_fn_ref(_pipeline->function()->fn(), &r.fn, &r.mOff, &r.mSize);
			r.threads = threads;
			r.grid[0] = grid.width, r.grid[1] = grid.height, r.grid[2] = grid.depth;
			r.tpg[0] = tpg.width, r.tpg[1] = tpg.height, r.tpg[2] = tpg.depth;
			memcpy(r.tgmem, _tgmem, sizeof(r.tgmem));
			r.imageblock[0] = _ib[0], r.imageblock[1] = _ib[1];
			fillBuffers(r.buf, _buf, PD_CS_BUFS);
			r.ext.magic = PD_EXT_MAGIC, r.ext.size = sizeof(r.ext);
			for (size_t i = 0; i < PD_DRAW_TEXS; i++) {
				if (_tex[i] && _tex[i]->describe(&r.tex[i]))
					_tex[i]->layout(&r.ext.texLayout[i]);
			}
			memcpy(r.samp, _samp, sizeof(r.samp));
			if (args)
				cb->add([r, args, argOff]() mutable {
					if (indirectDispatch(r, argOff))
						runCompute(r);
				});
			else
				cb->add([r]() mutable { runCompute(r); });
		}

		std::weak_ptr<CommandBuffer> _cb;
		Indium::DispatchType _type;
		std::shared_ptr<ComputePipelineState> _pipeline;
		Binding _buf[PD_CS_BUFS];
		std::shared_ptr<Texture> _tex[PD_DRAW_TEXS];
		uint32_t _samp[16] = {}, _tgmem[4] = {}, _ib[2] = {};
	};

	// blits are byte copies in the arena, done in order with the other jobs
	class BlitEncoder: public Indium::BlitCommandEncoder {
	public:
		BlitEncoder(std::shared_ptr<CommandBuffer> cb): _cb(cb) {}

		void endEncoding() override {}
		void copy(std::shared_ptr<Indium::Buffer> src, size_t srcOff, std::shared_ptr<Indium::Buffer> dst, size_t dstOff, size_t size) override {
			auto s = asBuffer(src), d = asBuffer(dst);
			add([=] {
				// blits run on the CPU against the arena: what the GPU wrote comes back first
				pdmtl_sync(s->offset() + (uint32_t)srcOff, (uint32_t)size);
				pdmtl_sync(d->offset() + (uint32_t)dstOff, (uint32_t)size);
				memmove((uint8_t *)d->contents() + dstOff, (uint8_t *)s->contents() + srcOff, size);
			});
		}
		void copy(std::shared_ptr<Indium::Buffer> src, size_t srcOff, size_t srcBpr, size_t srcBpi, Indium::Size size,
		          std::shared_ptr<Indium::Texture> dst, size_t slice, size_t level, Indium::Origin origin, Indium::BlitOption) override {
			auto s = asBuffer(src);
			auto d = asTexture(dst);
			add([=] {
				d->replaceRegion(Indium::Region { origin, size }, level, slice, (uint8_t *)s->contents() + srcOff, srcBpr, srcBpi);
			});
		}
		void copy(std::shared_ptr<Indium::Texture> src, size_t slice, size_t level, Indium::Origin origin, Indium::Size size, std::shared_ptr<Indium::Buffer> dst,
		          size_t dstOff, size_t dstBpr, size_t dstBpi, Indium::BlitOption options) override {
			auto s = asTexture(src);
			auto d = asBuffer(dst);
			auto f = s->pixelFormat();
			bool combined = f == Indium::PixelFormat::Depth32Float_Stencil8 || f == Indium::PixelFormat::Depth24Unorm_Stencil8;

			// one plane of a combined depth/stencil texture: depth as float32, stencil as a byte
			if (combined && options != Indium::BlitOption::None) {
				bool depthPlane = (options & Indium::BlitOption::DepthFromDepthStencil) == Indium::BlitOption::DepthFromDepthStencil;
				add([=] {
					size_t bpp = formatBytes(f);
					if (level >= s->levels())
						return;
					s->pullSurface();
					pdmtl_sync(s->offset(), s->totalBytes());
					pdmtl_sync(d->offset(), (uint32_t)d->length());
					for (size_t y = 0; y < size.height; y++) {
						const uint8_t *row = s->levelBytes(level, slice) + (origin.y + y) * s->levelBytesPerRow(level) + origin.x * bpp;
						uint8_t *out = (uint8_t *)d->contents() + dstOff + y * dstBpr;

						for (size_t x = 0; x < size.width; x++) {
							const uint8_t *p = row + x * bpp;
							uint32_t w;

							if (!depthPlane) {
								out[x] = f == Indium::PixelFormat::Depth32Float_Stencil8 ? p[4] : p[3];
							} else if (f == Indium::PixelFormat::Depth32Float_Stencil8) {
								memcpy(out + x * 4, p, 4);
							} else {
								float v;
								memcpy(&w, p, 4);
								v = (w & 0xffffff) / 16777215.0f;
								memcpy(out + x * 4, &v, 4);
							}
						}
					}
				});
				return;
			}
			add([=] {
				// in blocks for a compressed format: the buffer's rows are then rows of blocks
				auto pf = s->pixelFormat();
				size_t bpp = formatBytes(pf), blk = formatBlock(pf), depth = size.depth ? size.depth : 1;
				size_t rows = formatRows(pf, size.height), row = formatRowBytes(pf, size.width);
				size_t image = dstBpi ? dstBpi : dstBpr * rows;
				bool is3d = s->textureType() == Indium::TextureType::e3D;
				if (level >= s->levels())
					return;
					s->pullSurface();
					pdmtl_sync(s->offset(), s->totalBytes());
				pdmtl_sync(d->offset(), (uint32_t)d->length());
				// a 3D region's depth slices are the texture's slices, bytesPerImage apart in the buffer
				for (size_t z = 0; z < depth; z++) {
					const uint8_t *from = s->levelBytes(level, is3d ? origin.z + z : slice);

					for (size_t y = 0; y < rows; y++) {
						memcpy((uint8_t *)d->contents() + dstOff + z * image + y * dstBpr,
						       from + (origin.y / blk + y) * s->levelBytesPerRow(level) + origin.x / blk * bpp, row);
					}
				}
			});
		}
		void copy(std::shared_ptr<Indium::Texture> src, std::shared_ptr<Indium::Texture> dst) override {
			auto s = asTexture(src), d = asTexture(dst);
			add([=] {
					s->pullSurface();
					pdmtl_sync(s->offset(), s->totalBytes());
				pdmtl_sync(d->offset(), d->totalBytes());
				memcpy(d->bytes(), s->bytes(), std::min(s->totalBytes(), d->totalBytes()));
				pdmtl_dirty(d->offset(), d->totalBytes());
			});
		}
		// whole levels and slices: each one a region the size of the source level
		void copy(std::shared_ptr<Indium::Texture> src, size_t sslice, size_t slevel, std::shared_ptr<Indium::Texture> dst, size_t dslice,
		          size_t dlevel, size_t sliceCount, size_t levelCount) override {
			for (size_t k = 0; k < levelCount; k++) {
				size_t w = std::max<size_t>(src->width() >> (slevel + k), 1), h = std::max<size_t>(src->height() >> (slevel + k), 1);
				for (size_t i = 0; i < sliceCount; i++)
					copy(src, sslice + i, slevel + k, Indium::Origin { 0, 0, 0 }, Indium::Size { w, h, 1 }, dst, dslice + i, dlevel + k,
					     Indium::Origin { 0, 0, 0 });
			}
		}
		void copy(std::shared_ptr<Indium::Texture> src, size_t sslice, size_t slevel, Indium::Origin so, Indium::Size size, std::shared_ptr<Indium::Texture> dst,
		          size_t dslice, size_t dlevel, Indium::Origin dorg) override {
			auto s = asTexture(src), d = asTexture(dst);
			add([=] {
				// in blocks for a compressed format
				auto pf = s->pixelFormat();
				size_t bpp = formatBytes(pf), blk = formatBlock(pf), rows = formatRows(pf, size.height), row = formatRowBytes(pf, size.width);
				if (slevel >= s->levels() || dlevel >= d->levels())
					return;
					s->pullSurface();
					pdmtl_sync(s->offset(), s->totalBytes());
				pdmtl_sync(d->offset(), d->totalBytes());
				for (size_t y = 0; y < rows; y++) {
					memcpy(d->levelBytes(dlevel, dslice) + (dorg.y / blk + y) * d->levelBytesPerRow(dlevel) + dorg.x / blk * bpp,
					       s->levelBytes(slevel, sslice) + (so.y / blk + y) * s->levelBytesPerRow(slevel) + so.x / blk * bpp, row);
				}
				pdmtl_dirty(d->offset(), d->totalBytes());
			});
		}
		void fillBuffer(std::shared_ptr<Indium::Buffer> buffer, Indium::Range<size_t> range, uint8_t value) override {
			auto b = asBuffer(buffer);
			add([=] {
				pdmtl_sync(b->offset() + (uint32_t)range.start, (uint32_t)range.length);
				memset((uint8_t *)b->contents() + range.start, value, range.length);
			});
		}
		void generateMipmapsForTexture(std::shared_ptr<Indium::Texture> texture) override {
			auto t = asTexture(texture);
			struct PDDrawTex d;
			struct PDTexLayout l;

			if (!t || !t->describe(&d))
				return;
			t->layout(&l);
			add([d, l] { pdmtl_generate_mipmaps(&d, &l); });
		}

	private:
		void add(Job job) {
			auto cb = _cb.lock();
			if (cb)
				cb->add(std::move(job));
		}

		std::weak_ptr<CommandBuffer> _cb;
	};

	std::shared_ptr<Indium::CommandBuffer> CommandQueue::commandBuffer() {
		return std::make_shared<CommandBuffer>(_device.lock(), shared_from_this());
	}

	std::shared_ptr<Indium::RenderCommandEncoder> CommandBuffer::renderCommandEncoder(const Indium::RenderPassDescriptor &descriptor) {
		return std::make_shared<RenderEncoder>(shared_from_this(), descriptor);
	}

	std::shared_ptr<Indium::BlitCommandEncoder> CommandBuffer::blitCommandEncoder() {
		return std::make_shared<BlitEncoder>(shared_from_this());
	}

	std::shared_ptr<Indium::BlitCommandEncoder> CommandBuffer::blitCommandEncoder(const Indium::BlitPassDescriptor &) {
		return blitCommandEncoder();
	}

	std::shared_ptr<Indium::ComputeCommandEncoder> CommandBuffer::computeCommandEncoder() {
		return computeCommandEncoder(Indium::DispatchType::Serial);
	}

	std::shared_ptr<Indium::ComputeCommandEncoder> CommandBuffer::computeCommandEncoder(const Indium::ComputePassDescriptor &descriptor) {
		return computeCommandEncoder(descriptor.dispatchType);
	}

	std::shared_ptr<Indium::ComputeCommandEncoder> CommandBuffer::computeCommandEncoder(Indium::DispatchType type) {
		return std::make_shared<ComputeEncoder>(shared_from_this(), type);
	}

	void CommandBuffer::commit() {
		auto d = _device.lock();

		if (d)
			d->submit(shared_from_this());
	}

	void CommandBuffer::presentDrawable(std::shared_ptr<Indium::Drawable> drawable) {
		_present.push_back(drawable);
	}

	void CommandBuffer::addScheduledHandler(Handler handler) {
		_scheduled.push_back(std::move(handler));
	}

	void CommandBuffer::noteSurfaceWrite(std::shared_ptr<Texture> texture) {
		for (auto &t: _surfaceWrites)
			if (t == texture)
				return;
		_surfaceWrites.push_back(std::move(texture));
	}

	void CommandBuffer::addCompletedHandler(Handler handler) {
		_completed.push_back(std::move(handler));
	}

	void CommandBuffer::waitUntilCompleted() {
		std::unique_lock<std::mutex> lock(_mu);

		_cv.wait(lock, [this] { return _done; });
	}

	void CommandBuffer::execute() {
		auto self = shared_from_this();

		for (auto &h: _scheduled) {
			h(self);
		}
		for (auto &j: _jobs) {
			j();
		}
		for (auto &d: _present) {
			d->present();
		}
		// the batch goes to the GPU, buffers it wrote come back for the CPU
		pdmtl_finish();
		// what it rendered into surfaces goes back to them, before anyone is told the work completed
		for (auto &t: _surfaceWrites)
			if (auto link = t->surfaceLink(); link && link->push)
				link->push(t->bytes(), t->bytesPerRow());
		_surfaceWrites.clear();
		_jobs.clear();
		_present.clear();
		for (auto &h: _completed) {
			h(self);
		}
		{
			std::lock_guard<std::mutex> lock(_mu);
			_done = true;
		}
		_cv.notify_all();
	}
};

namespace IndiumPD {
	bool addStep(std::shared_ptr<Indium::CommandBuffer> commandBuffer, std::function<void()> step) {
		auto cb = std::dynamic_pointer_cast<CommandBuffer>(commandBuffer);

		if (cb)
			cb->add(std::move(step));
		return cb != nullptr;
	}

	void finishSubmitted() {
		pdmtl_finish();
	}

	void setStoreAction(std::shared_ptr<Indium::RenderCommandEncoder> encoder, int attachment, Indium::StoreAction action) {
		if (auto r = dynamic_cast<RenderEncoder *>(encoder.get()))
			r->setStoreAction(attachment, action);
	}

	static RenderEncoder *tileEncoder(const std::shared_ptr<Indium::RenderCommandEncoder> &e) {
		return dynamic_cast<RenderEncoder *>(e.get());
	}

	bool setTileFunction(std::shared_ptr<Indium::RenderCommandEncoder> e, std::shared_ptr<Indium::Function> f) {
		auto r = tileEncoder(e);
		if (r)
			r->setTileFunction(std::dynamic_pointer_cast<Function>(f));
		return r != nullptr;
	}

	void setTileBuffer(std::shared_ptr<Indium::RenderCommandEncoder> e, std::shared_ptr<Indium::Buffer> b, size_t offset, size_t index) {
		if (auto r = tileEncoder(e))
			r->setTileBuffer(b, offset, index);
	}

	void setTileBytes(std::shared_ptr<Indium::RenderCommandEncoder> e, const void *bytes, size_t length, size_t index) {
		if (auto r = tileEncoder(e))
			r->setTileBytes(bytes, length, index);
	}

	void setTileTexture(std::shared_ptr<Indium::RenderCommandEncoder> e, std::shared_ptr<Indium::Texture> t, size_t index) {
		if (auto r = tileEncoder(e))
			r->setTileTexture(t, index);
	}

	void setTileSamplerState(std::shared_ptr<Indium::RenderCommandEncoder> e, std::shared_ptr<Indium::SamplerState> s, size_t index) {
		if (auto r = tileEncoder(e))
			r->setTileSamplerState(s, index);
	}

	void setTileThreadgroupMemoryLength(std::shared_ptr<Indium::RenderCommandEncoder> e, size_t length, size_t index) {
		if (auto r = tileEncoder(e))
			r->setTileThreadgroupMemoryLength(length, index);
	}

	void dispatchThreadsPerTile(std::shared_ptr<Indium::RenderCommandEncoder> e, Indium::Size tpt) {
		if (auto r = tileEncoder(e))
			r->dispatchThreadsPerTile(tpt);
	}

	void drawIndirect(std::shared_ptr<Indium::RenderCommandEncoder> e, Indium::PrimitiveType type, bool indexed, Indium::IndexType indexType,
	                  std::shared_ptr<Indium::Buffer> indexBuffer, size_t indexOffset, std::shared_ptr<Indium::Buffer> args, size_t argOff) {
		if (auto r = tileEncoder(e))
			r->drawIndirect(type, indexed, indexType, indexBuffer, indexOffset, args, argOff);
	}

	void dispatchIndirect(std::shared_ptr<Indium::ComputeCommandEncoder> e, std::shared_ptr<Indium::Buffer> args, size_t argOff, Indium::Size tpg) {
		if (auto c = dynamic_cast<ComputeEncoder *>(e.get()))
			c->dispatchIndirect(args, argOff, tpg);
	}
};
