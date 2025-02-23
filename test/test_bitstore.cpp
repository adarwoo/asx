#include <gtest/gtest.h>
#include <asx/bitstore.hpp>

namespace asx
{
   // Test default constructor
   TEST(BitStoreTest, DefaultConstructor)
   {
      BitStore<8> bs;
      for (size_t i = 0; i < 8; ++i)
      {
         EXPECT_FALSE(bs.get(i));
      }
   }

   // Test constructor with initial value
   TEST(BitStoreTest, ConstructorWithValue)
   {
      BitStore<8> bs(0b10101010);
      EXPECT_TRUE(bs.get(1));
      EXPECT_TRUE(bs.get(3));
      EXPECT_TRUE(bs.get(5));
      EXPECT_TRUE(bs.get(7));
      EXPECT_FALSE(bs.get(0));
      EXPECT_FALSE(bs.get(2));
      EXPECT_FALSE(bs.get(4));
      EXPECT_FALSE(bs.get(6));
   }

   // Test set and get methods
   TEST(BitStoreTest, SetAndGet)
   {
      BitStore<8> bs;
      bs.set(3);
      EXPECT_TRUE(bs.get(3));
      bs.set(3, false);
      EXPECT_FALSE(bs.get(3));
   }

   // Test reset method
   TEST(BitStoreTest, Reset)
   {
      BitStore<8> bs;
      bs.set(4);
      EXPECT_TRUE(bs.get(4));
      bs.reset(4);
      EXPECT_FALSE(bs.get(4));
   }

   // Test toggle method
   TEST(BitStoreTest, Toggle)
   {
      BitStore<8> bs;
      bs.toggle(2);
      EXPECT_TRUE(bs.get(2));
      bs.toggle(2);
      EXPECT_FALSE(bs.get(2));
   }

   // Test bitwise XOR operator
   TEST(BitStoreTest, BitwiseXOR)
   {
      BitStore<8> bs1(0b1100);
      BitStore<8> bs2(0b1010);
      BitStore<8> result = bs1 ^ bs2;
      EXPECT_TRUE(result.get(1));
      EXPECT_TRUE(result.get(2));
      EXPECT_FALSE(result.get(0));
      EXPECT_FALSE(result.get(3));
   }

   // Test bitwise AND operator
   TEST(BitStoreTest, BitwiseAND)
   {
      BitStore<8> bs1(0b1100);
      BitStore<8> bs2(0b1010);
      BitStore<8> result = bs1 & bs2;
      EXPECT_TRUE(result.get(3));
      EXPECT_FALSE(result.get(0));
      EXPECT_FALSE(result.get(1));
      EXPECT_FALSE(result.get(2));
   }

   // Test bitwise OR operator
   TEST(BitStoreTest, BitwiseOR)
   {
      BitStore<8> bs1(0b1100);
      BitStore<8> bs2(0b1010);
      BitStore<8> result = bs1 | bs2;
      EXPECT_TRUE(result.get(1));
      EXPECT_TRUE(result.get(2));
      EXPECT_TRUE(result.get(3));
      EXPECT_FALSE(result.get(0));
   }

   // Test iterator
   TEST(BitStoreTest, Iterator)
   {
      BitStore<8> bs(0b10101010);
      auto it = bs.begin();
      EXPECT_FALSE(*it);
      ++it;
      EXPECT_TRUE(*it);
      ++it;
      EXPECT_FALSE(*it);
      ++it;
      EXPECT_TRUE(*it);
   }
}
