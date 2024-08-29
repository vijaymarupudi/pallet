#pragma once

#include <utility>
#include <functional>
#include <queue>
#include <vector>
#include <memory_resource>

template <size_t _capacity, class KeyType, class DataType,
          class Compare = std::less<KeyType>>
class KeyedPriorityQueue {
  using QueueEntry = std::pair<KeyType, DataType>;
  class QueueEntryCompare {
  public:
    bool operator()(const QueueEntry& a, const QueueEntry& b) {
      Compare compare;
      return compare(std::get<0>(a), std::get<0>(b));
    }
  };

  unsigned char storage[sizeof(QueueEntry) * _capacity];
  std::pmr::monotonic_buffer_resource storage_mbr;
  std::pmr::polymorphic_allocator<QueueEntry> storage_allocator;
  QueueEntryCompare comparer;
  std::pmr::vector<QueueEntry> storage_vector;
  std::priority_queue<QueueEntry, std::pmr::vector<QueueEntry>, QueueEntryCompare> queue;
  
public:

  KeyedPriorityQueue() : storage_mbr(std::pmr::monotonic_buffer_resource(&storage, sizeof(QueueEntry) * _capacity)),
                         storage_allocator(&storage_mbr),
                         storage_vector(storage_allocator),
                         queue(comparer, storage_vector) {
    storage_vector.reserve(_capacity);
  }

  using value_type = KeyType;
  
  template<class KType, class DType>
  void push(KType&& key, DType&& data) {
    this->queue.push(std::pair{std::forward<KType>(key), std::forward<DType>(data)});
  }

  const std::pair<KeyType, DataType>& top() {
    return this->queue.top();
  }

  std::pair<KeyType, DataType> pop() {
    QueueEntry topItem = std::move(this->queue.top());
    this->queue.pop();
    return topItem;
  }
  
  size_t size() {
    return queue.size();
  }
};
