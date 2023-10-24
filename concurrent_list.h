#include <atomic>
#include <vector>

template <typename KeyType, typename ValueType>
class ConcurrentList {
 private:
  struct Node {
    KeyType key;
    ValueType val;
    std::atomic<Node *> pre;
    std::atomic<Node *> next;

    Node() : key(0), val(0), pre(nullptr), next(nullptr) {}
    Node(KeyType _key, ValueType _val) : key(_key), val(_val), pre(nullptr), next(nullptr) {}
  };

 public:
  ConcurrentList() {
    Init();
  }

  void Init();
  Node *push(KeyType key, ValueType val);
  Node *pop();

 private:
  std::atomic<Node *> head;
  std::atomic<Node *> tail;
};