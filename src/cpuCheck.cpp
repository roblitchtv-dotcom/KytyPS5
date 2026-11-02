// Stops with a readable message when the CPU lacks x86-64-v3. Everything else is built for
// x86-64-v3 because guest code runs natively and needs it, so on an older CPU the first such
// instruction would end the process without a word. This file alone is built for plain x86-64
// and runs before any of that code: as the Windows entry point, ahead of the C runtime, or as the
// first Linux constructor.

#if (defined(__x86_64__) || defined(_M_X64)) &&                                                    \
    ((defined(_MSC_VER) && defined(KYTY_CPU_CHECK_CRT_ENTRY)) || defined(__linux__))

#include <cpuid.h>

namespace {

constexpr char MESSAGE[] =
    "KytyPS5 needs a processor with AVX2, BMI2 and FMA (x86-64-v3): Intel Haswell (2013) or\n"
    "newer, or AMD Excavator, Ryzen or newer. PS5 games run their own code directly on the\n"
    "processor, so they cannot run on this one.\n";

bool HostSupportsX86_64V3() {
	unsigned           eax     = 0;
	unsigned           ebx     = 0;
	unsigned           ecx     = 0;
	unsigned           edx     = 0;
	constexpr unsigned FMA     = 1u << 12u;
	constexpr unsigned MOVBE   = 1u << 22u;
	constexpr unsigned OSXSAVE = 1u << 27u;
	constexpr unsigned AVX     = 1u << 28u;
	constexpr unsigned F16C    = 1u << 29u;
	constexpr unsigned LEAF1   = FMA | MOVBE | OSXSAVE | AVX | F16C;
	if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0 || (ecx & LEAF1) != LEAF1) {
		return false;
	}
	// The OS must also save the SSE and AVX register state.
	unsigned xcr0_low  = 0;
	unsigned xcr0_high = 0;
	asm volatile("xgetbv" : "=a"(xcr0_low), "=d"(xcr0_high) : "c"(0));
	if ((xcr0_low & 0x6u) != 0x6u) {
		return false;
	}
	constexpr unsigned BMI1  = 1u << 3u;
	constexpr unsigned AVX2  = 1u << 5u;
	constexpr unsigned BMI2  = 1u << 8u;
	constexpr unsigned LEAF7 = BMI1 | AVX2 | BMI2;
	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) == 0 || (ebx & LEAF7) != LEAF7) {
		return false;
	}
	constexpr unsigned LZCNT = 1u << 5u;
	return __get_cpuid(0x80000001u, &eax, &ebx, &ecx, &edx) != 0 && (ecx & LZCNT) != 0;
}

} // namespace

#if defined(_MSC_VER)

#include <windows.h>

extern "C" DWORD KYTY_CPU_CHECK_CRT_ENTRY(LPVOID);

// /ENTRY points here. The C runtime is not initialized yet, so only Win32 calls are made.
extern "C" __declspec(safebuffers) DWORD KytyCpuCheckEntry(LPVOID peb) {
	if (!HostSupportsX86_64V3()) {
		DWORD written = 0;
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), MESSAGE, sizeof(MESSAGE) - 1, &written, nullptr);
		MessageBoxA(nullptr, MESSAGE, "KytyPS5", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
	return KYTY_CPU_CHECK_CRT_ENTRY(peb);
}

#else

#include <unistd.h>

// 101 is the earliest priority available to programs.
__attribute__((constructor(101))) static void KytyCpuCheck() {
	if (!HostSupportsX86_64V3()) {
		(void)!write(STDERR_FILENO, MESSAGE, sizeof(MESSAGE) - 1);
		_exit(1);
	}
}

#endif

#endif
