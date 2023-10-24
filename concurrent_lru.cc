#include "ks/photo_store/concurrent_lru/concurrent_lru.h"
#include "folly/MPMCQueue.h"
#include "folly/concurrency/ConcurrentHashMap.h"

template <typename KeyType, typename ValueType>
typename ConcurrentLru<KeyType, ValueType>::Node *ConcurrentLru<KeyType, ValueType>::get(KeyType key) {
  if (mp.find(key) != mp.end()) {
    Node *e = mp[key];
    offer(*e);

    purge();
  } else {
    Node* n = new Node(key, value);
   }
}

template <typename KeyType, typename ValueType>
void ConcurrentLru<KeyType, ValueType>::offer(typename ConcurrentLru<KeyType, ValueType>::Node *n) {}

template <typename KeyType, typename ValueType>
void ConcurrentLru<KeyType, ValueType>::put(typename ConcurrentLru<KeyType, ValueType>::Node *n) {}

template <typename KeyType, typename ValueType>
void ConcurrentLru<KeyType, ValueType>::evict() {}

template <typename KeyType, typename ValueType>
void ConcurrentLru<KeyType, ValueType>::purge() {}