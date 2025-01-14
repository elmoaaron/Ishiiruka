// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2++
// Refer to the license.txt file included.

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/BoundingBox.h"

#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/BoundingBox.h"

static GLuint s_bbox_buffer_id;
alignas(128) static s32 s_values[4];
static bool s_cpu_dirty;
static bool s_gpu_dirty;

namespace OGL
{

void BBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		s_values[0] = s_values[1] = s_values[2] = s_values[3] = 0;
		glGenBuffers(1, &s_bbox_buffer_id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(s32), s_values, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, s_bbox_buffer_id);
		s_cpu_dirty = true;
		s_gpu_dirty = true;
	}
}

void BBox::Shutdown()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
		glDeleteBuffers(1, &s_bbox_buffer_id);
}

void BBox::Update()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox 
		&& g_ActiveConfig.iBBoxMode == BBoxGPU
		&& BoundingBox::active
		&& s_cpu_dirty)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 4 * sizeof(s32), s_values);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		s_cpu_dirty = false;
		s_gpu_dirty = true;
	}
}

void BBox::Set(s32 index, s32 value)
{
	if (s_values[index] != value)
	{
		s_values[index] = value;
		s_cpu_dirty = true;
	}
}

s32 BBox::Get(s32 index)
{
	if (s_gpu_dirty && g_ActiveConfig.iBBoxMode == BBoxGPU)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
		void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4 * sizeof(s32), GL_MAP_READ_BIT);
		if (ptr)
		{
			memcpy(s_values, ptr, 4 * sizeof(s32));
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		s_gpu_dirty = false;
	}
	return s_values[index];
}

};
