#include "ui.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <utility>

#include "ImGuiFileDialog.h"
#include "common/msg.h"
#include "imgui.h"

namespace srun_gui {

static const std::regex URL_REGEX(
    R"(^(http|https):\/\/([a-zA-Z0-9.-]+):(\d+)$)");

static constexpr std::size_t MAX_PATH_SIZE = 1024;
static constexpr std::size_t MAX_SHORT_STR_SIZE = 256;

// Config State
static bool show_config_error = false;
static std::string config_path_erro;
static char text_config_path[MAX_PATH_SIZE] = "";
static int protocol_item_current{};
static char host[MAX_SHORT_STR_SIZE] = "";
static std::uint16_t port = 80;
static char username[MAX_SHORT_STR_SIZE] = "";
static char password[MAX_SHORT_STR_SIZE] = "";
static std::array<std::uint8_t, 4> ip_parts = {0, 0, 0, 0};
static bool auto_ip = true;
static bool auto_ac_id = true;
static int ac_id = 1;

auto Ui::loadConfig(std::string_view config_file) -> void {
  sendToSrun(RequestLoadConfigFile{.config_file = std::string(config_file)});
}

auto Ui::drawIdle() -> void {
  bool show_window = true;
  static std::string popup_msg = "UNKNOWN ERROR";
  static const char* popup_id = popupId(PopupType::Unknown);
  static std::function<void()> popup_callback{};
  static bool enable_handle = true;

  ImGui::Begin("Hello, world!", &show_window,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize);

  if (enable_handle) {
    _receiver.wait<false>()
        .dispatch<ErrMsg>([this](const ErrMsg& msg) {
          std::cerr << std::format("Ui Error: {}", msg.err_msg) << "\n";
          auto center = ImGui::GetMainViewport()->GetCenter();
          ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                  ImVec2(0.5, 0.5));
          popup_id = popupId(PopupType::Error);
          ImGui::OpenPopup(popup_id);
          popup_msg = std::format("Error: {}", msg.err_msg);
          popup_callback = [this]() { enable_handle = true; };
          enable_handle = false;
        })
        .dispatch<DrawConfig>([this](const DrawConfig& msg) {
          if (msg.err_msg.has_value()) {
            show_config_error = true;
            config_path_erro = msg.err_msg.value();
            return;
          }

          if (!msg.finished) {
            return;
          }

          show_config_error = false;
          _config = msg.config;
          if ((sizeof(text_config_path)) < msg.config_file.size()) {
            std::cout << std::format("Config file path too long: {}. Cut\n",
                                     msg.config_file);
            auto new_name = std::format(
                "...{}", msg.config_file.substr(msg.config_file.size() -
                                                sizeof(text_config_path) + 4));
            std::ranges::copy(new_name, text_config_path);
          } else {
            std::ranges::copy(msg.config_file, text_config_path);
          }

          protocol_item_current = _config.protocol == "https" ? 1 : 0;
          std::ranges::copy(_config.host, host);
          port = std::stoi(_config.port);
          std::ranges::copy(_config.username, username);
          std::ranges::copy(_config.password, password);
          auto_ip = _config.auto_ip;
          auto_ac_id = _config.auto_ac_id;
          if (!auto_ip) {
            auto_ip = false;
            std::sscanf(_config.ip.c_str(), "%hhu.%hhu.%hhu.%hhu",
                        ip_parts.data(), &ip_parts[1], &ip_parts[2],
                        &ip_parts[3]);
          }
          if (!auto_ac_id) {
            ac_id = _config.ac_id;
          }
        });
  }

  configWidget();

  // FIXME(franzero): The problem is that the _state is transitioned to other
  // state while this Popup is still open.
  if (ImGui::BeginPopupModal(popup_id, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s", popup_msg.c_str());
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
      if (popup_callback) {
        popup_callback();
      }
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

// Waiting state
static std::string waiting_overlay_text = "Connecting...";  // Progress text
static bool enable_cancel = true;
static std::function<void()> waiting_widget = nullptr;
auto Ui::drawWait() -> void {
  static std::string waiting_title = "Waiting";
  static bool enable_handle = true;  // Enable handle message
  static std::string popup_msg = "UNKNOWN ERROR";
  static const char* popup_id = popupId(PopupType::Unknown);
  static std::function<void()> popup_callback{};

  ImGui::Begin(waiting_title.c_str(), nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize);

  if (enable_handle) {
    // Handle message
    _receiver.wait<false>()
        .dispatch<ErrMsg>([this](const ErrMsg& msg) {
          auto center = ImGui::GetMainViewport()->GetCenter();
          ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                  ImVec2(0.5, 0.5));
          popup_id = popupId(PopupType::Error);
          ImGui::OpenPopup(popup_id);
          popup_msg = "Error: " + msg.err_msg;
          popup_callback = [this]() {
            revertState();
            enable_handle = true;
          };
          enable_handle = false;
        })
        .dispatch<DrawLogin>([this](const DrawLogin& msg) {
          if (msg.err_msg.has_value()) {
            auto center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                    ImVec2(0.5, 0.5));
            popup_id = popupId(PopupType::Error);
            ImGui::OpenPopup(popup_id);
            popup_msg = "Error: " + msg.err_msg.value();
            popup_callback = [this]() {
              revertState();
              enable_handle = true;
            };
            enable_handle = false;
            return;
          }

          if (!msg.finished) {
            waiting_overlay_text = "Login...";
            return;
          }

          waiting_overlay_text = "Getting user info...";
          sendToSrun(RequestInfo{});
        })
        .dispatch<DrawInfo>([this](const DrawInfo& msg) {
          if (msg.err_msg.has_value()) {
            auto center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                    ImVec2(0.5, 0.5));
            popup_id = popupId(PopupType::Error);
            ImGui::OpenPopup(popup_id);
            popup_msg = "Error: " + msg.err_msg.value();
            popup_callback = [this]() {
              revertState();
              enable_handle = true;
            };
            enable_handle = false;
            return;
          }

          if (!msg.finished) {
            waiting_overlay_text = "Getting user info...";
            return;
          }

          _user_info = msg.user_info;
          transitState(&Ui::drawInfo);
        })
        .dispatch<DrawLogout>([this](const DrawLogout& msg) {
          if (msg.err_msg.has_value()) {
            auto center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                    ImVec2(0.5, 0.5));
            popup_id = popupId(PopupType::Error);
            ImGui::OpenPopup(popup_id);
            popup_msg = "Error: " + msg.err_msg.value();
            popup_callback = [this]() { revertState(); };
            return;
          }

          if (!msg.finished) {
            waiting_overlay_text = "Logout...";
            return;
          }
          auto center = ImGui::GetMainViewport()->GetCenter();
          ImGui::SetNextWindowPos(center, ImGuiCond_Appearing,
                                  ImVec2(0.5, 0.5));
          popup_id = popupId(PopupType::Info);
          ImGui::OpenPopup(popup_id);
          popup_msg = "Logout success.";
          popup_callback = [this]() { transitState(&Ui::drawIdle); };
        });
  }

  {
    // Disable widget
    if (waiting_widget) {
      ImGui::BeginDisabled();
      waiting_widget();
      ImGui::EndDisabled();
    }
  }

  {
    // Progress bar
    ImGui::ProgressBar(-1.0F * static_cast<float>(ImGui::GetTime()),
                       ImVec2(-FLT_MIN, 0.0F), waiting_overlay_text.c_str());
  }

  {
    // Cancel button
    if (enable_cancel) {
      if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
        revertState();
      }
    }
  }

  {
    // Alert popup
    if (ImGui::BeginPopupModal(popup_id, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("%s", popup_msg.c_str());
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
        if (popup_callback) {
          popup_callback();
        }
      }
      ImGui::EndPopup();
    }
  }

  ImGui::End();
}

auto Ui::drawInfo() -> void {
  ImGui::Begin("Info", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize);

  _receiver.wait<false>()
      .dispatch<ErrMsg>([](const ErrMsg& msg) {
        std::cerr << "Ui Error: " << msg.err_msg << "\n";
      })
      .dispatch<DrawInfo>([this](const DrawInfo& msg) {
        if (msg.err_msg.has_value()) {
          std::cerr << "Ui Error: " << msg.err_msg.value() << "\n";
          return;
        }

        if (!msg.finished) {
          return;
        }

        _user_info = msg.user_info;
      });
  // TODO(franzero): Register timer to get info every 5 seconds
  infoWidget();

  ImGui::End();
}

auto Ui::configWidget() -> void {
  {  // config file select
    if (show_config_error) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s",
                         config_path_erro.c_str());
    }

    ImGui::Text("Config Path: %s", text_config_path);
    ImGui::SameLine();
    if (ImGui::Button("Open Config File")) {
      IGFD::FileDialogConfig config;
      config.path = ".";
      // TODO(franzero): Set size
      ImGuiFileDialog::Instance()->OpenDialog(
          "ChooseFileDlgKey", "Choose Config File", ".json,.*", config);
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
      if (ImGuiFileDialog::Instance()->IsOk()) {
        std::string file_path_name =
            ImGuiFileDialog::Instance()->GetFilePathName();

        sendToSrun(RequestLoadConfigFile{.config_file = file_path_name});
      }

      ImGuiFileDialog::Instance()->Close();
    }
  }

  {  // [protocol]://[host]:[port]
    ImGui::PushItemWidth(80);
    const char* protocol_items[] = {"http", "https"};

    protocol_item_current = _config.protocol == "https" ? 1 : 0;
    ImGui::Combo("##protocol", &protocol_item_current, protocol_items,
                 IM_ARRAYSIZE(protocol_items), ImGuiComboFlags_WidthFitPreview);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::InputTextWithHint("##host", "host", host, sizeof(host));
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    ImGui::InputScalar("##port", ImGuiDataType_U16, &port, nullptr, nullptr,
                       nullptr, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();
    _config.protocol = protocol_items[protocol_item_current];
    _config.host = host;
    _config.port = std::to_string(port);

    static std::string url;
    static bool valid_url = false;
    url = _config.protocol + "://" + host + ":" + std::to_string(port);
    valid_url = std::regex_match(url, URL_REGEX);
    if (!valid_url) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid URL:");
    } else {
      ImGui::Text("URL:");
    }
    ImGui::SameLine();
    ImGui::Text("%s", url.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Click to copy");
      ImGui::EndTooltip();
    }
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(url.c_str());
    }
  }

  {  // more option
    if (ImGui::TreeNode("More Option")) {
      ImGui::Text("AC ID:");
      ImGui::SameLine();
      ImGui::Checkbox("##AcIdAuto", &auto_ac_id);
      // tooltip
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("uncheck to input");
        ImGui::EndTooltip();
      }
      ImGui::SameLine();
      ImGui::PushItemWidth(30);
      if (auto_ac_id) {
        ImGui::Text("Auto");
      } else {
        ImGui::InputInt("##AcId", &ac_id, 0, 0);
      }
      ImGui::PopItemWidth();
      _config.auto_ac_id = auto_ac_id;
      _config.ac_id = ac_id;

      ImGui::Text("IP:");
      ImGui::SameLine(78);
      ImGui::Checkbox("##AutoIp", &auto_ip);
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("uncheck to input");
        ImGui::EndTooltip();
      }
      ImGui::SameLine();
      if (auto_ip) {
        ImGui::Text("Auto");
      } else {
        ImGui::InputScalarN("##ip", ImGuiDataType_U8, ip_parts.data(), 4,
                            nullptr, nullptr, "%d");
      }
      _config.auto_ip = auto_ip;
      _config.ip = std::format("{}.{}.{}.{}", ip_parts[0], ip_parts[1],
                               ip_parts[2], ip_parts[3]);

      ImGui::TreePop();
    }
  }

  {  // username and password
    ImGui::Text("Username:");
    ImGui::SameLine();
    ImGui::InputText("##username", username, sizeof(username));
    _config.username = username;

    static bool show_password = false;
    ImGui::Text("Password:");
    ImGui::SameLine();
    ImGui::InputText("##password", password, sizeof(password),
                     !show_password ? ImGuiInputTextFlags_Password
                                    : ImGuiInputTextFlags_None);
    ImGui::SameLine();
    ImGui::Checkbox("Show Password", &show_password);
    _config.password = password;
  }

  {  // submit
    const char* err_popup_id = "Error Config";
    static std::string err_msg;
    if (ImGui::Button("Ready!", ImVec2(-1, 0))) {
      auto err = validConfig(_config);
      bool valid_url = std::regex_match(
          _config.protocol + "://" + _config.host + ":" + _config.port,
          URL_REGEX);

      if (err.has_value() || !valid_url) {
        err_msg = "Error: " + err.value_or("Invalid URL");
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing,
                                ImVec2(0, 0));
        auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5, 0.5));
        ImGui::OpenPopup(err_popup_id);
      } else {
        sendToSrun(RequestLoadConfig{.config = _config});
        sendToSrun(RequestLogin{});
        toWaiting("Connecting...", true, [this]() { configWidget(); });
      }
    }

    if (ImGui::BeginPopupModal(err_popup_id, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("%s", err_msg.c_str());
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}

auto Ui::infoWidget() -> void {
  {
    ImGui::Text("Username: %s", _user_info.username.c_str());
    ImGui::Text("Online IP: %s", _user_info.online_ip.c_str());
    ImGui::Text("MAC: %s", _user_info.mac.c_str());
    ImGui::Text("Wallet Balance: %.2f", _user_info.wallet_balance);
    ImGui::Text("Remain Seconds: %zu", _user_info.remain_seconds);
    ImGui::Text("Sum Seconds: %zu", _user_info.sum_seconds);
    ImGui::Text("In Bytes: %zu", _user_info.in_bytes);
  }
  if (ImGui::Button("Logout", ImVec2(-1, 0))) {
    sendToSrun(RequestLogout{});
    toWaiting("Logout...", false, [this]() { infoWidget(); });
  }

  // TODO(franzero): Add refresh button
  if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
    sendToSrun(RequestInfo{});
    toWaiting("Getting user info...", false, [this]() { infoWidget(); });
  }
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Not implemented yet");
    ImGui::EndTooltip();
  }
}

constexpr auto Ui::popupId(Ui::PopupType type) -> const char* {
  switch (type) {
    case PopupType::Error:
      return "Error";
    case PopupType::Warning:
      return "Warning";
    case PopupType::Info:
      return "Info";
    default:
      return "Unknown";
  }
}

auto Ui::validConfig(const Config& config) -> std::optional<std::string> {
  if (config.protocol.empty()) {
    return "Protocol is empty";
  }

  if (config.host.empty()) {
    return "Host is empty";
  }

  if (config.port.empty()) {
    return "Port is empty";
  }

  if (config.username.empty()) {
    return "Username is empty";
  }

  if (config.password.empty()) {
    return "Password is empty";
  }

  if (!config.auto_ip && config.ip.empty()) {
    return "IP is empty";
  }

  if (!config.auto_ac_id && config.ac_id == 0) {
    return "AC ID is empty";
  }

  return std::nullopt;
}

auto Ui::toWaiting(std::string_view overlap, bool waiting_enable_cancel,
                   std::function<void()> disable_widget) -> void {
  waiting_overlay_text = overlap;
  waiting_enable_cancel = waiting_enable_cancel;
  waiting_widget = std::move(disable_widget);
  transitState(&Ui::drawWait);
}

}  // namespace srun_gui
