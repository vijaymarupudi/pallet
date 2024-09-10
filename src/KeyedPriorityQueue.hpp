#pragma once

#include <utility>
#include <functional>
#include "PriorityQueue.hpp"

template <class KeyType, class DataType,
          template<class> class Container = std::vector,
          class Compare = std::less<KeyType>>
class KeyedPriorityQueue {
  using QueueEntry = std::pair<KeyType, DataType>;
  class QueueEntryCompare {
    Compare compare;
  public:
    bool operator()(const QueueEntry& a, const QueueEntry& b) {
      return compare(std::get<0>(a), std::get<0>(b));
    }
  };
  vjcontainers::PriorityQueue<QueueEntry, Container<QueueEntry>, QueueEntryCompare> queue;
public:

  KeyedPriorityQueue() : queue(QueueEntryCompare()) {
  }

  using value_type = KeyType;

  template<class KType, class DType>
  void push(KType&& key, DType&& data) {
    this->queue.push(std::pair{std::forward<KType>(key), std::forward<DType>(data)});
  }

  const std::pair<KeyType, DataType>& top() {
    return this->queue.top();
  }

  void pop() {
    this->queue.pop();
  }

  size_t size() const {
    return queue.size();
  }
};
