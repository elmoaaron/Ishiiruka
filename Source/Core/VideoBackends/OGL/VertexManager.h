// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

namespace OGL
{
	class GLVertexFormat : public NativeVertexFormat
	{
	public:
		GLVertexFormat(const PortableVertexDeclaration &_vtx_decl);
		~GLVertexFormat();

		virtual void SetupVertexPointers() override;

		GLuint VAO;
	};

// Handles the OpenGL details of drawing lots of vertices quickly.
// Other functionality is moving out.
class VertexManager : public ::VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();
	NativeVertexFormat* CreateNativeVertexFormat(const PortableVertexDeclaration &_vtx_decl) override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;
	void PrepareShaders(PrimitiveType primitive, u32 components, const XFMemory &xfr, const BPMemory &bpm, bool ongputhread);
	// NativeVertexFormat use this
	GLuint m_vertex_buffers;
	GLuint m_index_buffers;
	GLuint m_last_vao;
protected:
	void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() override;
private:
	void Draw(u32 stride);
	void vFlush(bool useDstAlpha) override;
	void PrepareDrawBuffers(u32 stride);
};

}
