// ME Pro EQ — ME Audio
// Mid/Side helpers. Convention: M = (L + R) / 2, S = (L - R) / 2, L = M + S, R = M - S.
// With this scaling a mono-compatible signal (L == R) has S == 0 exactly and M == L exactly.
#pragma once

namespace mepro::dsp
{

inline void encodeMidSide (const double* interleavedLR, int n, double* mid, double* side) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const double l = interleavedLR[2 * i], r = interleavedLR[2 * i + 1];
        mid[i] = 0.5 * (l + r);
        side[i] = 0.5 * (l - r);
    }
}

inline void decodeMidSide (const double* mid, const double* side, int n, double* interleavedLR) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        interleavedLR[2 * i] = mid[i] + side[i];
        interleavedLR[2 * i + 1] = mid[i] - side[i];
    }
}

// Mono fold-down used by the mono-compatibility audition: both outputs carry (L + R) / 2.
inline void foldToMono (double* interleavedLR, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const double m = 0.5 * (interleavedLR[2 * i] + interleavedLR[2 * i + 1]);
        interleavedLR[2 * i] = m;
        interleavedLR[2 * i + 1] = m;
    }
}

} // namespace mepro::dsp
