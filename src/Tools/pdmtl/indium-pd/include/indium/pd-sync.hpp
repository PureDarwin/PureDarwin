// indium-pd's hooks for Metal's synchronisation objects, which Indium's interface has no words for. A command
// buffer runs its work in order on the device's thread, so an event signal or wait is one more step in that order
#pragma once

#include <indium/indium.hpp>

#include <functional>
#include <string>
#include <vector>

namespace IndiumPD {
	// runs step on the device's thread after the work already encoded into commandBuffer, false when the
	// command buffer is not indium-pd's
	bool addStep(std::shared_ptr<Indium::CommandBuffer> commandBuffer, std::function<void()> step);
	// on the device's thread: waits for the GPU work submitted so far, so the CPU sees what it wrote
	void finishSubmitted();

	// a render encoder's store action for a colour attachment (its index), the depth or the stencil one
	constexpr int storeDepth = -1, storeStencil = -2;
	void setStoreAction(std::shared_ptr<Indium::RenderCommandEncoder> encoder, int attachment, Indium::StoreAction action);

	// placement heaps: one arena range, with buffers and textures placed at offsets in it (aliasing each other)
	std::shared_ptr<void> newArenaRange(size_t size);
	std::shared_ptr<Indium::Buffer> newPlacedBuffer(std::shared_ptr<Indium::Device> device, std::shared_ptr<void> range, size_t offset,
	                                                size_t length);
	std::shared_ptr<Indium::Texture> newPlacedTexture(std::shared_ptr<Indium::Device> device, std::shared_ptr<void> range, size_t offset,
	                                                  const Indium::TextureDescriptor &descriptor);
	// a texture over a buffer's bytes, rows have to be the format's tight rows
	std::shared_ptr<Indium::Texture> newBufferTexture(std::shared_ptr<Indium::Buffer> buffer, size_t offset,
	                                                  const Indium::TextureDescriptor &descriptor);
	size_t textureStorageSize(const Indium::TextureDescriptor &descriptor);

	// a texture whose bytes follow memory outside the arena, an IOSurface: pull copies it in before the texture is
	// used and says whether it did, push copies the texture out after a command buffer rendered into it
	struct SurfaceLink {
		std::function<bool(uint8_t *bytes, size_t bytesPerRow)> pull;
		std::function<void(const uint8_t *bytes, size_t bytesPerRow)> push;
	};
	bool linkSurface(std::shared_ptr<Indium::Texture> texture, std::shared_ptr<SurfaceLink> link);
	size_t tightRowBytes(Indium::PixelFormat format, size_t width);

	// function constants: what a function declares (MSL type name) and a library function with values set
	struct FunctionConstant {
		std::string name, type;
		uint32_t index;
		bool required;
	};
	struct ConstantValue {
		uint32_t index;
		std::string name;
		std::vector<uint8_t> bytes;
	};
	std::vector<FunctionConstant> functionConstants(std::shared_ptr<Indium::Function> function);
	std::shared_ptr<Indium::Function> specializeFunction(std::shared_ptr<Indium::Library> library, const std::string &name,
	                                                     const std::vector<ConstantValue> &values);
};
