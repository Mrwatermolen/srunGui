#ifndef __SRUN_GUI_UI_UI_H__
#define __SRUN_GUI_UI_UI_H__

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "common/msg.h"
#include "csp/receiver.h"

namespace srun_gui {

class Ui {
 public:
  auto loadConfig(std::string_view config_file) -> void;

  auto action() { (this->*_state)(); }

  auto setSrun(std::unique_ptr<Sender> srun) { _srun = std::move(srun); }

  auto getSender() const { return _receiver.getSender(); }

  auto configFile() const { return _config_file; }

  auto config() const { return _config; }

  auto userInfo() const { return _user_info; }

 private:
  auto drawIdle() -> void;

  auto drawWait() -> void;

  auto drawInfo() -> void;

  auto configWidget() -> void;

  auto infoWidget() -> void;

  enum class PopupType : std::uint8_t { Unknown, Error, Warning, Info };

  constexpr auto popupId(PopupType type) -> const char*;

  template <typename Msg>
  auto sendToSrun(Msg&& msg) {
    if (!_srun) {
      return;
    }

    _srun->send(std::forward<Msg>(msg));
  }

  auto validConfig(const Config& config) -> std::optional<std::string>;

  auto transitState(void (Ui::*state)()) {
    _last_state = _state;
    _state = state;
  }

  auto revertState() {
    _state = _last_state;
    _last_state = nullptr;
  }

  auto toWaiting(std::string_view overlap, bool enable_cancel = true,
                 std::function<void()> widget = nullptr) -> void;

 private:
  void (Ui::*_state)(){&Ui::drawIdle};
  void (Ui::*_last_state)(){nullptr};

  Receiver _receiver;
  std::unique_ptr<Sender> _srun;

  std::string _config_file;
  Config _config;
  UserInfo _user_info;
};

}  // namespace srun_gui

#endif  // __SRUN_GUI_UI_UI_H__
