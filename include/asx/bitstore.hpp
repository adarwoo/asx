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

#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <cstdbool>

namespace asx
{
   template <size_t N>
   class BitStore
   {
   public:
      static constexpr auto size = N;
      static_assert(N<=32, "Are you kidding! This is an 8 bits micro");
      using storage_t =
         std::conditional_t<
            N <= 8,
            uint8_t,
            std::conditional_t<
               N <= 16,
               uint16_t,
               uint32_t
            >
         >;

   private:
      storage_t bits;

   public:
      // Default constructor
      BitStore() : bits{0} {};

      // Constructor to initialize with an unsigned integer
      BitStore(storage_t value) : bits{value} {};

      // Set a bit at a specific position
      void set(size_t pos, bool value = true)
      {
         if (pos < N)
         {
            if (value)
            {
               bits |= (1U << pos);
            }
            else
            {
               bits &= ~(1U << pos);
            }
         }
      }

      // Get the value of a bit at a specific position
      bool get(size_t pos) const
      {
         if (pos < N)
         {
            return (bits >> pos) & 1U;
         }
         return false;
      }

      // Reset a bit at a specific position
      void reset(size_t pos)
      {
         if (pos < N)
         {
            bits &= ~(1U << pos);
         }
      }

      // Toggle a bit at a specific position
      void toggle(size_t pos)
      {
         if (pos < N)
         {
            bits ^= (1U << pos);
         }
      }

      // Bitwise XOR operator
      BitStore operator^(const BitStore &other) const
      {
         return BitStore(bits ^ other.bits);
      }

      // Bitwise AND operator
      BitStore operator&(const BitStore &other) const
      {
         return BitStore(bits & other.bits);
      }

      // Bitwise OR operator
      BitStore operator|(const BitStore &other) const
      {
         return BitStore(bits | other.bits);
      }

      // Equality operator
      bool operator==(const BitStore &other) const
      {
         return bits == other.bits;
      }

      // Iterator class
      class Iterator
      {
      private:
         const BitStore &bitStore;
         size_t pos;

      public:
         Iterator(const BitStore &bs, size_t position) : bitStore(bs), pos(position) {}

         bool operator*() const
         {
            return bitStore.get(pos);
         }

         Iterator &operator++()
         {
            ++pos;
            return *this;
         }

         bool operator!=(const Iterator &other) const
         {
            return pos != other.pos;
         }
      };

      // Begin iterator
      Iterator begin() const
      {
         return Iterator(*this, 0);
      }

      // End iterator
      Iterator end() const
      {
         return Iterator(*this, N);
      }
   };

   // Specialization for 1 bit
   template <>
   class BitStore<1>
   {
   public:
      static constexpr auto size = 1;
      using storage_t = bool;

   private:
      storage_t bit;

   public:
      BitStore() : bit{false} {}
      BitStore(storage_t value) : bit{value} {}

      void set(size_t pos, bool value = true) {
         if (pos == 0) bit = value;
      }

      bool get(size_t pos) const {
         return (pos == 0) ? bit : false;
      }

      void reset(size_t pos) {
         if (pos == 0) bit = false;
      }

      void toggle(size_t pos) {
         if (pos == 0) bit = !bit;
      }

      void set() {
         bit = true;
      }

      bool get() const {
         return bit;
      }

      void reset() {
         bit = false;
      }

      void toggle() {
         bit = !bit;
      }

      BitStore operator^(const BitStore &other) const {
         return BitStore(bit ^ other.bit);
      }

      BitStore operator&(const BitStore &other) const {
         return BitStore(bit & other.bit);
      }

      BitStore operator|(const BitStore &other) const {
         return BitStore(bit | other.bit);
      }

      bool operator==(const BitStore &other) const {
         return bit == other.bit;
      }

      // Iterator for compatibility
      class Iterator {
         const BitStore &bs;
         size_t pos;
      public:
         Iterator(const BitStore &b, size_t p) : bs(b), pos(p) {}
         bool operator*() const { return bs.get(pos); }
         Iterator &operator++() { ++pos; return *this; }
         bool operator!=(const Iterator &other) const { return pos != other.pos; }
      };

      Iterator begin() const { return Iterator(*this, 0); }
      Iterator end() const { return Iterator(*this, 1); }
   };
}

// End if asx namespace