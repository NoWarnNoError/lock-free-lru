#include "ks/photo_store/concurrent_lru/concurrent_list.h"
#include <atomic>

template <typename KeyType, typename ValueType>
void ConcurrentList<KeyType, ValueType>::Init() {
  Node *dummy = new Node();
  head = dummy;
  tail = dummy;
}

template <typename KeyType, typename ValueType>
typename ConcurrentList<KeyType, ValueType>::Node *ConcurrentList<KeyType, ValueType>::push(KeyType key,
                                                                                            ValueType val) {
  std::atomic<Node *> now = new Node(key, val);
  Node *old_tail = tail.load();

  for (;;) {
    if (old_tail->next.compare_exchange_strong(nullptr, now)) {
      tail.compare_exchange_strong(old_tail, now);

      return now;
    } else {
      old_tail = tail.load();
    }
  }

  return nullptr;
}

template <typename KeyType, typename ValueType>
typename ConcurrentList<KeyType, ValueType>::Node *ConcurrentList<KeyType, ValueType>::pop() {
  if (head == tail) {
    return nullptr;
  }

  Node *old_head = head.load();
  for (;;) {
    if (head.compare_exchange_strong(old_head, old_head->next)) {
      return old_head;
    } else {
      old_head = head.load();
    }
  }

  return nullptr;
}