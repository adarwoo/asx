#pragma once
/**
 * Create constexpr ints for flags
 * Usage:
 *    Define a group

 */

#include <cstddef>
#include <utility>
#include <algorithm>
#include <string_view>
#include <alert.h>

namespace asx
{
   /**
    * Defines a string litteral type which allow passing a string in a template type
    */
   template <std::size_t N>
   struct string_literal {
      constexpr string_literal(const char (&str)[N]) {
         std::copy_n(str, N, value);
      }

      char value[N];
      constexpr std::string_view view() const { return { value, N - 1 }; }
   };

   template<typename T, std::size_t Size>
   class FixedPtrQueue {
         std::array<T*, Size> data;
         uint8_t front;
         uint8_t back;
         uint8_t count;
   public:
         FixedPtrQueue() : front(0), back(0), count(0) {}

         void push(T* item) {
            alert_and_stop_if(count == Size);
            data[back] = item;
            back = (back + 1) % Size;
            ++count;
         }

         T* pop() {
            alert_and_stop_if(count == 0);
            auto item = data[front];
            front = (front + 1) % Size;
            --count;
            return item;
         }
   };        
}   
