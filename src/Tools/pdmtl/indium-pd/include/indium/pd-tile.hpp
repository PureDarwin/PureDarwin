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
	// indirect draws and dispatches, which Indium's interface also lacks: their arguments are read from args when the
	// command runs, after the commands before it, so a kernel can write them
	void drawIndirect(std::shared_ptr<Indium::RenderCommandEncoder> encoder, Indium::PrimitiveType type, bool indexed,
	                  Indium::IndexType indexType, std::shared_ptr<Indium::Buffer> indexBuffer, size_t indexOffset,
	                  std::shared_ptr<Indium::Buffer> args, size_t argOffset);
	void dispatchIndirect(std::shared_ptr<Indium::ComputeCommandEncoder> encoder, std::shared_ptr<Indium::Buffer> args, size_t argOffset,
	                      Indium::Size threadsPerThreadgroup);
	// the executor's tile size
	constexpr size_t tileSize = 32;
};
