// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"

#include <vector>

class TextureScaler {
public:
	TextureScaler();
	~TextureScaler();

	u32* Scale(u32* data, int width, int height);

	enum { NONE = 0, XBRZ = 1, HYBRID = 2, BICUBIC = 3, HYBRID_BICUBIC = 4, JINC = 5 };

private:

	void ScaleXBRZ(int factor, u32* source, u32* dest, int width, int height);
	void ScaleBilinear(int factor, u32* source, u32* dest, int width, int height);
	void ScaleBicubicBSpline(int factor, u32* source, u32* dest, int width, int height);
	void ScaleBicubicMitchell(int factor, u32* source, u32* dest, int width, int height);
	void ScaleHybrid(int factor, u32* source, u32* dest, int width, int height, bool bicubic = false);
	void ScaleJinc(int factor, u32* source, u32* dest, int width, int height);

	void DePosterize(u32* source, u32* dest, int width, int height);

	bool IsEmptyOrFlat(u32* data, int pixels);

	// depending on the factor and texture sizes, these can get pretty large 
	// maximum is (100 MB total for a 512 by 512 texture with scaling factor 5 and hybrid scaling)
	// of course, scaling factor 5 is totally silly anyway
	SimpleBuf<u32> bufInput, bufDeposter, bufOutput, bufTmp1, bufTmp2, bufTmp3;
};
