#pragma once

#include "g_types.h"
#include "zebralib.h"

namespace vku {
	bool load_image_from_file(zebra::UploadContext& up, const char* file, zebra::AllocImage& out_image);
};