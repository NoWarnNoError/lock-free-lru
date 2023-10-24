#pragma once
#include <vector>
#include <string>
#include <functional>

#include "base/common/basic_types.h"
#include "base/time/timestamp.h"
#include "ks/util/dynamic_dict.h"
#include "ks/util/shared_ptr.h"

namespace ks {

class LRU {
 public:
  explicit LRU(int size) {
    size_ = size;
    prev_.resize(size + 2, -1);
    next_.resize(size + 2, -1);
    last_access_.resize(size, 0);
    next_[InnerHead()] = InnerTail();
    prev_[InnerTail()] = InnerHead();
    element_size_ = 0;
  }

  int64 GetLastAccess(int id) const {
    if (id < 0 || id >= size_) return 0;
    return last_access_[id];
  }

  void Access(int id, int64 ts) {
    if (id < 0 || id >= size_) return;
    last_access_[id] = ts;
    if (prev_[id] < 0 && next_[id] < 0) {
      ++element_size_;
    } else {
      // 先将 id 摘除
      next_[prev_[id]] = next_[id];
      prev_[next_[id]] = prev_[id];
    }
    // 将 id 放到队尾
    next_[id] = InnerTail();
    prev_[id] = prev_[InnerTail()];
    next_[prev_[id]] = id;
    prev_[next_[id]] = id;
  }

  void Access(int id) {
    Access(id, base::GetTimestamp());
  }

  void Clear(int id) {
    if (id < 0 || id >= size_) return;
    if (next_[id] == -1 || prev_[id] == -1) return;
    next_[prev_[id]] = next_[id];
    prev_[next_[id]] = prev_[id];
    prev_[id] = -1;
    next_[id] = -1;
    last_access_[id] = 0;
    --element_size_;
  }

  int Clear() {
    if (element_size_ == 0) return -1;
    int id = next_[InnerHead()];
    next_[prev_[id]] = next_[id];
    prev_[next_[id]] = prev_[id];
    prev_[id] = -1;
    next_[id] = -1;
    last_access_[id] = 0;
    --element_size_;
    return id;
  }

  // 最近访问时间离现在最远
  int GetHead() const { return (element_size_ == 0)? -1 : next_[InnerHead()]; }
  // 最近访问时间离现在最近
  int GetTail() const { return (element_size_ == 0)? -1 : prev_[InnerTail()]; }
  int GetElementSize() const { return element_size_; }
  int GetSize() const { return size_; }

 private:
  inline int InnerHead() const { return size_; }
  inline int InnerTail() const { return size_ + 1; }
  std::vector<int> prev_;
  std::vector<int> next_;
  std::vector<int64> last_access_;
  int size_;
  int element_size_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(LRU);
};

// 单写者，多读者的 LRUMap
// InsertOrUpdate, GetLRU, LRUClear, 三个函数只能单线程调用
template<class T>
class LRUMap {
 public:
  LRUMap(int bits, int64 delete_ts) : dict_(bits), lru_(1 << bits), values_(1 << bits, NULL) {
    recycle_ = new PtrRecycle<T>(delete_ts);
    own_recycle_ = true;
  }
  LRUMap(int bits, PtrRecycle<T> *recycle)
      : dict_(bits), lru_(1 << bits), values_(1 << bits, NULL), own_recycle_(false), recycle_(recycle) {}
  ~LRUMap() {
    for (auto value : values_) recycle_->Recycle(value);
    if (own_recycle_) delete recycle_;
  }
  T *Get(uint64 key) const {
    int dict_id = dict_.Lookup(key);
    if (dict_id < 0) return NULL;
    return values_[dict_id];
  }
  T *Get(const char *data, int size) const { return Get(base::CityHash64(data, size)); }
  void LRUClear() {
    int delete_id = lru_.Clear();
    if (delete_id >= 0) { recycle_->Recycle(values_[delete_id]);
      values_[delete_id] = NULL;
      CHECK_GE(dict_.DeleteId(delete_id), 0);
    }
  }
  void InsertOrUpdate(uint64 key, T *value) {
    if (dict_.GetTermNum() == dict_.Capacity()) LRUClear();
    int dict_id = dict_.Insert(key);
    CHECK_GE(dict_id, 0);
    recycle_->Recycle(values_[dict_id]);
    values_[dict_id] = value;
    lru_.Access(dict_id);
  }
  // 删除由func来处理的更新，因为delete values_[dict_id]耗时会是InsertOrUpdate的瓶颈
  void InsertOrUpdateWithDeleteHandler(uint64 key, T *value, std::function<void(T *)> func) {
    if (dict_.GetTermNum() == dict_.Capacity()) LRUClear();
    int dict_id = dict_.Insert(key);
    CHECK_GE(dict_id, 0);
    func(values_[dict_id]);
    values_[dict_id] = value;
    lru_.Access(dict_id);
  }
  // 不需要延迟删除的更新(确定没有其它线程读取的情况)
  void InsertOrUpdateWithoutDelay(uint64 key, T *value) {
    if (dict_.GetTermNum() == dict_.Capacity()) LRUClear();
    int dict_id = dict_.Insert(key);
    CHECK_GE(dict_id, 0);
    delete values_[dict_id];
    values_[dict_id] = value;
    lru_.Access(dict_id);
  }
  T *GetHead() const {
    int index = lru_.GetHead();
    if (index < 0 || index >= values_.size()) return NULL;
    return values_[index];
  }
  T *GetTail() const {
    int index = lru_.GetTail();
    if (index < 0 || index >= values_.size()) return NULL;
    return values_[index];
  }
  T *GetLRU() {
    int head_id = lru_.GetHead();
    if (head_id < 0) return NULL;
    return values_[head_id];
  }
  void InsertOrUpdate(const char *data, int size, T *value) {
    InsertOrUpdate(base::CityHash64(data, size), value);
  }
  void InsertOrUpdateWithoutDelay(const char *data, int size, T *value) {
    InsertOrUpdateWithoutDelay(base::CityHash64(data, size), value);
  }
  void InsertOrUpdateWithLock(uint64 key, T *value) {
    base::AutoLock lock(lock_);
    InsertOrUpdate(key, value);
  }
  void LRUClearWithLock() {
    base::AutoLock lock(lock_);
    LRUClear();
  }
  void InsertOrUpdateWithLock(const char *data, int size, T *value) {
    base::AutoLock lock(lock_);
    InsertOrUpdate(data, size, value);
  }
  T *GetLRUWithLock() {
    base::AutoLock lock(lock_);
    return GetLRU();
  }
  void GetAll(std::vector<T *> *effective_values) const {
    effective_values->clear();
    for (int i = 0; i < values_.size(); ++i) {
      auto value = values_[i];
      if (value) effective_values->push_back(value);
    }
  }
  const std::vector<T *> &values() const { return values_; }
  const DynamicDict &dict() const { return dict_; }
  const PtrRecycle<T> *recycle() const { return recycle_; }
  const LRU &lru() const { return lru_; }

 private:
  base::Lock lock_;
  DynamicDict dict_;
  LRU lru_;
  std::vector<T *> values_;
  bool own_recycle_;
  PtrRecycle<T> *recycle_;
  DISALLOW_COPY_AND_ASSIGN(LRUMap);
};

}  // namespace ks

