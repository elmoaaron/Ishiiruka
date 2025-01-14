// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/TextureCacheBase.h"
namespace DX11
{

	// 4-bit format: 8x8 texels / cache line
	// 8-bit format: 8x4 texels / cache line
	// 16-bit format: 4x4 texels / cache line
	// 32-bit format: 4x4 texels / 2 cache lines
	// Compressed format: 8x8 texels / cache line

	// Document EFB encoding formats here with examples of where they are used.

	// Format: 0 - R4
	// Used in The Legend of Zelda: The Wind Waker for character shadows (srcFormat 1,
	// isIntensity 1, scaleByHalf 1).

	// Format: 1 - R8
	// FIXME: Unseen. May or may not be a duplicate of format 8.

	// Format: 2 - A4 R4
	// FIXME: Unseen.

	// Format: 3 - A8 R8
	// FIXME: Unseen.

	// Format: 4 - R5 G6 B5
	// Used in Wind Waker for most render-to-texture effects like heat shimmer and
	// depth-of-field.

	// Format: 5 - 1 R5 G5 B5 or 0 A3 R4 G4 B4
	// Used in Twilight Princess for character shadows.

	// Format: 6 - A8 R8 A8 R8 | G8 B8 G8 B8
	// Used in Twilight Princess for bloom effect.

	// Format: 7 - A8
	// Used in Metroid Prime 2 for the scan visor.

	// Format: 8 - R8
	// Used in Twilight Princess for the map.

	// Format: 9 - G8
	// FIXME: Unseen.

	// Format: A - B8
	// Used in Metroid Prime 2 for the scan visor.

	// Format: B - G8 R8
	// Used in Wind Waker for depth-of-field. Usually used with srcFormat 3 to
	// render depth textures. The bytes are swapped, so games have to correct it
	// in RAM before using it as a texture.

	// Format: C - B8 G8
	// FIXME: Unseen.

	// Maximum number of bytes that can occur in a texture block-row generated by
	// the encoder
	static const UINT MAX_BYTES_PER_BLOCK_ROW = (EFB_WIDTH / 4) * 64;
	// The maximum amount of data that the texture encoder can generate in one call
	static const UINT MAX_BYTES_PER_ENCODE = MAX_BYTES_PER_BLOCK_ROW*(EFB_HEIGHT / 4);

	enum BaseType
	{
		Unorm4 = 0,
		Unorm8
	};

	class TextureEncoder
	{

	public:

		virtual ~TextureEncoder() { }

		virtual void Init() = 0;
		virtual void Shutdown() = 0;
		virtual void Encode(u8* dest_ptr, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
			PEControl::PixelFormat srcFormat, bool bIsIntensityFmt, bool bScaleByHalf, const EFBRectangle& source) = 0;

	};

	class TextureDecoder
	{

	public:

		virtual ~TextureDecoder() { }

		virtual void Init() = 0;
		virtual void Shutdown() = 0;
		virtual bool FormatSupported(u32 srcFmt) = 0;
		virtual bool Decode(const u8* src, u32 srcsize, u32 srcFmt, u32 w, u32 h, u32 levels, D3DTexture2D& dstTexture) = 0;
		virtual bool DecodeRGBAFromTMEM(u8 const * ar_src, u8 const * bg_src, u32 width, u32 height, D3DTexture2D& dstTexture) = 0;
		virtual bool Depalettize(D3DTexture2D& dstTexture, D3DTexture2D& srcTexture, BaseType baseType, u32 width, u32 height) = 0;
		virtual void LoadLut(u32 lutFmt, void* addr, u32 size) = 0;

	};
}
