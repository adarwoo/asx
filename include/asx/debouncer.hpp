   #pragma once
// MIT License
//
// Copyright (c) 2025 software@arreckx.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <asx/bitstore.hpp>
#include <array>

namespace asx {
    template<size_t N, size_t THR>
    class Debouncer {
        using bitstore_t = BitStore<N>;
        using status_t = bitstore_t::storage_t;

        bitstore_t inputs{};
        std::array<uint8_t, N> integrator{};

    public:
        bitstore_t append(status_t raw_sample) {
            auto previous = inputs;
            auto sample = bitstore_t{raw_sample};

            for (uint8_t i=0; i<N; ++i) {
                bool level = sample.get(i);

                if (level) {
                    if ( integrator[i] < THR ) {
                        ++integrator[i];
                    }

                    if ( integrator[i] == THR ) {
                        inputs.set(i);
                    }
                } else {
                    if ( integrator[i] > 0 ) {
                        --integrator[i];
                    }

                    if ( integrator[i] == 0 ) {
                        inputs.reset(i);
                    }
                }
            }

            // Return all bits which have changed to become 'true' (key is on)
            return (previous ^ inputs) & inputs;
        }

        bitstore_t status() {
            return inputs;
        }
    };
}
