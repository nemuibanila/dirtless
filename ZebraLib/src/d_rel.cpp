#include "d_rel.h"
#include <atomic>

namespace zebra {
	std::atomic<u64> id_counter = 1u;
	u64 genkey() {
		return std::atomic_fetch_add(&id_counter, 1);
	}
}