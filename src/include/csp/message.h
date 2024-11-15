#ifndef __SRUN_GUI_CSP_MESSAGE_H__
#define __SRUN_GUI_CSP_MESSAGE_H__

#include "csp/thread_safe_queue.h"

namespace srun_gui {

struct Message {
public:
  Message() = default;

  Message(const Message &) = default;

  Message(Message &&) noexcept = default;

  Message &operator=(const Message &) = default;

  Message &operator=(Message &&) noexcept = default;

  virtual ~Message() noexcept = default;
};

template <typename Msg> struct MessageWrapper : Message {

  MessageWrapper() = default;

  explicit MessageWrapper(const Msg &msg) : _msg(msg) {}

  explicit MessageWrapper(Msg &&msg) : _msg(std::move(msg)) {}

  MessageWrapper(const MessageWrapper &) = default;

  MessageWrapper(MessageWrapper &&) noexcept = default;

  MessageWrapper &operator=(const MessageWrapper &) = default;

  MessageWrapper &operator=(MessageWrapper &&) noexcept = default;

  ~MessageWrapper() noexcept override = default;

  const Msg &content() const { return _msg; }

  Msg &content() { return _msg; }

  Msg _msg;
};

using MessageQueue = ThreadSafeQueue<std::shared_ptr<Message>>;

} // namespace srun_gui

#endif // __SRUN_GUI_CSP_MESSAGE_H__
