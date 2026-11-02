#include "graphics/host_gpu/renderer/cache/gcLevels.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

using Libs::Graphics::BufferCacheGcLevels;
using Libs::Graphics::GcLevels;
using Libs::Graphics::TextureCacheGcLevels;

constexpr int64_t GiB = 1024ll * 1024 * 1024;

void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "GcLevelsTests: failed: %s\n", message);
    std::abort();
  }
}

// GraphicContext::GetTotalMemoryBudget() for a discrete card: the driver's heap budget less an
// eighth of it, at most 1 GiB.
int64_t DiscreteBudget(int64_t heap_budget) {
  return heap_budget - std::min(heap_budget / 8, GiB);
}

bool SameLevels(const GcLevels &a, const GcLevels &b) {
  return a.trigger == b.trigger && a.pressure == b.pressure && a.critical == b.critical;
}

void TestLargeCardsKeepTheirLevels() {
  // Heap budgets of 8, 12, 16 and 24 GiB cards.
  for (const int64_t heap : {7 * GiB, 11 * GiB, 15 * GiB, 23 * GiB}) {
    const auto budget = DiscreteBudget(heap);
    Check(SameLevels(TextureCacheGcLevels(budget, true), TextureCacheGcLevels(budget, false)),
          "the texture levels of an 8 GiB or larger card changed");
    Check(SameLevels(BufferCacheGcLevels(budget, true), BufferCacheGcLevels(budget, false)),
          "the buffer levels of an 8 GiB or larger card changed");
  }
}

void TestSmallCardsCollectBeforeTheBudget() {
  // Heap budgets of 1, 2, 3 and 4 GiB cards.
  for (const int64_t heap : {9 * GiB / 10, 18 * GiB / 10, 27 * GiB / 10, 36 * GiB / 10}) {
    const auto budget  = DiscreteBudget(heap);
    const auto texture = TextureCacheGcLevels(budget, true);
    Check(texture.pressure < texture.critical,
          "texture collection reaches the critical level before the pressure level");
    Check(texture.critical < static_cast<uint64_t>(budget),
          "aggressive texture collection starts past the budget");
    const auto buffer = BufferCacheGcLevels(budget, true);
    Check(buffer.trigger < buffer.critical,
          "buffer collection reaches the critical level before it triggers");
    Check(buffer.critical < static_cast<uint64_t>(budget),
          "aggressive buffer collection starts past the budget");
  }
}

void TestFourGiBCardCollectsAtFourFifths() {
  const auto budget      = DiscreteBudget(36 * GiB / 10);
  const auto critical    = TextureCacheGcLevels(budget, true).critical;
  const auto four_fifths = static_cast<uint64_t>(budget * 4 / 5);
  // Integer rounding puts the level a few bytes above four fifths.
  Check(critical >= four_fifths && critical - four_fifths < 1024 * 1024,
        "a 4 GiB card does not collect textures aggressively at four fifths of its budget");
  Check(TextureCacheGcLevels(budget, false).critical == static_cast<uint64_t>(3 * GiB),
        "the fixed 3 GiB floor no longer applies without the discrete cap");
}

void TestIntegratedGpusKeepFixedFloors() {
  const auto texture = TextureCacheGcLevels(2 * GiB, false);
  Check(texture.pressure == static_cast<uint64_t>(GiB + GiB / 2) &&
            texture.critical == static_cast<uint64_t>(3 * GiB),
        "an integrated GPU lost the fixed texture floors");
  const auto buffer = BufferCacheGcLevels(2 * GiB, false);
  Check(buffer.trigger == static_cast<uint64_t>(GiB) &&
            buffer.critical == static_cast<uint64_t>(2 * GiB),
        "an integrated GPU lost the fixed buffer floors");
}

} // namespace

int main() {
  TestLargeCardsKeepTheirLevels();
  TestSmallCardsCollectBeforeTheBudget();
  TestFourGiBCardCollectsAtFourFifths();
  TestIntegratedGpusKeepFixedFloors();
  std::puts("GcLevelsTests: all cases passed");
  return 0;
}
