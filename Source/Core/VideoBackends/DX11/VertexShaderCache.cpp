// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/HLSLCompiler.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VertexShaderManager.h"

#include "VideoBackends/DX11/D3DShader.h"
#include "VideoBackends/DX11/D3DUtil.h"
#include "VideoBackends/DX11/VertexShaderCache.h"

namespace DX11 {

VertexShaderCache::VSCache VertexShaderCache::s_vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::s_last_entry;
VertexShaderUid VertexShaderCache::s_last_uid;
VertexShaderUid VertexShaderCache::s_external_last_uid;

static HLSLAsyncCompiler *s_compiler;
static Common::SpinLock<true> s_vshaders_lock;

static D3D::VertexShaderPtr s_simple_vertex_shader;
static D3D::VertexShaderPtr s_clear_vertex_shader;
static D3D::InputLayoutPtr s_simple_layout;
static D3D::InputLayoutPtr s_clear_layout;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader() { return s_simple_vertex_shader.get(); }
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader() { return s_clear_vertex_shader.get(); }
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout() { return s_simple_layout.get(); }
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout() { return s_clear_layout.get(); }

D3D::ConstantStreamBuffer* vscbuf = nullptr;

D3D::BufferDescriptor VertexShaderCache::GetConstantBuffer()
{
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
	if (VertexShaderManager::IsDirty())
	{
		const size_t size = sizeof(float) * VertexShaderManager::ConstantBufferSize;
		vscbuf->AppendData((void*)VertexShaderManager::GetBuffer(), size);
		VertexShaderManager::Clear();
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, size);
	}
	return vscbuf->GetDescriptor();
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8 *value, u32 value_size)
	{
		VertexShaderCache::InsertByteCode(key, D3DBlob(value_size, value));
	}
};

const char* simple_shader_code = R"hlsl(
struct VSOUTPUT
{
	float4 vPosition : SV_Position;
	float3 vTexCoord : TEXCOORD0;
	float vTexCoord1 : TEXCOORD1;
	float4 vTexCoord2 : TEXCOORD2;
	float4 vTexCoord3 : TEXCOORD3;
};
VSOUTPUT main(float4 inPosition : POSITION,float3 inTEX0 : TEXCOORD0, float3 inTEX1 : TEXCOORD1)
{
	VSOUTPUT OUT;
	OUT.vPosition = inPosition;
	OUT.vTexCoord = inTEX0;
	OUT.vTexCoord1 = inTEX1.z;
	OUT.vTexCoord2 = inTEX0.xyyx + (float4(-0.375f,-0.125f,-0.375f, 0.125f) * inTEX1.xyyx);
	OUT.vTexCoord3 = inTEX0.xyyx + (float4( 0.375f, 0.125f, 0.375f,-0.125f) * inTEX1.xyyx);
	return OUT;
})hlsl";

const char* clear_shader_code = R"hlsl(
struct VSOUTPUT
{
	float4 vPosition : SV_Position;
	float4 vColor0   : COLOR0;
};
VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)
{
	VSOUTPUT OUT;
	OUT.vPosition = inPosition;
	OUT.vColor0 = inColor0;
	return OUT;
})hlsl";


void VertexShaderCache::Init()
{
	s_vshaders_lock.unlock();
	s_compiler = &HLSLAsyncCompiler::getInstance();
	const D3D11_INPUT_ELEMENT_DESC simpleelems[3] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	const D3D11_INPUT_ELEMENT_DESC clearelems[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	
	bool use_partial_buffer_update = D3D::SupportPartialContantBufferUpdate();
	u32 cbsize = VertexShaderManager::ConstantBufferSize * sizeof(float) * (use_partial_buffer_update ? 1024 : 1); // is always multiple of 16
	vscbuf = new D3D::ConstantStreamBuffer(cbsize);
	ID3D11Buffer* buf = vscbuf->GetBuffer();
	
	CHECK(buf != nullptr, "Create vertex shader constant buffer (size=%u)", cbsize);
	D3D::SetDebugObjectName(buf, "vertex shader constant buffer used to emulate the GX pipeline");

	D3DBlob blob;
	D3D::CompileShader(D3D::ShaderType::Vertex, simple_shader_code, blob);
	D3D::device->CreateInputLayout(simpleelems, 3, blob.Data(), blob.Size(), D3D::ToAddr(s_simple_layout));
	s_simple_vertex_shader = D3D::CreateVertexShaderFromByteCode(blob);
	if (s_simple_layout == nullptr ||
		s_simple_vertex_shader == nullptr)
		PanicAlert("Failed to create simple vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	
	D3D::SetDebugObjectName(s_simple_layout.get(), "simple input layout");
	D3D::SetDebugObjectName(s_simple_vertex_shader.get(), "simple vertex shader");

	D3D::CompileShader(D3D::ShaderType::Vertex, clear_shader_code, blob);
	D3D::device->CreateInputLayout(clearelems, 2, blob.Data(), blob.Size(), D3D::ToAddr(s_clear_layout));
	s_clear_vertex_shader = D3D::CreateVertexShaderFromByteCode(blob);
	if (s_clear_layout == nullptr ||
		s_clear_vertex_shader == nullptr)
		PanicAlert("Failed to create clear vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(s_clear_vertex_shader.get(), "clear vertex shader");
	D3D::SetDebugObjectName(s_clear_layout.get(), "clear input layout");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	std::string cache_filename = StringFromFormat("%sIDX11-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_strUniqueID.c_str());
	
	VertexShaderCacheInserter inserter;
	s_vshaders_lock.lock();
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);
	s_vshaders_lock.unlock();

	if (g_Config.bEnableShaderDebugging)
		Clear();

	s_last_entry = nullptr;
	VertexShaderManager::DisableDirtyRegions();
}

void VertexShaderCache::Clear()
{
	s_vshaders_lock.lock();
	for (VSCache::iterator iter = s_vshaders.begin(); iter != s_vshaders.end(); ++iter)
		iter->second.Destroy();
	s_vshaders.clear();
	s_vshaders_lock.unlock();

	s_last_entry = nullptr;
}

void VertexShaderCache::Shutdown()
{
	if (s_compiler)
	{
		s_compiler->WaitForFinish();
	}
	if (vscbuf != nullptr)
	{
		delete vscbuf;
	}

	s_simple_vertex_shader.reset();
	s_clear_vertex_shader.reset();

	s_simple_layout.reset();
	s_clear_layout.reset();

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

void VertexShaderCache::PrepareShader(
	u32 components, 
	const XFMemory 
	&xfr, 
	const BPMemory &bpm, 
	bool ongputhread)
{
	VertexShaderUid uid;
	GetVertexShaderUID(uid, components, xfr, bpm);
	if (ongputhread)
	{
		s_compiler->ProcCompilationResults();
	#if defined(_DEBUG) || defined(DEBUGFAST)
		if (g_ActiveConfig.bEnableShaderDebugging)
		{
			ShaderCode code;
			GenerateVertexShaderCodeD3D11(code, uid.GetUidData());
		}
	#endif
		if (s_last_entry)
		{
			if (uid == s_last_uid)
			{
				return;
			}
		}
		s_last_uid = uid;
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	}
	else
	{
		if (s_external_last_uid == uid)
		{
			return;
		}
		s_external_last_uid = uid;
	}
	s_vshaders_lock.lock();
	VSCacheEntry* entry = &s_vshaders[uid];
	s_vshaders_lock.unlock();
	if (ongputhread)
	{
		s_last_entry = entry;
	}
	// Compile only when we have a new instance
	if (entry->initialized.test_and_set())
	{
		return;
	}

	ShaderCompilerWorkUnit *wunit = s_compiler->NewUnit(VERTEXSHADERGEN_BUFFERSIZE);
	wunit->GenerateCodeHandler = [uid](ShaderCompilerWorkUnit* wunit)
	{
		ShaderCode code;
		code.SetBuffer(wunit->code.data());
		GenerateVertexShaderCodeD3D11(code, uid.GetUidData());
		wunit->codesize = (u32)code.BufferSize();
	};

	wunit->entrypoint = "main";
	wunit->flags = D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
	wunit->target = D3D::VertexShaderVersionString();
	wunit->ResultHandler = [uid, entry](ShaderCompilerWorkUnit* wunit)
	{
		if (SUCCEEDED(wunit->cresult))
		{
			g_vs_disk_cache.Append(uid, (const u8*)wunit->shaderbytecode->GetBufferPointer(), (u32)wunit->shaderbytecode->GetBufferSize());
			PushByteCode(D3DBlob(D3D::UniquePtr<ID3D10Blob>(wunit->shaderbytecode)), entry);
			wunit->shaderbytecode = nullptr;
#if defined(_DEBUG) || defined(DEBUGFAST)
			if (g_ActiveConfig.bEnableShaderDebugging)
			{
				entry->code = wunit->code.data();
			}
#endif
		}
		else
		{
			static int num_failures = 0;
			std::string filename = StringFromFormat("%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
			std::ofstream file;
			OpenFStream(file, filename, std::ios_base::out);
			file << ((const char*)wunit->code.data());
			file.close();

			PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
				filename,
				D3D::VertexShaderVersionString(),
				(char*)wunit->error->GetBufferPointer());
		}
	};
	s_compiler->CompileShaderAsync(wunit);
}

bool VertexShaderCache::TestShader()
{
	int count = 0;
	while (!s_last_entry->compiled)
	{
		s_compiler->ProcCompilationResults();
		if (g_ActiveConfig.bFullAsyncShaderCompilation)
		{
			break;
		}
		Common::cYield(count++);
	}
	return s_last_entry->shader != nullptr && s_last_entry->compiled;
}


void VertexShaderCache::PushByteCode(D3DBlob&& bcodeblob, VSCacheEntry* entry)
{
	entry->shader = std::move(D3D::CreateVertexShaderFromByteCode(bcodeblob));
	entry->compiled = true;
	entry->SetByteCode(std::move(bcodeblob));
	if (entry->shader)
	{
		// TODO: Somehow make the debug name a bit more specific
		D3D::SetDebugObjectName(entry->shader.get(), "a vertex shader of VertexShaderCache");
		INCSTAT(stats.numVertexShadersCreated);
		SETSTAT(stats.numVertexShadersAlive, (int)s_vshaders.size());
	}
}

void VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, D3DBlob&& bcodeblob)
{
	VSCacheEntry* entry = &s_vshaders[uid];
	entry->initialized.test_and_set();
	PushByteCode(std::move(bcodeblob), entry);
}
}  // namespace DX11
