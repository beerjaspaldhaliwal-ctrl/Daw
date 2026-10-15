// ME Pro EQ — ME Audio
// Real-input FFT (power-of-two sizes, 2^4 .. 2^16), double precision. Iterative radix-2 complex transform of
// size N/2 with the standard real-packing post-processing. Tables are built in the constructor; transforms
// do not allocate.
#pragma once

#include <complex>
#include <vector>

namespace mepro::dsp
{

class RealFFT
{
public:
    explicit RealFFT (int size);

    int getSize() const noexcept { return n; }

    // input: n real samples. output: n/2 + 1 complex bins (DC .. Nyquist). Unnormalised (sum convention).
    void forward (const double* input, std::complex<double>* output) noexcept;

    // input: n/2 + 1 complex bins (Hermitian spectrum). output: n real samples, scaled by 1/n.
    void inverse (const std::complex<double>* input, double* output) noexcept;

private:
    void complexTransform (std::complex<double>* data, bool inverse) noexcept;

    int n = 0, half = 0;
    std::vector<int> bitrev;
    std::vector<std::complex<double>> twiddles; // half-size complex FFT twiddles
    std::vector<std::complex<double>> post;     // real-packing twiddles e^{-j 2 pi k / n}
    std::vector<std::complex<double>> work;
};

} // namespace mepro::dsp
