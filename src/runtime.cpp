#include "appearance.hpp"
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace appearance {
namespace {
constexpr DWORD kDark=20,kCorner=33,kBorder=34,kCaption=35,kText=36,kMaterial=38;
constexpr LONG_PTR kStyleMask=WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
constexpr wchar_t kMarker[]=L"MyWallpaper.Appearance.OwnerToken";
Json RectJson(const RECT& r) { return Json::array({r.left,r.top,r.right,r.bottom}); }
RECT RectValue(const Json& v) { if(!v.is_array()||v.size()!=4)throw std::runtime_error("Rectangle invalide."); return {v[0].get<LONG>(),v[1].get<LONG>(),v[2].get<LONG>(),v[3].get<LONG>()}; }
bool SameRect(const RECT& a,const RECT& b) { return EqualRect(&a,&b)!=FALSE; }
Json PlacementJson(const WINDOWPLACEMENT& p) { return {{"show",p.showCmd},{"flags",p.flags},{"min",{p.ptMinPosition.x,p.ptMinPosition.y}},{"max",{p.ptMaxPosition.x,p.ptMaxPosition.y}},{"rect",RectJson(p.rcNormalPosition)}}; }
WINDOWPLACEMENT PlacementValue(const Json& v) { WINDOWPLACEMENT p{sizeof(p)}; p.showCmd=v.at("show"); p.flags=v.at("flags"); p.ptMinPosition={v.at("min")[0],v.at("min")[1]}; p.ptMaxPosition={v.at("max")[0],v.at("max")[1]}; p.rcNormalPosition=RectValue(v.at("rect")); return p; }
std::vector<RECT> RegionRects(HRGN r) {
  const DWORD bytes=GetRegionData(r,0,nullptr);
  if(bytes<sizeof(RGNDATAHEADER)||bytes>1024*1024)throw std::runtime_error("Région de fenêtre trop complexe.");
  std::vector<BYTE> buffer(bytes);
  auto* data=reinterpret_cast<RGNDATA*>(buffer.data());
  if(!GetRegionData(r,bytes,data)||data->rdh.nCount>16384)throw std::runtime_error("Région de fenêtre indisponible.");
  const auto* first=reinterpret_cast<const RECT*>(data->Buffer);
  return {first,first+data->rdh.nCount};
}
HRGN MakeRegion(const std::vector<RECT>& rectangles) {
  HRGN result=CreateRectRgn(0,0,0,0);
  for(const auto& r:rectangles) { HRGN part=CreateRectRgnIndirect(&r); if(!part||!CombineRgn(result,result,part,RGN_OR)) {if(part)DeleteObject(part);DeleteObject(result);return nullptr;} DeleteObject(part); }
  return result;
}
bool SameRegion(HWND hwnd,const std::vector<RECT>& expected) {
  HRGN current=CreateRectRgn(0,0,0,0), wanted=MakeRegion(expected);
  const bool equal=wanted&&GetWindowRgn(hwnd,current)!=ERROR&&EqualRgn(current,wanted);
  DeleteObject(current);if(wanted)DeleteObject(wanted);return equal;
}
bool SetStyle(HWND hwnd,int index,LONG_PTR value) { SetLastError(0); const auto previous=SetWindowLongPtrW(hwnd,index,value); return previous!=0||GetLastError()==0; }
DWORD Rgb(const Json& value) { const auto rgb=std::stoul(value.get<std::string>().substr(1),nullptr,16); return RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255); }
bool ThemeDark() { DWORD light=1,bytes=sizeof(light); RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes); return light==0; }
std::string ImageName(HANDLE process) { wchar_t buffer[32768]; DWORD size=32768; if(!QueryFullProcessImageNameW(process,0,buffer,&size))return {}; auto name=Utf8(std::filesystem::path(std::wstring(buffer,size)).filename().wstring()); std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return name; }
std::wstring Title(HWND hwnd) { wchar_t text[1024]{}; GetWindowTextW(hwnd,text,1024); return text; }
std::wstring Class(HWND hwnd) { wchar_t text[256]{}; GetClassNameW(hwnd,text,256); return text; }
bool HiddenOrCloaked(HWND hwnd) { DWORD cloaked=0; return !IsWindowVisible(hwnd)||IsIconic(hwnd)||(SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked); }
bool MatchesRule(const Json& rule,const std::string& image,const std::wstring& cls,const std::wstring& title) {
  return rule["app"]==image&&(!rule.contains("class")||Wide(rule["class"])==cls)&&(!rule.contains("titleContains")||title.find(Wide(rule["titleContains"]))!=std::wstring::npos);
}
class Surface {
 public:
  HWND hwnd{},target{};
  bool title{};
  Json style;
  double scale=1;
  bool active{},maximized{};
  int width{},height{},hovered=-1;
  int pressed=-1;
  std::wstring text;
  HICON icon{};
  Surface(HWND owner,bool isTitle):target(owner),title(isTitle) {
    static ATOM registered=[] { WNDCLASSEXW c{sizeof(c)};c.style=CS_DBLCLKS;c.lpfnWndProc=&Proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"MyWallpaper.Appearance.Decoration";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);return RegisterClassExW(&c); }();
    if(!registered)throw std::runtime_error("Classe de décoration indisponible.");
    hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|(title?0:WS_EX_TRANSPARENT),L"MyWallpaper.Appearance.Decoration",L"",WS_POPUP,0,0,1,1,title?owner:nullptr,nullptr,GetModuleHandleW(nullptr),this);
    if(!hwnd)throw std::runtime_error("Impossible de créer la décoration.");
  }
  ~Surface() { if(hwnd&&GetWindowLongPtrW(hwnd,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(this))DestroyWindow(hwnd); }
  void Paint(const Pixels& pixels,int x,int y,HWND insertAfter) {
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=pixels.width;info.bmiHeader.biHeight=-pixels.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap||!dc) { if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);ReleaseDC(nullptr,screen);throw std::runtime_error("Rendu de décoration indisponible."); }
    std::copy(pixels.data.begin(),pixels.data.end(),static_cast<uint32_t*>(bits));
    const auto old=SelectObject(dc,bitmap);
    POINT position{x,y},source{};SIZE size{pixels.width,pixels.height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    const BOOL updated=UpdateLayeredWindow(hwnd,screen,&position,&size,dc,&source,0,&blend,ULW_ALPHA);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    if(!updated)throw std::runtime_error("Échec de composition de la décoration.");
    width=pixels.width;height=pixels.height;
    SetWindowPos(hwnd,insertAfter,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
  }
  void RefreshTitle() { RECT r{};GetWindowRect(hwnd,&r);Paint(DrawTitle(width,height,scale,style,active,text,maximized,icon,hovered),r.left,r.top,GetWindow(hwnd,GW_HWNDPREV)); }
  static LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_NCCREATE)SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    auto* self=reinterpret_cast<Surface*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(!self)return DefWindowProcW(hwnd,message,wp,lp);
    if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(message==WM_NCHITTEST&&!self->title)return HTTRANSPARENT;
    if(message==WM_MOUSEMOVE&&self->title) {
      const int button=ButtonAt(GET_X_LPARAM(lp),self->width,self->scale,self->style);
      if(button!=self->hovered) {self->hovered=button;try{self->RefreshTitle();}catch(...){}}
      TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);return 0;
    }
    if(message==WM_MOUSELEAVE&&self->title) {self->hovered=-1;try{self->RefreshTitle();}catch(...){}return 0;}
    if(message==WM_LBUTTONDOWN&&self->title) {
      const int button=ButtonAt(GET_X_LPARAM(lp),self->width,self->scale,self->style);
      if(button>=0) {self->pressed=button;SetCapture(hwnd);return 0;}
      ReleaseCapture();SetForegroundWindow(self->target);POINT cursor{};GetCursorPos(&cursor);PostMessageW(self->target,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(cursor.x,cursor.y));return 0;
    }
    if(message==WM_LBUTTONUP&&self->title) {
      const bool captured=GetCapture()==hwnd;const int pressed=self->pressed;ReleaseCapture();self->pressed=-1;
      const int button=ButtonAt(GET_X_LPARAM(lp),self->width,self->scale,self->style);
      if(captured&&button>=0&&button==pressed)PostMessageW(self->target,WM_SYSCOMMAND,button==0?SC_MINIMIZE:button==1?(IsZoomed(self->target)?SC_RESTORE:SC_MAXIMIZE):SC_CLOSE,0);
      return 0;
    }
    if(message==WM_LBUTTONDBLCLK&&self->title&&ButtonAt(GET_X_LPARAM(lp),self->width,self->scale,self->style)<0) {PostMessageW(self->target,WM_SYSCOMMAND,IsZoomed(self->target)?SC_RESTORE:SC_MAXIMIZE,0);return 0;}
    if(message==WM_CAPTURECHANGED)self->pressed=-1;
    if(message==WM_RBUTTONUP&&self->title) {POINT p{};GetCursorPos(&p);PostMessageW(self->target,WM_SYSCOMMAND,SC_MOUSEMENU,MAKELPARAM(p.x,p.y));return 0;}
    return DefWindowProcW(hwnd,message,wp,lp);
  }
};
}

Json WindowState::ToJson() const {
  Json attrs=Json::array();for(const auto& [key,value]:originalAttributes)attrs.push_back({{"key",key},{"original",value},{"applied",appliedAttributes.at(key)}});
  Json originalR=Json::array(),appliedR=Json::array();for(const auto& r:originalRegion)originalR.push_back(RectJson(r));for(const auto& r:appliedRegion)appliedR.push_back(RectJson(r));
  return {{"hwnd",reinterpret_cast<uint64_t>(hwnd)},{"pid",pid},{"birth",birth},{"marker",marker},{"unreadableAttributes",unreadableAttributes},{"style",{originalStyle,appliedStyle,styleMask}},
    {"ex",{originalEx,appliedEx,exMask}},{"attributes",attrs},{"layeredKnown",layeredKnown},{"layeredTouched",layeredTouched},
    {"layered",{originalKey,originalAlpha,originalAlphaFlags,appliedKey,appliedAlpha,appliedAlphaFlags}},
    {"regionTouched",regionTouched},{"originalHasRegion",originalHasRegion},{"originalRegion",originalR},{"appliedRegion",appliedR},
    {"placementTouched",placementTouched},{"originalPlacement",PlacementJson(originalPlacement)},{"appliedPlacement",PlacementJson(appliedPlacement)}};
}
WindowState WindowState::FromJson(const Json& v) {
  WindowState s; s.hwnd=reinterpret_cast<HWND>(v.at("hwnd").get<uint64_t>());s.pid=v.at("pid");s.birth=v.at("birth");
  s.marker=v.at("marker");s.unreadableAttributes=v.at("unreadableAttributes").get<std::set<DWORD>>();
  s.originalStyle=v.at("style")[0];s.appliedStyle=v.at("style")[1];s.styleMask=v.at("style")[2].get<LONG_PTR>()&kStyleMask;
  s.originalEx=v.at("ex")[0];s.appliedEx=v.at("ex")[1];s.exMask=v.at("ex")[2].get<LONG_PTR>()&WS_EX_LAYERED;
  for(const auto& a:v.at("attributes")) {const DWORD key=a.at("key");if(key!=kDark&&key!=kCorner&&key!=kBorder&&key!=kCaption&&key!=kText&&key!=kMaterial)throw std::runtime_error("Attribut de récupération inconnu.");s.originalAttributes[key]=a.at("original");s.appliedAttributes[key]=a.at("applied");}
  s.layeredKnown=v.at("layeredKnown");s.layeredTouched=v.at("layeredTouched");s.originalKey=v.at("layered")[0];s.originalAlpha=v.at("layered")[1];s.originalAlphaFlags=v.at("layered")[2];s.appliedKey=v.at("layered")[3];s.appliedAlpha=v.at("layered")[4];s.appliedAlphaFlags=v.at("layered")[5];
  s.regionTouched=v.at("regionTouched");s.originalHasRegion=v.at("originalHasRegion");
  if(v.at("originalRegion").size()>16384||v.at("appliedRegion").size()>16384)throw std::runtime_error("Journal de régions trop grand.");
  for(const auto& r:v.at("originalRegion"))s.originalRegion.push_back(RectValue(r));for(const auto& r:v.at("appliedRegion"))s.appliedRegion.push_back(RectValue(r));
  s.placementTouched=v.at("placementTouched");s.originalPlacement=PlacementValue(v.at("originalPlacement"));s.appliedPlacement=PlacementValue(v.at("appliedPlacement"));return s;
}
bool WindowState::SameIdentity() const {
  DWORD current=0;GetWindowThreadProcessId(hwnd,&current);if(current!=pid||!IsWindow(hwnd)||birth==0||reinterpret_cast<uint64_t>(GetPropW(hwnd,kMarker))!=marker)return false;
  HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;
  const bool same=ProcessBirth(process)==birth;CloseHandle(process);return same;
}
bool WindowState::Restore() {
  if(!IsWindow(hwnd))return true;
  if(!SameIdentity())return true; // Never restore a recycled HWND / process object.
  bool ok=true,frameChanged=false;
  for(const auto& [key,applied]:appliedAttributes) {DWORD current{};if(unreadableAttributes.contains(key)||(SUCCEEDED(DwmGetWindowAttribute(hwnd,key,&current,sizeof(current)))&&current==applied)) {const auto original=originalAttributes.at(key);ok=SUCCEEDED(DwmSetWindowAttribute(hwnd,key,&original,sizeof(original)))&&ok;}}
  if(regionTouched&&SameRegion(hwnd,appliedRegion)) {HRGN original=originalHasRegion?MakeRegion(originalRegion):nullptr;const bool restored=SetWindowRgn(hwnd,original,TRUE)!=0;if(!restored&&original)DeleteObject(original);ok=restored&&ok;}
  if(layeredTouched) {
    COLORREF key{};BYTE alpha{};DWORD flags{};
    if(GetLayeredWindowAttributes(hwnd,&key,&alpha,&flags)&&key==appliedKey&&alpha==appliedAlpha&&flags==appliedAlphaFlags) {
      if(layeredKnown)ok=(SetLayeredWindowAttributes(hwnd,originalKey,originalAlpha,originalAlphaFlags)!=0)&&ok;
      else {const auto current=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);ok=SetStyle(hwnd,GWL_EXSTYLE,(current&~exMask)|(originalEx&exMask))&&ok;}
    }
  }
  if(styleMask) {const auto current=GetWindowLongPtrW(hwnd,GWL_STYLE);if((current&styleMask)==(appliedStyle&styleMask)) {ok=SetStyle(hwnd,GWL_STYLE,(current&~styleMask)|(originalStyle&styleMask))&&ok;frameChanged=true;}}
  if(frameChanged)SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
  if(placementTouched) {WINDOWPLACEMENT current{sizeof(current)};if(GetWindowPlacement(hwnd,&current)&&SameRect(current.rcNormalPosition,appliedPlacement.rcNormalPosition)&&current.showCmd==appliedPlacement.showCmd)ok=(SetWindowPlacement(hwnd,&originalPlacement)!=0)&&ok;}
  RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);if(ok)RemovePropW(hwnd,kMarker);return ok;
}
struct Runtime::Entry {
  WindowState state;
  std::string image;
  std::wstring cls;
  Json lastStyle;
  RECT lastRect{};
  std::wstring lastTitle;
  bool lastActive{},lastMaximized{};
  UINT lastDpi{};
  std::unique_ptr<Surface> frame,title;
};
Runtime::Runtime(std::filesystem::path journal,bool allowOwnProcess):journal_(std::move(journal)),allowOwn_(allowOwnProcess) {}
Runtime::~Runtime() {try{Stop();}catch(...){}}
void Runtime::Persist() {
  if(journal_.empty())return;
  Json states=Json::array();for(const auto& [hwnd,e]:entries_)states.push_back(e->state.ToJson());
  const auto text=Json({{"schemaVersion",1},{"windows",states}}).dump();
  auto temp=journal_;temp+=L".tmp";
  {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(text.data(),static_cast<std::streamsize>(text.size()));file.flush();if(!file)throw std::runtime_error("Impossible de sauvegarder le journal de restauration.");}
  bool moved=false;DWORD lastError=0;
  for(int attempt=0;attempt<8;++attempt) {
    if(MoveFileExW(temp.c_str(),journal_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {moved=true;break;}
    lastError=GetLastError();
    if(lastError!=ERROR_SHARING_VIOLATION&&lastError!=ERROR_ACCESS_DENIED)break;
    Sleep(20);
  }
  if(!moved)throw std::runtime_error("Impossible de valider le journal de restauration (Windows "+std::to_string(lastError)+").");
}
void Runtime::Drop(HWND hwnd) {
  const auto found=entries_.find(hwnd);if(found==entries_.end())return;
  auto& e=*found->second;e.frame.reset();e.title.reset();
  if(!e.state.Restore()) {++restoreFailures_;lastIssue_="Restauration refusée pour une fenêtre ; le gardien réessaiera.";return;}
  entries_.erase(found);Persist();
}
void Runtime::Stop() {
  stopped_=true;
  std::vector<HWND> handles;for(const auto& [hwnd,e]:entries_)handles.push_back(hwnd);
  for(auto hwnd:handles)Drop(hwnd);
  Persist();
  if(entries_.empty()&&!journal_.empty()) {std::error_code error;std::filesystem::remove(journal_,error);}
}
void Runtime::Update(Config config) {Stop();if(!entries_.empty())throw std::runtime_error("Attendez la restauration des fenêtres avant de changer de profil.");config_=std::move(config);stopped_=false;Tick();}
size_t Runtime::Count() const {return entries_.size();}
Json Runtime::Diagnostics() const {return {{"managedWindows",entries_.size()},{"attributeWrites",writes_},{"unsupportedOperations",unsupported_},{"rejectedWindows",rejected_},{"restoreFailures",restoreFailures_},{"lastIssue",lastIssue_},{"mechanism","external-win32-dwm-and-decoration"}};}
void Runtime::Recover(const std::filesystem::path& journal) {
  std::ifstream file(journal,std::ios::binary);if(!file)return;
  const Json document=Json::parse(file);
  file.close();
  if(document.value("schemaVersion",0)!=1||!document.contains("windows")||!document["windows"].is_array()||document["windows"].size()>256)throw std::runtime_error("Journal de restauration invalide.");
  bool complete=true;for(const auto& value:document["windows"]) {auto state=WindowState::FromJson(value);complete=state.Restore()&&complete;}
  if(complete) {std::error_code error;std::filesystem::remove(journal,error);}
}
void Runtime::Tick() {
  if(stopped_)return;
  std::vector<HWND> windows;
  EnumWindows([](HWND hwnd,LPARAM p)->BOOL {static_cast<std::vector<HWND>*>(reinterpret_cast<void*>(p))->push_back(hwnd);return TRUE;},reinterpret_cast<LPARAM>(&windows));
  std::set<HWND> wanted;
  const bool dark=config_.values["theme"]=="dark"||(config_.values["theme"]=="auto"&&ThemeDark());
  const HWND foreground=GetForegroundWindow();
  for(HWND hwnd:windows) {
    if(HiddenOrCloaked(hwnd))continue;
    const auto cls=Class(hwnd);
    if(cls==L"Shell_TrayWnd"||cls==L"Shell_SecondaryTrayWnd"||cls==L"Progman"||cls==L"WorkerW"||cls==L"MyWallpaper.Appearance.Decoration")continue;
    const auto flags=GetWindowLongPtrW(hwnd,GWL_STYLE),ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    if((flags&WS_CHILD)||(ex&WS_EX_TOOLWINDOW))continue;
    if(!config_.values["includeDialogs"].get<bool>()&&GetWindow(hwnd,GW_OWNER))continue;
    DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);
    if(pid==GetCurrentProcessId()&&!allowOwn_)continue;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)continue;
    const auto birth=ProcessBirth(process);const auto image=ImageName(process);CloseHandle(process);
    if(birth==0||!Selected(config_,image))continue;
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    if(config_.values["monitor"]=="primary"&&!(monitor.dwFlags&MONITORINFOF_PRIMARY))continue;
    if(config_.values["monitor"]=="secondary"&&(monitor.dwFlags&MONITORINFOF_PRIMARY))continue;
    auto style=ResolveStyle(config_,image,dark);const auto title=Title(hwnd);bool excluded=false;
    for(const auto& rule:config_.values["_advanced"]) {
      if(!MatchesRule(rule,image,cls,title))continue;
      if(rule.value("exclude",false)) {excluded=true;break;}
      if(rule.contains("profile")) {style=config_.values;style.update(Profiles().at(rule["profile"].get<std::string>()).at("dark"));if(!dark)style.update(Profiles().at(rule["profile"].get<std::string>()).at("light"));style["_dark"]=dark;}
      if(rule.contains("opacity")) {style["windowOpacity"]=rule["opacity"];style["inactiveOpacity"]=rule["opacity"];}
      break;
    }
    if(excluded)continue;
    if(entries_.size()>=128&&!entries_.contains(hwnd)) {++rejected_;lastIssue_="La limite de 128 fenêtres décorées est atteinte.";continue;}
    if(auto old=entries_.find(hwnd);old!=entries_.end()&&(old->second->state.pid!=pid||old->second->state.birth!=birth))Drop(hwnd);
    if(!entries_.contains(hwnd)) {
      if(GetPropW(hwnd,kMarker)) {++rejected_;lastIssue_="Cette fenêtre attend la restauration d’un ancien companion.";continue;}
      auto entry=std::make_unique<Entry>();entry->state.hwnd=hwnd;entry->state.pid=pid;entry->state.birth=birth;entry->image=image;entry->cls=cls;
      static uint64_t sequence=0;entry->state.marker=ProcessBirth(GetCurrentProcess())+(++sequence);
      if(!SetPropW(hwnd,kMarker,reinterpret_cast<HANDLE>(entry->state.marker))) {++rejected_;continue;}
      entry->state.originalStyle=entry->state.appliedStyle=flags;entry->state.originalEx=entry->state.appliedEx=ex;
      GetWindowPlacement(hwnd,&entry->state.originalPlacement);
      if(ex&WS_EX_LAYERED)entry->state.layeredKnown=GetLayeredWindowAttributes(hwnd,&entry->state.originalKey,&entry->state.originalAlpha,&entry->state.originalAlphaFlags)!=FALSE;
      entries_[hwnd]=std::move(entry);
    }
    wanted.insert(hwnd);
    const bool active=foreground==hwnd||GetAncestor(foreground,GA_ROOTOWNER)==hwnd;
    try {Apply(*entries_.at(hwnd),style,active);}catch(const std::exception& error) {lastIssue_=error.what();++rejected_;Drop(hwnd);wanted.erase(hwnd);}
  }
  std::vector<HWND> stale;for(const auto& [hwnd,e]:entries_)if(!wanted.contains(hwnd))stale.push_back(hwnd);
  for(auto hwnd:stale)Drop(hwnd);
}
void Runtime::Apply(Entry& e,const Json& style,bool active) {
  auto& state=e.state;const HWND hwnd=state.hwnd;
  if(!state.SameIdentity())throw std::runtime_error("L’identité de la fenêtre a changé.");
  RECT rect{};if(!GetWindowRect(hwnd,&rect))return;
  UINT dpi=GetDpiForWindow(hwnd);if(!dpi)dpi=96;const double scale=dpi/96.0;
  const bool maximized=IsZoomed(hwnd)||style["frameMode"]=="fullscreen";
  const auto title=Title(hwnd);
  const bool change=e.lastStyle!=style||e.lastActive!=active||e.lastDpi!=dpi||e.lastMaximized!=maximized;
  // Failed attributes are counted once per observed state change, never per poll.
  if(change) {
    auto attribute=[&](DWORD key,DWORD value) {
      DWORD current{};
      if(FAILED(DwmGetWindowAttribute(hwnd,key,&current,sizeof(current)))) {
        if(!style["restoreNativeDefaults"].get<bool>()) {++unsupported_;return;}
        current=key==kBorder||key==kCaption||key==kText?0xffffffffu:0;
        state.unreadableAttributes.insert(key);
        if(state.appliedAttributes.contains(key)&&state.appliedAttributes.at(key)==value)return;
      }
      if(current==value)return;
      const bool recorded=state.originalAttributes.contains(key);
      const DWORD previous=recorded?state.appliedAttributes.at(key):current;
      if(!recorded)state.originalAttributes[key]=current;
      state.appliedAttributes[key]=value;Persist();
      if(SUCCEEDED(DwmSetWindowAttribute(hwnd,key,&value,sizeof(value))))++writes_;
      else {++unsupported_;if(recorded)state.appliedAttributes[key]=previous;else {state.originalAttributes.erase(key);state.appliedAttributes.erase(key);state.unreadableAttributes.erase(key);}Persist();}
    };
    attribute(kDark,style["_dark"].get<bool>()?1:0);
    if(style["nativeMaterial"]!="system")attribute(kMaterial,style["nativeMaterial"]=="opaque"?1:style["nativeMaterial"]=="mica"?2:style["nativeMaterial"]=="acrylic"?3:4);
    if(style["nativeColors"].get<bool>()) {attribute(kCaption,Rgb(active?style["captionColor"]:style["inactiveCaption"]));attribute(kText,Rgb(active?style["textColor"]:style["inactiveText"]));}
    if(style["borderMode"]!="system")attribute(kBorder,style["borderMode"]=="none"?0xFFFFFFFEu:Rgb(active?style["borderColor"]:style["inactiveBorder"]));
    if(style["corner"]!="system")attribute(kCorner,style["corner"]=="square"||style["corner"]=="custom"?1:style["corner"]=="small"?3:2);
    LONG_PTR mask=0,desired=GetWindowLongPtrW(hwnd,GWL_STYLE);
    const auto mode=style["frameMode"].get<std::string>();
    if(mode=="native") {mask=WS_CAPTION|WS_SYSMENU|WS_THICKFRAME;desired|=WS_CAPTION|WS_SYSMENU;if(style["nativeResize"].get<bool>())desired|=WS_THICKFRAME;else desired&=~WS_THICKFRAME;}
    if(mode=="borderless"||mode=="fullscreen") {mask=kStyleMask;desired&=~mask;}
    if(style["titleMode"]=="companion"||style["titleMode"]=="hidden") {
      mask|=WS_CAPTION;
      if(maximized&&style["preserveMaximized"].get<bool>()&&mode!="fullscreen")desired=(desired&~WS_CAPTION)|(state.originalStyle&WS_CAPTION);
      else desired&=~WS_CAPTION;
    }
    if(mask) {state.styleMask|=mask;state.appliedStyle=desired;Persist();if(!SetStyle(hwnd,GWL_STYLE,desired))throw std::runtime_error("L’application refuse ce cadre.");SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);}
    if(mode=="fullscreen"&&!state.placementTouched) {
      MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
      state.placementTouched=true;state.appliedPlacement=state.originalPlacement;state.appliedPlacement.showCmd=SW_SHOWNORMAL;state.appliedPlacement.rcNormalPosition=monitor.rcMonitor;Persist();
      ShowWindow(hwnd,SW_SHOWNOACTIVATE);SetWindowPos(hwnd,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_NOACTIVATE|SWP_NOZORDER);
      GetWindowPlacement(hwnd,&state.appliedPlacement);Persist();
    }
    const auto opacity=(active?style["windowOpacity"]:style["inactiveOpacity"]).get<int>();
    const auto currentEx=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    if(opacity<100||state.layeredTouched) {
      if((state.originalEx&WS_EX_LAYERED)&&!state.layeredKnown) {++unsupported_;lastIssue_="La transparence existante de cette application a été préservée.";}
      else {
        state.layeredTouched=true;state.exMask=WS_EX_LAYERED;state.appliedEx=currentEx|WS_EX_LAYERED;state.appliedAlpha=static_cast<BYTE>(opacity*255/100);state.appliedKey=state.originalKey;state.appliedAlphaFlags=(state.layeredKnown?state.originalAlphaFlags:0)|LWA_ALPHA;Persist();
        if(!SetStyle(hwnd,GWL_EXSTYLE,state.appliedEx)||!SetLayeredWindowAttributes(hwnd,state.appliedKey,state.appliedAlpha,state.appliedAlphaFlags)) {
          if(!state.layeredKnown) {const auto now=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);if((now&WS_EX_LAYERED)==(state.appliedEx&WS_EX_LAYERED))SetStyle(hwnd,GWL_EXSTYLE,(now&~WS_EX_LAYERED)|(state.originalEx&WS_EX_LAYERED));}
          state.layeredTouched=false;Persist();
          throw std::runtime_error("La transparence de cette fenêtre n’est pas accessible.");
        }
      }
    }
  }
  GetWindowRect(hwnd,&rect);
  if(style["corner"]=="custom"&&!(maximized&&style["preserveMaximized"].get<bool>())) {
    if(change||!SameRect(rect,e.lastRect)) {
      if(!state.regionTouched) {HRGN original=CreateRectRgn(0,0,0,0);state.originalHasRegion=GetWindowRgn(hwnd,original)!=ERROR;if(state.originalHasRegion)state.originalRegion=RegionRects(original);DeleteObject(original);}
      const int diameter=static_cast<int>(style["cornerRadius"].get<double>()*scale*2);
      HRGN region=diameter?CreateRoundRectRgn(0,0,rect.right-rect.left+1,rect.bottom-rect.top+1,diameter,diameter):CreateRectRgn(0,0,rect.right-rect.left,rect.bottom-rect.top);
      state.regionTouched=true;state.appliedRegion=RegionRects(region);Persist();
      if(!SetWindowRgn(hwnd,region,TRUE)) {DeleteObject(region);throw std::runtime_error("L’application refuse la découpe des coins.");}
    }
  } else if(state.regionTouched) {
    if(SameRegion(hwnd,state.appliedRegion)) {HRGN original=state.originalHasRegion?MakeRegion(state.originalRegion):nullptr;if(!SetWindowRgn(hwnd,original,TRUE)){if(original)DeleteObject(original);throw std::runtime_error("Impossible de restaurer la région de fenêtre.");}}
    state.regionTouched=false;Persist();
  }
  const bool companionTitle=style["titleMode"]=="companion"&&style["frameMode"]!="fullscreen"&&!(maximized&&style["preserveMaximized"].get<bool>());
  RECT visual=rect;DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&visual,sizeof(visual));
  const int titleHeight=companionTitle?static_cast<int>(style["titleHeight"].get<double>()*scale):0;
  if(!companionTitle)e.title.reset();
  const bool paint=change||!SameRect(rect,e.lastRect)||e.lastTitle!=title;
  const bool frameNeeded=(style["shadowMode"]=="soft"||style["borderMode"]=="gradient"||style["borderMode"]=="solid")&&!(maximized&&style["preserveMaximized"].get<bool>());
  if(!frameNeeded)e.frame.reset();
  if(paint) {
    if(frameNeeded) {
      if(!e.frame)e.frame=std::make_unique<Surface>(hwnd,false);
      const int margin=static_cast<int>((std::max(style["shadowSize"].get<double>()+std::abs(style["shadowOffset"].get<double>()),style["borderWidth"].get<double>())+2)*scale);
      auto pixels=DrawFrame(visual.right-visual.left+margin*2,visual.bottom-visual.top+titleHeight+margin*2,margin,scale,style,active);
      e.frame->Paint(pixels,visual.left-margin,visual.top-titleHeight-margin,hwnd);
    }
    if(companionTitle) {
      if(!e.title)e.title=std::make_unique<Surface>(hwnd,true);
      e.title->style=style;e.title->scale=scale;e.title->active=active;e.title->maximized=maximized;e.title->text=title;
      auto icon=reinterpret_cast<HICON>(GetClassLongPtrW(hwnd,GCLP_HICONSM));if(!icon)icon=reinterpret_cast<HICON>(GetClassLongPtrW(hwnd,GCLP_HICON));e.title->icon=icon;
      HWND previous=GetWindow(hwnd,GW_HWNDPREV);if(previous==e.title->hwnd)previous=GetWindow(e.title->hwnd,GW_HWNDPREV);
      e.title->Paint(DrawTitle(visual.right-visual.left,titleHeight,scale,style,active,title,maximized,icon),visual.left,visual.top-titleHeight,previous?previous:HWND_TOP);
    }
  } else {
    // Keep z-order with the target without bringing decoration above other apps.
    if(e.frame)SetWindowPos(e.frame->hwnd,hwnd,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  }
  e.lastStyle=style;e.lastRect=rect;e.lastActive=active;e.lastDpi=dpi;e.lastMaximized=maximized;e.lastTitle=title;
}
}
