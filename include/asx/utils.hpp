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

   /**
    * @brief Fixed size queue for pointers
    *
    * This is a simple queue implementation that uses a fixed-size array to store pointers.
    * It provides basic operations like push, pop, and checking if the queue is empty or full.
    *
    * @tparam T The type of the elements in the queue (should be a pointer type).
    * @tparam Size The maximum number of elements in the queue.
    */
   template <typename T, std::size_t Size>
   class FixedPtrQueue {
   private:
      std::array<T*, Size> data;
      uint8_t front = 0;
      uint8_t back = 0;
      uint8_t count = 0;

      int find(T* ptr) const {
         for (uint8_t i = 0; i < count; ++i) {
             if (data[(front + i) % Size] == ptr) {
                 return (front + i) % Size;
             }
         }
         return -1; // Not found
     }

     void move_to_front(int index) {
         if (count > 1 && index != front) {
             T* ptr_to_move = data[index];
             // Shift elements from 'front' up to 'index' one position back
             int current = index;

             while (current != front) {
                 int prev = (current == 0) ? Size - 1 : current - 1;
                 data[current] = data[prev];
                 current = prev;
             }

             data[front] = ptr_to_move;
         }
     }

   public:
      FixedPtrQueue() : data{}, front(0), back(0), count(0) {}

      bool empty() const {
         return count == 0;
      }

      bool full() const {
         return count == Size;
      }

      void push(T* item) {
         alert_and_stop_if(full());
         data[back] = item;
         back = (back + 1) % Size;
         ++count;
      }

      void push_unique(T* item) {
         int existing_index = find(item);
         if (existing_index != -1) {
             move_to_front(existing_index);
         } else {
             push(item);
         }
     }

      T* pop() {
         alert_and_stop_if(empty());
         T* item = data[front];
         data[front] = nullptr; // Optional: Clear the popped slot
         front = (front + 1) % Size;
         --count;
         return item;
      }

      T* front_element() const {
         alert_and_stop_if(empty());
         return data[front];
      }

      T* back_element() const {
         alert_and_stop_if(empty());
         // Calculate the index of the back element
         uint8_t back_index = (back == 0) ? Size - 1 : back - 1;
         return data[back_index];
      }

      size_t size() const {
         return count;
      }

      size_t capacity() const {
         return Size;
      }
   };
}
