#include <srun/exception.h>

#include <iostream>
#include <vector>

#include "common/msg.h"
#include "csp/dispatcher.h"
#include "srun_backend.h"

namespace srun_gui {

auto SrunBackend::run() -> void {
  try {
    while (true) {
      (this->*_state)();
    }
  } catch (const DispatcherExceptionGetCloseQueueMsg& e) {
    std::cout << "Received CloseQueueMsg, exiting...\n";
    return;
  }
}

void SrunBackend::idle() {
  _receiver.wait<true>()
      .dispatch<ErrMsg>([](const ErrMsg& msg) {
        std::cerr << "SrunBack Error: " << msg.err_msg << "\n";
      })
      .dispatch<RequestLoadConfig>(
          [this](const RequestLoadConfig& request_msg) {
            std::cout << "Load config\n";
            this->loadConfig(request_msg.config);
            std::cout << "Load config done\n";
          })
      .dispatch<RequestLoadConfigFile>(
          [this](const RequestLoadConfigFile& msg) {
            std::cout << "Load config file\n";
            this->loadConfigFile(msg.config_file);
            std::cout << "Load config file done\n";
          })
      .dispatch<RequestLogin>([this](const RequestLogin& msg) {
        std::cout << "Login\n";
        this->login();
        std::cout << "Login done\n";
      })
      .dispatch<RequestInfo>([this](const RequestInfo& msg) {
        std::cout << "Get info\n";
        this->getInfo();
        std::cout << "Get info done\n";
      })
      .dispatch<RequestLogout>([this](const RequestLogout& msg) {
        std::cout << "Logout\n";
        this->logout();
        std::cout << "Logout done\n";
      });
}

auto SrunBackend::loadConfig(const Config& config) -> void {
  if (!config.auto_ip) {
    _client.setIp(config.ip);
  }

  if (!config.auto_ac_id) {
    _client.setAcId(config.ac_id);
  }

  _client.setSsl(config.protocol == "https");
  _client.setHost(config.host);
  _client.setPort(config.port);
  _client.setUsername(config.username);
  _client.setPassword(config.password);
}

auto SrunBackend::loadConfigFile(std::string_view config_file) -> void {
  try {
    _client.init(config_file);
    sendToUi(DrawConfig{.err_msg = {},
                        .finished = true,
                        .config_file = std::string(config_file),
                        .config = {.protocol = _client.ssl() ? "https" : "http",
                                   .host = _client.host(),
                                   .port = _client.port(),
                                   .username = _client.username(),
                                   .password = _client.password(),
                                   .auto_ip = _client.autoIp(),
                                   .ip = _client.ip(),
                                   .auto_ac_id = _client.autoAcId(),
                                   .ac_id = _client.acId()}});
  } catch (const srun::SrunException& e) {
    sendToUi(DrawConfig{.err_msg = e.what(),
                        .finished = false,
                        .config_file = std::string(config_file)});
    return;
  }
}

auto SrunBackend::login() -> void {
  try {
    // is online
    if (_client.checkOnline()) {
      sendToUi(DrawLogin{.finished = true, .username = _client.username()});
      return;
    }

    sendToUi(DrawLogin{
        .err_msg = {}, .finished = false, .username = _client.username()});

    _client.login();

    sendToUi(DrawLogin{
        .err_msg = {},
        .finished = true,
        .username = _client.username(),
    });
  } catch (const srun::SrunException& e) {
    sendToUi(DrawLogin{.err_msg = e.what(),
                       .finished = false,
                       .username = _client.username()});
    return;
  }
}

auto SrunBackend::getInfo() -> void {
  try {
    sendToUi(makeDrawInfo(_client.getInfo()));
  } catch (const srun::SrunException& e) {
    sendToUi(DrawInfo{.err_msg = e.what(), .finished = false, .user_info = {}});
    return;
  }
}

auto SrunBackend::makeDrawInfo(const srun::InfoResponse& info) -> DrawInfo {
  std::vector<OnlineDeviceInfo> online_device_info;
  for (const auto& device : info.onlineDevices()) {
    online_device_info.push_back(
        OnlineDeviceInfo{.class_name = device.className(),
                         .ipv4 = device.ipv4(),
                         .ipv6 = device.ipv6(),
                         .os_name = device.osName(),
                         .rad_online_id = device.radOnlineId()});
  }

  return DrawInfo{
      .err_msg = {},
      .finished = true,
      .user_info = {.username = info.username(),
                    .online_ip = info.onlineIp(),
                    .mac = info.userMac(),
                    .wallet_balance = static_cast<double>(info.walletBalance()),
                    .remain_seconds = info.remainSeconds(),
                    .sum_seconds = info.sumSeconds(),
                    .in_bytes = info.bytesIn(),
                    .out_bytes = info.bytesOut(),
                    .remain_bytes = info.remainBytes(),
                    .sum_bytes = info.sumBytes(),
                    .online_device_info = online_device_info}};
}

auto SrunBackend::logout() -> void {
  try {
    _client.logout();
    sendToUi(DrawLogout{.err_msg = {}, .finished = true});
  } catch (const srun::SrunException& e) {
    sendToUi(DrawLogout{.err_msg = e.what(), .finished = false});
    return;
  }
}

}  // namespace srun_gui