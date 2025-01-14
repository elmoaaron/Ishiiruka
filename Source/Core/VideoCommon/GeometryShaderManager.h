// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;

// The non-API dependent parts.
class GeometryShaderManager
{
	static bool dirty;
public:
	static void Init();
	static void Dirty();
	static bool IsDirty(){ return dirty; }
	static void Clear(){ dirty = false; }
	static void Shutdown();
	static void DoState(PointerWrap &p);

	static void SetConstants();
	static void SetViewportChanged();
	static void SetProjectionChanged();
	static void SetLinePtWidthChanged();
	static void SetTexCoordChanged(u8 texmapid);

	static GeometryShaderConstants constants;	
};
