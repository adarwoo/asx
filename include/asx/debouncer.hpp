#pragma once

#include <asx/bitstore.hpp>

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
                uint8_t thr = integrator[i];

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
