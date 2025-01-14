// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <atomic>
#include <unordered_map>

#include "Common/Common.h"
#include "Common/LinearDiskCache.h"

#include "VideoBackends/DX9/D3DBase.h"

#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX9
{

typedef u32 tevhash;

tevhash GetCurrentTEV();

class PixelShaderCache
{
private:
	struct PSCacheEntry
	{
		LPDIRECT3DPIXELSHADER9 shader;
		std::string code;
		bool compiled;
		std::atomic_flag initialized;
		PSCacheEntry() : shader(NULL), compiled(false)
		{
			initialized.clear();
		}
		void Destroy()
		{
			if (shader)
				shader->Release();
			shader = NULL;
		}
	};

	typedef std::unordered_map<PixelShaderUid, PSCacheEntry, PixelShaderUid::ShaderUidHasher> PSCache;
	static inline void PushByteCode(const PixelShaderUid &uid, const u8 *bytecode, int bytecodelen, PSCacheEntry* entry);
	static PSCache PixelShaders;
	static const PSCacheEntry *last_entry[PSRM_DEPTH_ONLY + 1];
	static PixelShaderUid last_uid[PSRM_DEPTH_ONLY + 1];
	static PixelShaderUid external_last_uid[PSRM_DEPTH_ONLY + 1];

	static void Clear();

public:
	static void Init();
	static void Shutdown();
	static void PrepareShader(
		PIXEL_SHADER_RENDER_MODE render_mode,
		u32 componets,
		const XFMemory &xfr,
		const BPMemory &bpm, 
		bool ongputhread);
	static bool SetShader(PIXEL_SHADER_RENDER_MODE render_mode);
	static void InsertByteCode(const PixelShaderUid &uid, const u8 *bytecode, int bytecodelen);
	static LPDIRECT3DPIXELSHADER9 GetColorMatrixProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetColorCopyProgram(int SSAAMode);
	static LPDIRECT3DPIXELSHADER9 GetDepthMatrixProgram(int SSAAMode, bool depthConversion);
	static LPDIRECT3DPIXELSHADER9 GetClearProgram();	
	static LPDIRECT3DPIXELSHADER9 ReinterpRGBA6ToRGB8();
	static LPDIRECT3DPIXELSHADER9 ReinterpRGB8ToRGBA6();
};

}  // namespace DX9