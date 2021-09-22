#pragma once
#include "zebratypes.h"
#include <atomic>

namespace zebra {
	extern std::atomic<u64> id_counter;
	u64 genkey();
}