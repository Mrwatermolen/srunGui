#ifndef __SRUN_GUI_SRUN_SRUN_H__
#define __SRUN_GUI_SRUN_SRUN_H__

#include <srun/common.h>
#include <srun/srun.h>

#include <memory>
#include <string_view>

#include "common/msg.h"
#include "csp/receiver.h"

namespace srun_gui {

class SrunBackend {
 public:
  auto setUi(std::unique_ptr<Sender> ui) -> void { _ui = std::move(ui); }

  auto run() -> void;

  auto getSender() const { return _receiver.getSender(); }

 private:
  template <typename Msg>
  auto sendToUi(Msg&& msg) {
    if (!_ui) {
      return;
    }

    _ui->send(std::forward<Msg>(msg));
  }

  auto idle() -> void;

  auto loadConfig(const Config& config) -> void;

  auto loadConfigFile(std::string_view config_file) -> void;

  auto login() -> void;

  auto getInfo() -> void;

  auto logout() -> void;

  auto makeDrawInfo(const srun::InfoResponse& info) -> DrawInfo;

  void (SrunBackend::*_state)(){&SrunBackend::idle};

  Receiver _receiver;
  std::unique_ptr<Sender> _ui;
  srun::SrunClient _client;
};

}  // namespace srun_gui

#endif  // __SRUN_GUI_SRUN_SRUN_H__
