// indium-pd's tile shading, which Indium's interface has no words for: a render encoder runs a tile function
// over 32x32 tiles of its colour attachments (PDTileRequest), bound like a compute kernel. For the ObjC layer
#pragma once

#include <indium/indium.hpp>

namespace IndiumPD {
	// false when the encoder is not indium-pd's
	bool setTileFunction(std::shared_ptr<Indium::RenderCommandEncoder> encoder, std::shared_ptr<Indium::Function> function);
	void setTileBuffer(std::shared_ptr<Indium::RenderCommandEncoder> encoder, std::shared_ptr<Indium::Buffer> buffer, size_t offset, size_t index);
	void setTileBytes(std::shared_ptr<Indium::RenderCommandEncoder> encoder, const void *bytes, size_t length, size_t index);
	void setTileTexture(std::shared_ptr<Indium::RenderCommandEncoder> encoder, std::shared_ptr<Indium::Texture> texture, size_t index);
	void setTileSamplerState(std::shared_ptr<Indium::RenderCommandEncoder> encoder, std::shared_ptr<Indium::SamplerState> sampler, size_t index);
	void setTileThreadgroupMemoryLength(std::shared_ptr<Indium::RenderCommandEncoder> encoder, size_t length, size_t index);
	void dispatchThreadsPerTile(std::shared_ptr<Indium::RenderCommandEncoder> encoder, Indium::Size threadsPerTile);
	// the executor's tile size
	constexpr size_t tileSize = 32;
};
