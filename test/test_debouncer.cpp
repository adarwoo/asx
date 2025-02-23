#include <gtest/gtest.h>
#include <asx/bitstore.hpp>
#include <asx/debouncer.hpp>

namespace asx
{
    // Test the Debouncer class
    TEST(DebouncerTest, AppendAndStatus)
    {
        Debouncer<8, 3> debouncer;

        // Initial status should be all bits off
        EXPECT_EQ(debouncer.status(), 0);

        // Append a raw sample with some bits set
        auto changed = debouncer.append(0b00001111);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0);

        changed = debouncer.append(0b00001111);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0);

        changed = debouncer.append(0b00001111);
        EXPECT_EQ(changed, 0b00001111);
        EXPECT_EQ(debouncer.status(), 0b00001111);

        changed = debouncer.append(0b00001111);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0b00001111);

        // Append a raw sample with different bits set
        changed = debouncer.append(0b11110000);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0b00001111);

        changed = debouncer.append(0b11110000);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0b00001111);

        changed = debouncer.append(0b11110000);
        EXPECT_EQ(changed, 0b11110000);
        EXPECT_EQ(debouncer.status(), 0b11110000);

        changed = debouncer.append(0b11110000);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0b11110000);

        changed = debouncer.append(0b11110000);
        EXPECT_EQ(changed, 0);
        EXPECT_EQ(debouncer.status(), 0b11110000);

        // Append a raw sample with all bits off
        changed = debouncer.append(0b00000011);
        EXPECT_EQ(changed, 0);

        changed = debouncer.append(0b00000011);
        EXPECT_EQ(changed, 0);

        changed = debouncer.append(0b00000011);
        EXPECT_EQ(changed, 0b0011);
        EXPECT_EQ(debouncer.status(), 0b00000011);
    }
}
