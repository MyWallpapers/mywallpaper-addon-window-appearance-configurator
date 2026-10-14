#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace appearance {
using Json = nlohmann::json;
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
const Json& Manifest();
const Json& Profiles();
struct Config { Json values; };
Config ParseConfig(const Json& snapshot);
Json ResolveStyle(const Config& config, const std::string& image, bool dark);
std::vector<std::string> AppList(const std::string& text);
bool Selected(const Config& config, const std::string& image);
bool ProtectedImage(const std::string& image);
uint64_t ProcessBirth(HANDLE process);

struct GraphicsSession {
  ULONG_PTR token{};
  GraphicsSession();
  ~GraphicsSession();
};
struct Pixels { int width{}, height{}; std::vector<uint32_t> data; };
Pixels DrawFrame(int width, int height, int margin, double scale, const Json& style, bool active);
Pixels DrawTitle(int width, int height, double scale, const Json& style, bool active,
  const std::wstring& title, bool maximized, HICON icon = nullptr, int hovered = -1);
void SavePng(const Pixels& pixels, const std::filesystem::path& path);
int ButtonAt(int x, int width, double scale, const Json& style);

struct WindowState {
  HWND hwnd{};
  DWORD pid{};
  uint64_t birth{};
  uint64_t marker{};
  LONG_PTR originalStyle{}, appliedStyle{}, styleMask{};
  LONG_PTR originalEx{}, appliedEx{}, exMask{};
  std::map<DWORD, DWORD> originalAttributes, appliedAttributes;
  std::set<DWORD> unreadableAttributes;
  bool layeredKnown{}, layeredTouched{};
  COLORREF originalKey{}, appliedKey{};
  BYTE originalAlpha{255}, appliedAlpha{255};
  DWORD originalAlphaFlags{}, appliedAlphaFlags{};
  bool regionTouched{}, originalHasRegion{};
  std::vector<RECT> originalRegion, appliedRegion;
  bool placementTouched{};
  WINDOWPLACEMENT originalPlacement{sizeof(WINDOWPLACEMENT)}, appliedPlacement{sizeof(WINDOWPLACEMENT)};
  Json ToJson() const;
  static WindowState FromJson(const Json& value);
  bool SameIdentity() const;
  bool Restore();
};

class Runtime {
 public:
  Runtime(std::filesystem::path journal, bool allowOwnProcess = false);
  ~Runtime();
  void Update(Config config);
  void Tick();
  void Stop();
  Json Diagnostics() const;
  size_t Count() const;
  static void Recover(const std::filesystem::path& journal);
 private:
  struct Entry;
  Config config_;
  std::filesystem::path journal_;
  std::map<HWND, std::unique_ptr<Entry>> entries_;
  bool allowOwn_{}, stopped_{true};
  uint64_t rejected_{}, unsupported_{}, writes_{}, restoreFailures_{};
  std::string lastIssue_;
  void Apply(Entry& entry, const Json& style, bool active);
  void Persist();
  void Drop(HWND hwnd);
};

std::optional<Json> ReadRecord(HANDLE input);
void WriteRecord(HANDLE output, const Json& record);
void ValidateInit(const Json& frame);
bool StartGuardian(const std::filesystem::path& journal);
int GuardianMain(DWORD owner, uint64_t birth, const std::filesystem::path& journal, const std::wstring& eventName);
}
