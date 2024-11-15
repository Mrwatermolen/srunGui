#ifndef __SRUN_GUI_CSP_THREAD_SAFE_QUEUE_H__
#define __SRUN_GUI_CSP_THREAD_SAFE_QUEUE_H__

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>

namespace srun_gui {

template <typename T> class ThreadSafeQueue {
public:
  ThreadSafeQueue() = default;

  ThreadSafeQueue(const ThreadSafeQueue<T> &other) {
    std::scoped_lock lock{other._m};
    _data = other._data;
  }

  ThreadSafeQueue(ThreadSafeQueue<T> &&other) noexcept {
    std::scoped_lock lock{other._m};
    _data = std::move(other._data);
  }

  ThreadSafeQueue<T> &operator=(const ThreadSafeQueue<T> &other) = delete;

  ThreadSafeQueue<T> &operator=(ThreadSafeQueue<T> &&other) noexcept = delete;

  ~ThreadSafeQueue() = default;

  auto empty() const {
    std::scoped_lock lock{_m};
    return _data.empty();
  }

  auto size() const {
    std::scoped_lock lock{_m};
    return _data.size();
  }

  auto push(T value) {
    std::scoped_lock lock{_m};
    _data.push(std::move(value));
    _cond.notify_one();
  }

  auto waitAndPop(T &value) {
    std::unique_lock lock{_m};
    _cond.wait(lock, [this] { return !_data.empty(); });
    try {
      value = std::move(_data.front());
      _data.pop();
    } catch (const std::exception &e) {
      _cond.notify_one();
      throw e;
    }
  }

  auto tryPop(T &value) -> bool {
    std::scoped_lock lock{_m};
    if (_data.empty()) {
      return false;
    }
    value = std::move(_data.front());
    _data.pop();
    return true;
  }

  auto waitAndPop() -> std::unique_ptr<T> {
    std::unique_lock lock{_m};
    _cond.wait(lock, [this] { return !_data.empty(); });
    try {
      auto res = std::make_unique<T>(std::move(_data.front()));
      _data.pop();
      return res;
    } catch (const std::exception &e) {
      _cond.notify_one();
      throw e;
    }
  }

  auto tryPop() -> std::unique_ptr<T> {
    std::scoped_lock lock{_m};
    if (_data.empty()) {
      return std::unique_ptr<T>{};
    }
    auto res = std::make_unique<T>(std::move(_data.front()));
    _data.pop();
    return res;
  }

private:
  mutable std::mutex _m{};
  std::condition_variable _cond{};
  std::queue<T> _data{};
};

} // namespace srun_gui

#endif // __SRUN_GUI_CSP_THREAD_SAFE_QUEUE_H__
