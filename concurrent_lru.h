#include "folly/MPMCQueue.h"
#include "folly/concurrency/ConcurrentHashMap.h"
#include "folly/concurrency/ConcurrentHashMap.h"
#include "ks/photo_store/concurrent_lru/concurrent_list.h"

template <typename KeyType, typename ValueType>
class ConcurrentLru {
 private:
  struct Node;

  struct Node {
    KeyType key;
    ValueType type;
  };

 private:
  Node *get(KeyType key);
  Node *load(KeyType key, ValueType val);
  void offer(Node *n);
  void put(Node *n);
  void evict();
  void purge();

 private:
  folly::ConcurrentHashMap<KeyType, Node *> mp;
  folly::MPMCQueue<Node *> q;

  // friend ConcurrentList<KeyType, ValueType>;
};