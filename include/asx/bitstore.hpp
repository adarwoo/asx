#pragma once

#include <type_traits>

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
}

// End if asx namespace