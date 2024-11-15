#ifndef __SRUN_GUI_CSP_DISPATCHER_H__
#define __SRUN_GUI_CSP_DISPATCHER_H__

#include <exception>
#include <memory>
#include <utility>

#include "csp/message.h"

namespace srun_gui {

struct CloseQueueMsg {};

class DispatcherException : public std::exception {};

class DispatcherExceptionGetCloseQueueMsg : public DispatcherException {
 public:
  const char *what() const noexcept override {
    return "Dispatcher get close queue msg";
  }
};

template <typename Msg, typename PrevNode, typename Func, bool Blocking>
class DispatcherNode;

template <bool Blocking>
class DispatcherHead {
  template <typename Msg, typename PrevNodePtr, typename Func,
            bool OtherBlocking>
  friend class DispatcherNode;

 public:
  explicit DispatcherHead(std::shared_ptr<MessageQueue> q) : _q{std::move(q)} {}

  DispatcherHead(const DispatcherHead &) = delete;

  DispatcherHead(DispatcherHead &&) noexcept = default;

  DispatcherHead &operator=(const DispatcherHead &) = delete;

  DispatcherHead &operator=(DispatcherHead &&) noexcept = default;

  ~DispatcherHead() noexcept(false) {
    if (_tail) {
      run();
    }
  }

  template <typename OtherMsg, typename OtherFunc>
  DispatcherNode<OtherMsg, DispatcherHead, OtherFunc, Blocking> dispatch(
      OtherFunc &&func) {
    return DispatcherNode<OtherMsg, DispatcherHead, OtherFunc, Blocking>{
        this, std::forward<OtherFunc>(func), _q};
  }

 private:
  bool _tail{true};
  std::shared_ptr<MessageQueue> _q;

  void join() { _tail = false; }

  void run() {
    std::shared_ptr<Message> msg;
    if constexpr (Blocking) {
      for (;;) {
        _q->waitAndPop(msg);
        tryHandleMsg(msg);
      }
    } else {
      if (!_q->tryPop(msg)) {
        return;
      }

      tryHandleMsg(msg);
    }
  }

  bool tryHandleMsg(const std::shared_ptr<Message> &msg) {
    auto close_msg =
        std::dynamic_pointer_cast<MessageWrapper<CloseQueueMsg>>(msg);
    if (close_msg == nullptr) {
      return false;
    }

    throw DispatcherExceptionGetCloseQueueMsg{};
  }
};

template <typename Msg, typename PrevNode, typename Func, bool Blocking>
class DispatcherNode {
  template <typename OtherMsg, typename OtherPrevNode, typename OtherFunc,
            bool OtherBlocking>
  friend class DispatcherNode;

 public:
  using NodePtr = PrevNode *;

  DispatcherNode(NodePtr prev, Func func, std::shared_ptr<MessageQueue> q)
      : _prev{prev}, _func{std::move(func)}, _q{std::move(q)} {
    prev->join();
  }

  DispatcherNode(const DispatcherNode &) = delete;

  DispatcherNode(DispatcherNode &&) noexcept = delete;

  DispatcherNode &operator=(const DispatcherNode &) = delete;

  DispatcherNode &operator=(DispatcherNode &&) noexcept = delete;

  ~DispatcherNode() {
    if (_tail) {
      run();
    }
  }

  template <typename OtherMsg, typename OtherFunc>
  DispatcherNode<OtherMsg, DispatcherNode, OtherFunc, Blocking> dispatch(
      OtherFunc &&func) {
    return DispatcherNode<OtherMsg, DispatcherNode, OtherFunc, Blocking>{
        this, std::forward<OtherFunc>(func), _q};
  }

 private:
  bool _tail{true};
  NodePtr _prev;
  Func _func;
  std::shared_ptr<MessageQueue> _q;

  void join() { _tail = false; }

  void run() {
    std::shared_ptr<Message> msg;
    if constexpr (Blocking) {
      for (;;) {
        _q->waitAndPop(msg);
        if (tryHandleMsg(msg)) {
          break;
        }
      }
    } else {
      if (!_q->tryPop(msg)) {
        return;
      }

      tryHandleMsg(msg);
    }
  }

  bool tryHandleMsg(const std::shared_ptr<Message> &msg) {
    auto my_msg_wrapper = std::dynamic_pointer_cast<MessageWrapper<Msg>>(msg);
    if (my_msg_wrapper != nullptr) {
      _func(my_msg_wrapper->content());
      return true;
    }

    return _prev->tryHandleMsg(msg);
  }
};

}  // namespace srun_gui

#endif  // __SRUN_GUI_CSP_DISPATCHER_H__
