#ifndef __SRUN_GUI_COMMON_MSG_H__
#define __SRUN_GUI_COMMON_MSG_H__

#include <optional>
#include <string>
#include <vector>

namespace srun_gui {

struct Config {
  std::string protocol;
  std::string host;
  std::string port;
  std::string username;
  std::string password;
  bool auto_ip{true};
  std::string ip;
  bool auto_ac_id{true};
  int ac_id{};
};

struct OnlineDeviceInfo {
  std::string class_name;
  std::string ipv4;
  std::string ipv6;
  std::string os_name;
  std::string rad_online_id;
};

struct UserInfo {
  std::string username;
  std::string online_ip;
  std::string mac;
  double wallet_balance{};
  std::size_t remain_seconds{};
  std::size_t sum_seconds{};
  std::size_t in_bytes{};
  std::size_t out_bytes{};
  std::size_t remain_bytes{};
  std::size_t sum_bytes{};
  std::vector<OnlineDeviceInfo> online_device_info;
};

struct RequestLoadConfigFile {
  std::string config_file;
};

struct RequestLoadConfig {
  struct Config config;
};

struct RequestLogin {};

struct RequestInfo {};

struct RequestLogout {};

struct ErrMsg {
  std::string err_msg;
};

struct DrawConfig {
  std::optional<std::string> err_msg;
  bool finished{};
  std::string config_file;
  struct Config config;
};

struct DrawLogin {
  std::optional<std::string> err_msg;
  bool finished{};
  std::string username;
};

struct DrawInfo {
  std::optional<std::string> err_msg;
  bool finished{};
  UserInfo user_info;
};

struct DrawLogout {
  std::optional<std::string> err_msg;
  bool finished;
  std::string username;
  std::string ip;
};

}  // namespace srun_gui

#endif  // __SRUN_GUI_COMMON_MSG_H__
