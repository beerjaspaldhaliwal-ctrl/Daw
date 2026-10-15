// ME Pro EQ — ME Audio
#include "FFT.h"
#include "MathUtils.h"

#include <cassert>
#include <cmath>

namespace mepro::dsp
{

RealFFT::RealFFT (int size) : n (size), half (size / 2)
{
    assert (size >= 16 && (size & (size - 1)) == 0);
    bitrev.resize ((size_t) half);
    int bits = 0;
    while ((1 << bits) < half)
        ++bits;
    for (int i = 0; i < half; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            r |= ((i >> b) & 1) << (bits - 1 - b);
        bitrev[(size_t) i] = r;
    }
    twiddles.resize ((size_t) half / 2 + 1);
    for (int k = 0; k <= half / 2; ++k)
        twiddles[(size_t) k] = std::polar (1.0, -kTwoPi * k / half);
    post.resize ((size_t) half + 1);
    for (int k = 0; k <= half; ++k)
        post[(size_t) k] = std::polar (1.0, -kTwoPi * k / n);
    work.resize ((size_t) half);
}

void RealFFT::complexTransform (std::complex<double>* data, bool inverse) noexcept
{
    for (int i = 0; i < half; ++i)
    {
        const int j = bitrev[(size_t) i];
        if (j > i)
            std::swap (data[i], data[j]);
    }
    for (int len = 2; len <= half; len <<= 1)
    {
        const int step = half / len;
        for (int i = 0; i < half; i += len)
        {
            for (int k = 0; k < len / 2; ++k)
            {
                auto w = twiddles[(size_t) (k * step)];
                if (inverse)
                    w = std::conj (w);
                const auto u = data[i + k];
                const auto v = data[i + k + len / 2] * w;
                data[i + k] = u + v;
                data[i + k + len / 2] = u - v;
            }
        }
    }
}

void RealFFT::forward (const double* in, std::complex<double>* out) noexcept
{
    // Pack even/odd samples into a half-size complex sequence.
    for (int i = 0; i < half; ++i)
        work[(size_t) i] = { in[2 * i], in[2 * i + 1] };
    complexTransform (work.data(), false);

    // Split: X[k] = (Z[k] + conj(Z[N/2-k]))/2 + e^{-j2pik/N} (Z[k] - conj(Z[N/2-k]))/(2j)
    out[0] = { work[0].real() + work[0].imag(), 0.0 };
    out[half] = { work[0].real() - work[0].imag(), 0.0 };
    for (int k = 1; k < half; ++k)
    {
        const auto z = work[(size_t) k];
        const auto zc = std::conj (work[(size_t) (half - k)]);
        const auto even = 0.5 * (z + zc);
        const auto odd = std::complex<double> (0.0, -0.5) * (z - zc);
        out[k] = even + post[(size_t) k] * odd;
    }
}

void RealFFT::inverse (const std::complex<double>* in, double* out) noexcept
{
    // Inverse of the split above.
    for (int k = 0; k < half; ++k)
    {
        const auto xk = in[k];
        const auto xc = std::conj (in[half - k]);
        const auto even = 0.5 * (xk + xc);
        const auto odd = 0.5 * (xk - xc) * std::conj (post[(size_t) k]);
        work[(size_t) k] = even + std::complex<double> (0.0, 1.0) * odd;
    }
    complexTransform (work.data(), true);
    const double scale = 1.0 / half;
    for (int i = 0; i < half; ++i)
    {
        out[2 * i] = work[(size_t) i].real() * scale;
        out[2 * i + 1] = work[(size_t) i].imag() * scale;
    }
}

} // namespace mepro::dsp
