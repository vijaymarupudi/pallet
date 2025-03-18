#include <atomic>
#include <optional>
#include "containerUtils.hpp"

namespace pallet::containers {

  template <class T>
  struct ThreadSafeStackNode {
    ThreadSafeStackNode* next;
    Space<T> item;
  };

  template <class T>
  struct ThreadSafeStack {
    using node_type = ThreadSafeStackNode<T>;

    std::atomic<node_type*> head = nullptr;
    std::atomic<node_type*> freeNodes = nullptr;

  private:

    void pushNodeOnList(std::atomic<node_type*>& list, node_type* node) {
      auto list_top_node = list.load(std::memory_order_relaxed);
      node->next = list_top_node;
      while (!list.compare_exchange_weak(node->next, node, std::memory_order_release,
                                         std::memory_order_relaxed)) {}
    }

    node_type*
    popNodeFromList(std::atomic<node_type*>& list) {
      auto list_top_node = list.load(std::memory_order_relaxed);
      if (!list_top_node) { return nullptr; }
      while (!list.compare_exchange_weak(list_top_node, list_top_node->next, std::memory_order_release, std::memory_order_relaxed)) {}
      if (!list_top_node) { return nullptr; }
      return list_top_node;
    }

    public:
    
    template <class V>
    void push(V&& v) {
      auto node = popNodeFromList(this->freeNodes);
      if (!node) {
        node = new node_type(nullptr, {});
      }
      node->item.construct(std::forward<V>(v));
      pushNodeOnList(head, node);
    }

    

    std::optional<T> pop() {
      auto node = popNodeFromList(this->head);
      if (!node) {
        return {};
      }
      auto ret = std::move(node->item.ref());
      // run the destructor of the moved from item
      node->item.ref().~T();
      pushNodeOnList(freeNodes, node);
      return ret;
    }

    ~ThreadSafeStack() {
      node_type *node, *next;
      node = freeNodes;

      while (node) {
        next = node->next;
        delete node;
        node = next;
      }

      node = head;
      while (node) {
        next = node->next;
        node->item.ref().~T();
        delete node;
        node = next;
      }
    }
  
  };

}


