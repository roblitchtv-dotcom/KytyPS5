#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GCLEVELS_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GCLEVELS_H_

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics {

// Device memory use at which a cache starts collecting, collects under pressure and collects
// aggressively, derived from the budget GraphicContext::GetTotalMemoryBudget() reports.
struct GcLevels {
	uint64_t trigger  = 0;
	uint64_t pressure = 0;
	uint64_t critical = 0;
};

namespace GcLevelsDetail {

constexpr int64_t GiB = 1024ll * 1024 * 1024;

// The fixed floors suit cards with 8 GiB or more. On a smaller discrete card they sit at or past
// the budget, so the cache would only collect once the driver already spills to system memory;
// there a floor is capped at a share of the budget instead. Integrated GPUs keep the fixed
// floors: their budget is the small carve-out, while the shared heap holds far more.
constexpr int64_t Floor(int64_t floor, int64_t budget, int64_t percent, bool discrete) {
	return discrete ? std::min(floor, budget * percent / 100) : floor;
}

} // namespace GcLevelsDetail

[[nodiscard]] constexpr GcLevels TextureCacheGcLevels(int64_t budget, bool discrete) noexcept {
	using GcLevelsDetail::Floor;
	using GcLevelsDetail::GiB;
	const auto threshold = std::min<int64_t>(budget, 8 * GiB);
	const auto pressure  = std::max<int64_t>(std::min(budget - 6 * threshold / 10, budget - GiB),
	                                         Floor(GiB + GiB / 2, budget, 50, discrete));
	const auto critical = std::max<int64_t>(std::min(budget - 2 * threshold / 10, budget - GiB / 2),
	                                        Floor(3 * GiB, budget, 80, discrete));
	return {.trigger  = static_cast<uint64_t>(std::max<int64_t>((budget - threshold) / 2, 0)),
	        .pressure = static_cast<uint64_t>(pressure),
	        .critical = static_cast<uint64_t>(critical)};
}

[[nodiscard]] constexpr GcLevels BufferCacheGcLevels(int64_t budget, bool discrete) noexcept {
	using GcLevelsDetail::Floor;
	using GcLevelsDetail::GiB;
	const auto threshold = std::min<int64_t>(budget, 8 * GiB);
	const auto expected  = std::min(budget - 6 * threshold / 10, budget - GiB);
	const auto critical  = std::min(budget - 2 * threshold / 10, budget - GiB / 2);
	return {.trigger = static_cast<uint64_t>(
	            std::max<int64_t>(expected, Floor(GiB, budget, 40, discrete))),
	        .critical = static_cast<uint64_t>(
	            std::max<int64_t>(critical, Floor(2 * GiB, budget, 80, discrete)))};
}

} // namespace Libs::Graphics

#endif /* EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GCLEVELS_H_ */
