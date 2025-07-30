/**
 * @file
 * Compute the FFT of a value sample by sample.
 * This code was written for a microcontroller with limited RAM space.
 * The FFT here is 'progressive', so each new sample participate in the
 *  computation of a new value from the previous samples. The computation
 *  has a predictable worse case duration, making this algorithm highly
 *  suitable for being called within a real time system.
 * The result will lag in time by 2x the FFT window size.
 * The twiddle factors are stored in flash memory to save RAM.
 * The incomming samples are stored in a separate buffer again to save RAM.
 *
 * @author software@arreckx.com
 *****************************************************************************
 */

#include <cmath>
#include <cstring>
#include <cstdint>

#include <algorithm>
#include <complex>
#include <type_traits>
#include <array>


namespace asx {
   namespace fft {
      /** Compute the exponent size from the window size */
      static constexpr size_t exponent_of(const size_t window_size) {
         return (window_size > 1) ? 1 + exponent_of(window_size >> 1) : 0;
      }

      template <size_t FFT_N, size_t SAMPLE_FREQUENCY>
      class SingleBinFFT {
         /** The number of samples based on the exponent size */
         static constexpr int FFT_M = exponent_of(FFT_N);

         static_assert(1L << FFT_M == FFT_N, "FFT window size must be a power of 2");

         /** Half the size of the FFT window */
         static constexpr int FFT_H = (FFT_N / 2);

         /** Compute the bin to compute */
         static constexpr int FFT_BIN_SIZE = (SAMPLE_FREQUENCY / FFT_N);

         /** Define a fast index type */
         using index_t = std::conditional_t<(FFT_N > 256), uint16_t, uint8_t>;

         /** Define the type of raw data coming in */
         using sample_t = int16_t;

         /** Define the complex type */
         using complex_t = std::complex<float>;

         /** Compute the twiddle factors at compile time */
         static constexpr std::array<complex_t, FFT_H> compute_twiddle_factors() {
            std::array<complex_t, FFT_H> twiddle{};

            for (int i = 0; i < FFT_H; ++i) {
               float angle = -2.0f * M_PI * i / FFT_N;
               twiddle[i] = complex_t(std::cos(angle), std::sin(angle));
            }

            return twiddle;
         }

         /** Twiddle factors lookup table */
         static constexpr std::array<complex_t, FFT_H> fft_twiddle = compute_twiddle_factors();

         /** Static data members */
         static inline sample_t  s[FFT_N]   = {0};
         static inline complex_t x[FFT_H]   = {0};
         static inline index_t   bin        = 0;
         static inline index_t   i          = 0;
         static inline index_t   g_size     = 0;
         static inline index_t   w_inc      = 0;
         static inline index_t   w_index    = 0;
         static inline index_t   g_count    = 0;

         /** Start a new FFT cycle */
         static void new_cycle() {
            i = 0;
            w_index = 0;
            g_count = 0;
            w_inc = 1;
            g_size = FFT_H;
         }

         /** Reset the FFT */
         static void reset() {
            w_inc = 0;
            i = 0;
         }

      public:
         /** Initialize the FFT */
         static constexpr void init(uint16_t center_frequency) {
            uint16_t bin_number = center_frequency / FFT_BIN_SIZE;
            bin = 0;

            for (index_t j = 0; j < FFT_M; ++j) {
               bin <<= 1;
               bin |= (bin_number & 1);
               bin_number >>= 1;
            }

            reset();
         }

         static inline uint16_t get_result() {
            // Compute the power
            auto real = x[FFT_H - 1].real();
            auto imag = x[FFT_H - 1].imag();

            auto result = std::sqrt(real * real + imag * imag);

            result /= FFT_H;

            return static_cast<uint16_t>(result);
         }

         /**
          * Process the next sample.
          * @return true when a result is ready.
          */
         static bool next(sample_t sample) {
            // Store the sample in the buffer
            s[i] = sample;

            if (w_inc > 0) {
               if (i != (FFT_N - 1)) {
                  index_t ii = i + g_size;
                  index_t jj = ii - FFT_H;

                  if (bin & g_size) {
                     complex_t w = fft_twiddle[w_index];

                     if (i < FFT_H) {
                        x[jj] = complex_t(
                           static_cast<float>(s[i]) * w.real() - static_cast<float>(s[ii]) * w.imag(),
                           0.0f
                        );
                     } else {
                        index_t k = i - FFT_H;
                        float temp = x[jj].real();

                        x[jj].real(w.real() * (x[k].real() - x[jj].real()) - w.imag() * (x[k].imag() - x[jj].imag()));
                        x[jj].imag(w.real() * (x[k].imag() - x[jj].imag()) + w.imag() * (x[k].real() - temp));
                     }
                  } else if (i < FFT_H) {
                     x[jj] = complex_t(static_cast<float>(s[i] + s[ii]), 0.0f);
                  } else {
                     x[jj] += x[i - FFT_H];
                  }

                  ++i;
                  ++g_count;
                  w_index += w_inc;

                  if (g_count == g_size) {
                     // Double-up the group size
                     g_size >>= 1;
                     g_count = 0;

                     // Next pass
                     w_index = 0;
                     w_inc <<= 1;
                  }
               } else {
                  // Start again
                  new_cycle();

                  // Tell the caller the result is ready
                  return true;
               }
            } else {
               ++i;

               if (i == FFT_N) {
                  new_cycle();
               }
            }

            return false;
         }
      };
   } // namespace fft
} // namespace asx

/* ----------------------------  End of file  ---------------------------- */
