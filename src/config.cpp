#include "appearance.hpp"
#include "settings_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <stdexcept>

namespace appearance {
std::wstring Wide(const std::string& text) {
  if (text.empty()) return {};
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (!count) throw std::runtime_error("Texte UTF-8 invalide.");
  std::wstring result(count, 0);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count);
  return result;
}
std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  std::string result(count, 0);
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
  return result;
}
const Json& Manifest() { static const auto value = Json::parse(kManifest); return value; }
const Json& Profiles() { static const auto value = Json::parse(kProfiles); return value; }
static std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}
std::vector<std::string> AppList(const std::string& text) {
  if (text.size() > 16384) throw std::runtime_error("La liste d’applications est trop longue.");
  std::vector<std::string> result;
  size_t begin = 0;
  while (begin < text.size()) {
    auto end = text.find('\n', begin);
    if (end == std::string::npos) end = text.size();
    auto app = text.substr(begin, end - begin);
    const auto left = app.find_first_not_of(" \r\t");
    if (left != std::string::npos) {
      app = Lower(app.substr(left, app.find_last_not_of(" \r\t") - left + 1));
      if (app.size() > 256 || !app.ends_with(".exe") || app.find_first_of("/\\:*?\"<>|") != std::string::npos)
        throw std::runtime_error("Utilisez un nom .exe exact par ligne, sans chemin ni joker.");
      Wide(app);
      if (std::find(result.begin(), result.end(), app) == result.end()) result.push_back(app);
    }
    begin = end + 1;
  }
  if (result.size() > 128) throw std::runtime_error("Une liste accepte au maximum 128 applications.");
  return result;
}
bool ProtectedImage(const std::string& image) {
  static const std::set<std::string> protectedImages = {"dwm.exe","winlogon.exe","logonui.exe","consent.exe","lsass.exe",
    "csrss.exe","smss.exe","services.exe","svchost.exe","codex.exe","mywallpaper.exe","companion.exe"};
  return protectedImages.contains(Lower(image));
}
Config ParseConfig(const Json& snapshot) {
  if (!snapshot.is_object() || !snapshot.contains("deviceSettings") || !snapshot["deviceSettings"].is_object())
    throw std::runtime_error("Un snapshot deviceSettings complet est requis.");
  if (snapshot.contains("layerSettings") && (!snapshot["layerSettings"].is_object() || !snapshot["layerSettings"].empty()))
    throw std::runtime_error("Cet add-on utilise uniquement des réglages de l’appareil.");
  Config result{Json::object()};
  std::map<std::string,Json> definitions;
  for (const auto& setting : Manifest()["settings"]) {
    if (setting["type"] == "section") continue;
    const auto key = setting["id"].get<std::string>();
    definitions[key] = setting;
    result.values[key] = setting["default"];
  }
  for (const auto& [key,value] : snapshot["deviceSettings"].items()) {
    const auto found = definitions.find(key);
    if (found == definitions.end()) throw std::runtime_error("Réglage inconnu : " + key);
    const auto& d = found->second;
    const auto type = d["type"].get<std::string>();
    bool valid = false;
    if (type == "boolean") valid = value.is_boolean();
    else if (type == "range") {
      if (value.is_number()) {
        const double number = value.get<double>();
        valid = std::isfinite(number) && number >= d["min"].get<double>() && number <= d["max"].get<double>() &&
          std::abs(number - std::round(number)) < 1e-8;
      }
    } else if (value.is_string()) {
      const auto text = value.get<std::string>();
      Wide(text);
      if (type == "select") valid = std::any_of(d["options"].begin(), d["options"].end(), [&](const Json& option) { return option["value"] == value; });
      else if (type == "color") valid = text.size() == 7 && text[0] == '#' && std::all_of(text.begin()+1,text.end(),[](unsigned char c) { return std::isxdigit(c) != 0; });
      else valid = text.size() <= (type == "textarea" ? 65536u : 256u) && text.find('\0') == std::string::npos;
    }
    if (!valid) throw std::runtime_error("Valeur non prise en charge pour « " + d["label"].get<std::string>() + " ».");
    result.values[key] = value;
  }
  for (const char* key : {"targetApps","excludedApps","rule1Apps","rule2Apps","rule3Apps"}) AppList(result.values[key]);
  result.values["_advanced"] = Json::array();
  const auto advanced = result.values["advancedRules"].get<std::string>();
  if (!advanced.empty()) {
    const auto document = Json::parse(advanced);
    if (!document.is_object() || document.value("schemaVersion",0) != 1 || !document.contains("rules") || !document["rules"].is_array() || document["rules"].size() > 64)
      throw std::runtime_error("Exceptions : schemaVersion 1 et au maximum 64 règles sont requis.");
    for (const auto& [key,value] : document.items()) if (key != "schemaVersion" && key != "rules") throw std::runtime_error("Clé d’exceptions inconnue : " + key);
    for (auto rule : document["rules"]) {
      if (!rule.is_object() || !rule.contains("app") || !rule["app"].is_string() || AppList(rule["app"]).size() != 1)
        throw std::runtime_error("Chaque exception doit désigner une application .exe exacte.");
      rule["app"] = AppList(rule["app"])[0];
      for (const auto& [key,value] : rule.items()) {
        if (key == "app") continue;
        if (key == "class" || key == "titleContains") { if (!value.is_string() || value.get<std::string>().size() > 256) throw std::runtime_error("Sélecteur d’exception invalide."); Wide(value); }
        else if (key == "profile") { if (!value.is_string() || !Profiles().contains(value.get<std::string>())) throw std::runtime_error("Profil d’exception inconnu."); }
        else if (key == "opacity") { if (!value.is_number_integer() || value.get<int>() < 35 || value.get<int>() > 100) throw std::runtime_error("L’opacité doit être un entier entre 35 et 100."); }
        else if (key == "exclude") { if (!value.is_boolean()) throw std::runtime_error("exclude doit être booléen."); }
        else throw std::runtime_error("Clé d’exception inconnue : " + key);
      }
      result.values["_advanced"].push_back(std::move(rule));
    }
  }
  return result;
}
static bool ContainsApp(const std::string& list, const std::string& image) {
  const auto apps = AppList(list);
  return std::find(apps.begin(),apps.end(),Lower(image)) != apps.end();
}
bool Selected(const Config& config, const std::string& image) {
  const auto& v = config.values;
  if (!v["enabled"].get<bool>() || ProtectedImage(image) || ContainsApp(v["excludedApps"],image)) return false;
  if (ContainsApp(v["targetApps"],image)) return true;
  for (int n=1;n<=3;++n) {
    const auto prefix = "rule" + std::to_string(n);
    if (v[prefix+"Enabled"].get<bool>() && ContainsApp(v[prefix+"Apps"],image)) return true;
  }
  return std::any_of(v["_advanced"].begin(),v["_advanced"].end(),[&](const Json& rule) { return rule["app"] == Lower(image) && !rule.value("exclude",false); });
}
Json ResolveStyle(const Config& config, const std::string& image, bool dark) {
  Json style = config.values;
  auto profile = style["profile"].get<std::string>();
  std::optional<int> opacity;
  for (int n=1;n<=3;++n) {
    const auto prefix = "rule" + std::to_string(n);
    if (style[prefix+"Enabled"].get<bool>() && ContainsApp(style[prefix+"Apps"],image)) {
      profile = style[prefix+"Profile"];
      opacity = style[prefix+"Opacity"].get<int>();
      break;
    }
  }
  if (profile != "custom") {
    style.update(Profiles().at(profile).at("dark"));
    if (!dark) style.update(Profiles().at(profile).at("light"));
  }
  if (opacity) { style["windowOpacity"] = *opacity; style["inactiveOpacity"] = *opacity; }
  style["_dark"] = dark;
  return style;
}
uint64_t ProcessBirth(HANDLE process) {
  FILETIME create{},exit{},kernel{},user{};
  if (!GetProcessTimes(process,&create,&exit,&kernel,&user)) return 0;
  return (static_cast<uint64_t>(create.dwHighDateTime)<<32)|create.dwLowDateTime;
}
GraphicsSession::GraphicsSession() {
  Gdiplus::GdiplusStartupInput options;
  if (Gdiplus::GdiplusStartup(&token,&options,nullptr) != Gdiplus::Ok) throw std::runtime_error("GDI+ indisponible.");
}
GraphicsSession::~GraphicsSession() { if (token) Gdiplus::GdiplusShutdown(token); }
}
