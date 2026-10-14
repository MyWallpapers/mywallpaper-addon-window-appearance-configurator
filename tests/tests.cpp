#include "appearance.hpp"
#include <dwmapi.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace appearance;
namespace {
int checks=0;
void Check(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
template<class F> void Reject(F fn,const char* reason) {bool rejected=false;try{fn();}catch(...){rejected=true;}Check(rejected,reason);}
Json Snapshot(Json values=Json::object()) {return {{"deviceSettings",values},{"layerSettings",Json::object()}};}
void Contract() {
  const auto defaults=ParseConfig(Snapshot());
  Check(Manifest()["settings"].size()<=100,"setting limit");
  Check(!Selected(defaults,"dwm.exe"),"DWM exclusion");
  Check(Selected(defaults,"NOTEPAD.EXE"),"selected application");
  const auto empty=ParseConfig(Snapshot({{"targetApps",""}}));Check(!Selected(empty,"notepad.exe"),"empty selection does nothing");
  Reject([]{ParseConfig(Snapshot({{"windowOpacity",34}}));},"low opacity rejected");
  Reject([]{ParseConfig(Snapshot({{"windowOpacity",80.5}}));},"fraction rejected");
  Reject([]{ParseConfig(Snapshot({{"profile","invented"}}));},"unknown profile rejected");
  Reject([]{ParseConfig(Snapshot({{"surprise",true}}));},"unknown field rejected");
  Reject([]{ParseConfig(Snapshot({{"borderColor","#001122AA"}}));},"alpha not allowed on RGB controls");
  Reject([]{ParseConfig(Snapshot({{"targetApps","*.exe"}}));},"wildcard rejected");
  Reject([]{ParseConfig(Snapshot({{"targetApps","C:\\app.exe"}}));},"path rejected");
  Reject([]{ParseConfig(Snapshot({{"advancedRules","{\"schemaVersion\":1,\"rules\":[{\"app\":\"a.exe\",\"profile\":\"made-up\"}]}"}}));},"unknown advanced profile");
  auto rule=ParseConfig(Snapshot({{"rule1Enabled",true},{"rule1Apps","EDITOR.EXE\neditor.exe\r\n"},{"rule1Profile","ember"},{"rule1Opacity",75},{"rule2Enabled",true},{"rule2Apps","editor.exe"},{"rule2Profile","graphite"}}));
  Check(Selected(rule,"editor.exe"),"rule adds selection");const auto rs=ResolveStyle(rule,"editor.exe",true);Check(rs["titleFont"]=="Georgia"&&rs["windowOpacity"]==75,"first readable rule wins");
  rule=ParseConfig(Snapshot({{"rule1Enabled",true},{"rule1Apps","editor.exe"},{"excludedApps","editor.exe"}}));Check(!Selected(rule,"editor.exe"),"exclusion wins");
  for(const auto& [id,p]:Profiles().items()) {
    auto c=ParseConfig(Snapshot({{"profile",id}}));auto dark=ResolveStyle(c,"notepad.exe",true),light=ResolveStyle(c,"notepad.exe",false);
    Check(dark.contains("titleStart")&&light.contains("borderColor"),"complete profile");
    if(id!="system")Check(dark["captionColor"]!=light["captionColor"],"light and dark differ");
    const auto image=DrawTitle(480,40,1,light,false,L"Un titre très long pour vérifier la troncature sans masquer les boutons",false);
    Check(image.data.size()==480*40,"title render");
  }
  auto custom=ParseConfig(Snapshot({{"profile","custom"},{"borderColor","#AB1234"}}));Check(ResolveStyle(custom,"a.exe",false)["borderColor"]=="#AB1234","custom palette literal");
  Json init={{"type","init"},{"v",5},{"addonId","dev-window-appearance"},{"artifactDigest","sha256:"+std::string(64,'a')},{"version","0.1.0"},{"targetArchitecture","windows-x86_64"},{"scratchDirectory","C:\\Temp\\appearance"},{"deviceSettings",Json::object()},{"layerSettings",Json::object()}};
  ValidateInit(init);init["v"]=4;Reject([&]{ValidateInit(init);},"wrong wire version rejected");
  const auto s=ResolveStyle(defaults,"notepad.exe",true);Check(ButtonAt(479,480,1,s)==2,"close hit area");Check(ButtonAt(0,480,1,s)==-1,"text area is drag area");
  const auto frame=DrawFrame(480,300,30,1,s,true);Check(frame.data[150*480+240]==0,"frame interior transparent");Check(frame.data[20*480+240]!=0,"shadow visible");
}
LRESULT CALLBACK Fixture(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_APP+77) {DestroyWindow(hwnd);return 0;}
  if(msg==WM_DESTROY) {PostQuitMessage(0);return 0;}
  if(msg==WM_PAINT) {PAINTSTRUCT p{};HDC dc=BeginPaint(hwnd,&p);RECT r{};GetClientRect(hwnd,&r);HBRUSH b=CreateSolidBrush(RGB(31,42,61));FillRect(dc,&r,b);DeleteObject(b);SetTextColor(dc,RGB(228,236,250));SetBkMode(dc,TRANSPARENT);r.left+=28;r.top+=35;DrawTextW(dc,L"Apparence des fenêtres\n\nFenêtre Win32 créée pour les tests de l’add-on.\nLe titre et le cadre supplémentaires sont dessinés par le companion.",-1,&r,DT_LEFT|DT_TOP);EndPaint(hwnd,&p);return 0;}
  return DefWindowProcW(hwnd,msg,wp,lp);
}
void WireFixture() {
  WNDCLASSW cls{};cls.lpfnWndProc=Fixture;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MyWallpaper.Appearance.WireFixture";RegisterClassW(&cls);
  HWND hwnd=CreateWindowW(cls.lpszClassName,L"A06 · process-v2 qualification",WS_OVERLAPPEDWINDOW,340,220,640,400,nullptr,nullptr,cls.hInstance,nullptr);
  if(!hwnd)throw std::runtime_error("Fixture unavailable");ShowWindow(hwnd,SW_SHOWNOACTIVATE);UpdateWindow(hwnd);
  std::cout<<Json({{"hwnd",reinterpret_cast<uint64_t>(hwnd)},{"pid",GetCurrentProcessId()},{"style",GetWindowLongPtrW(hwnd,GWL_STYLE)},{"ex",GetWindowLongPtrW(hwnd,GWL_EXSTYLE)}}).dump()<<std::endl;
  std::thread input([hwnd]{std::string line;if(std::getline(std::cin,line))PostMessageW(hwnd,WM_APP+77,0,0);});
  MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}CancelSynchronousIo(input.native_handle());input.join();
}
std::optional<DWORD> Attr(HWND hwnd,DWORD key) {DWORD value{};if(FAILED(DwmGetWindowAttribute(hwnd,key,&value,sizeof(value))))return {};return value;}
void Pump() {MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}}
void PreviewBoard(const std::filesystem::path& evidence) {
  Pixels board{1180,1210,std::vector<uint32_t>(1180*1210,0xffeef0f3)};
  Gdiplus::Bitmap bitmap(board.width,board.height,board.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(board.data.data()));
  Gdiplus::Graphics g(&bitmap);g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
  Gdiplus::Font heading(L"Segoe UI",25,Gdiplus::FontStyleBold,Gdiplus::UnitPixel),body(L"Segoe UI",14,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
  Gdiplus::SolidBrush ink(Gdiplus::Color(255,36,47,61));
  g.DrawString(L"Apparence des fenêtres",-1,&heading,Gdiplus::PointF(32,22),&ink);
  g.DrawString(L"Aperçus du renderer natif de l’add-on · palettes claires et sombres · aucun flou DWM simulé",-1,&body,Gdiplus::PointF(32,62),&ink);
  const std::vector<std::string> ids={"porcelain","nocturne","aurora","ember","graphite"};
  int row=0;
  for(const auto& id:ids) {
    g.DrawString(Wide(Profiles()[id]["label"]).c_str(),-1,&body,Gdiplus::PointF(32,108.0f+row*215),&ink);
    for(int column=0;column<2;++column) {
      const auto style=ResolveStyle(ParseConfig(Snapshot({{"profile",id}})),"fixture.exe",column==1);
      const int x=32+column*568,y=139+row*215;
      auto frame=DrawFrame(566,193,34,1,style,true);
      Gdiplus::Bitmap frameBitmap(frame.width,frame.height,frame.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(frame.data.data()));
      g.DrawImage(&frameBitmap,x,y);
      auto title=DrawTitle(498,34,1,style,true,L"Carnet · Une fenêtre à votre image",false);
      Gdiplus::Bitmap titleBitmap(title.width,title.height,title.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(title.data.data()));
      g.DrawImage(&titleBitmap,x+34,y+34);
      Gdiplus::SolidBrush surface(column?Gdiplus::Color(255,30,39,53):Gdiplus::Color(255,250,251,252));
      Gdiplus::SolidBrush text(column?Gdiplus::Color(255,222,230,241):Gdiplus::Color(255,50,65,82));
      g.FillRectangle(&surface,x+34,y+68,498,91);
      g.DrawString(L"Vos applications. Votre cadre.",-1,&body,Gdiplus::PointF(static_cast<float>(x+52),static_cast<float>(y+87)),&text);
      g.DrawString(column?L"Palette sombre":L"Palette claire",-1,&body,Gdiplus::PointF(static_cast<float>(x+52),static_cast<float>(y+117)),&text);
    }
    ++row;
  }
  g.Flush();SavePng(board,evidence/L"profiles-preview.png");
}
void Windows(const std::filesystem::path& evidence) {
  std::filesystem::create_directories(evidence);
  WNDCLASSW cls{};cls.lpfnWndProc=Fixture;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MyWallpaper.Appearance.TestFixture";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);Check(RegisterClassW(&cls)!=0,"register fixture");
  HWND hwnd=CreateWindowW(cls.lpszClassName,L"Fenêtre de qualification A06",WS_OVERLAPPEDWINDOW,260,250,700,430,nullptr,nullptr,cls.hInstance,nullptr);Check(hwnd!=nullptr,"create fixture");
  struct Destroy {HWND value;~Destroy(){DestroyWindow(value);}} destroy{hwnd};
  ShowWindow(hwnd,SW_SHOWNOACTIVATE);UpdateWindow(hwnd);Pump();
  const auto originalStyle=GetWindowLongPtrW(hwnd,GWL_STYLE),originalEx=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
  Json getters=Json::object();for(DWORD key:{20u,33u,34u,35u,36u,38u}) {DWORD value{};const auto hr=DwmGetWindowAttribute(hwnd,key,&value,sizeof(value));getters[std::to_string(key)]={{"hresult",hr},{"value",value}};}
  const auto originalCaption=Attr(hwnd,35),originalBorder=Attr(hwnd,34),originalCorner=Attr(hwnd,33),originalMaterial=Attr(hwnd,38);
  RECT originalRect{};GetWindowRect(hwnd,&originalRect);
  const auto journal=std::filesystem::temp_directory_path()/(L"mw-appearance-tests-"+std::to_wstring(GetCurrentProcessId())+L".json");
  Runtime runtime(journal,true);
  auto config=ParseConfig(Snapshot({{"targetApps","window_appearance_tests.exe"},{"profile","custom"},{"restoreNativeDefaults",true},{"titleMode","native"},{"frameMode","normal"},{"nativeMaterial","acrylic"},{"corner","custom"},{"cornerRadius",22},{"captionColor","#113355"},{"inactiveCaption","#113355"},{"borderColor","#6688AA"},{"inactiveBorder","#6688AA"},{"windowOpacity",80},{"inactiveOpacity",80}}));
  runtime.Update(config);Check(runtime.Count()==1,"one fixture selected");
  BYTE alpha{};COLORREF key{};DWORD flags{};Check(GetLayeredWindowAttributes(hwnd,&key,&alpha,&flags)&&alpha==204,"opacity applied");HRGN region=CreateRectRgn(0,0,0,0);Check(GetWindowRgn(hwnd,region)!=ERROR,"radius region applied");DeleteObject(region);
  Json nativeWrites;
  {std::ifstream file(journal);auto j=Json::parse(file);nativeWrites=j["windows"][0]["attributes"];const auto snapshot=WindowState::FromJson(j["windows"][0]);Check(snapshot.SameIdentity(),"journal round trip identity");Check(snapshot.appliedAttributes.at(35)==RGB(17,51,85)&&snapshot.appliedAttributes.at(34)==RGB(102,136,170)&&snapshot.appliedAttributes.at(38)==3,"native setters accepted by Windows (not getter / blur proof)");auto recycled=snapshot;recycled.marker++;Check(!recycled.SameIdentity(),"recycled HWND token rejected");}
  runtime.Stop();runtime.Stop();Check(runtime.Count()==0,"idempotent stop");Check(Attr(hwnd,35)==originalCaption&&Attr(hwnd,34)==originalBorder&&Attr(hwnd,33)==originalCorner&&Attr(hwnd,38)==originalMaterial,"readable DWM originals restored");Check(GetWindowLongPtrW(hwnd,GWL_STYLE)==originalStyle&&GetWindowLongPtrW(hwnd,GWL_EXSTYLE)==originalEx,"styles restored");region=CreateRectRgn(0,0,0,0);Check(GetWindowRgn(hwnd,region)==ERROR,"original region restored");DeleteObject(region);
  config=ParseConfig(Snapshot({{"targetApps","window_appearance_tests.exe"},{"profile","aurora"}}));runtime.Update(config);Check(!(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CAPTION),"companion title removes native caption");Pump();
  // PNGs come from the shipped native renderer, not a claim of host blur output.
  for(const auto& [id,p]:Profiles().items()) {
    for(bool dark:{true,false}) {
      const auto s=ResolveStyle(ParseConfig(Snapshot({{"profile",id}})),"window_appearance_tests.exe",dark);
      auto titlePixels=DrawTitle(680,48,1.25,s,true,L"Apparence des fenêtres · "+Wide(p["label"]),false);
      SavePng(titlePixels,evidence/(Wide(id)+(dark?L"-dark.png":L"-light.png")));
    }
  }
  PreviewBoard(evidence);
  runtime.Update(ParseConfig(Snapshot({{"enabled",false},{"targetApps","window_appearance_tests.exe"}})));Check(runtime.Count()==0&&(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CAPTION),"disabled restores frame");
  auto full=ParseConfig(Snapshot({{"targetApps","window_appearance_tests.exe"},{"profile","system"},{"frameMode","fullscreen"}}));runtime.Update(full);Check(!(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CAPTION),"fullscreen removes frame");runtime.Stop();RECT restored{};GetWindowRect(hwnd,&restored);Check(EqualRect(&originalRect,&restored),"fullscreen position restored");
  runtime.Update(ParseConfig(Snapshot({{"targetApps","window_appearance_tests.exe"},{"windowOpacity",80},{"inactiveOpacity",80}})));Check(SetLayeredWindowAttributes(hwnd,0,230,LWA_ALPHA)!=FALSE,"simulate another opacity owner");runtime.Stop();Check(GetLayeredWindowAttributes(hwnd,&key,&alpha,&flags)&&alpha==230,"other opacity owner preserved on stop");SetWindowLongPtrW(hwnd,GWL_EXSTYLE,originalEx);
  // Recover from a journal while the managed window still exists.
  runtime.Update(ParseConfig(Snapshot({{"targetApps","window_appearance_tests.exe"},{"profile","custom"},{"titleMode","native"},{"shadowMode","none"},{"inactiveCaption","#112233"}})));
  Runtime::Recover(journal);Check(Attr(hwnd,35)==originalCaption&&!std::filesystem::exists(journal),"guardian recovery restores readable attributes and clears owned state");runtime.Stop();
  std::ofstream result(evidence/L"windows-result.json");result<<Json({{"checks",checks},{"mechanism","real HWND + successful DWM setters, styles, opacity, region, placement, restore ownership"},{"dwmGetterProbe",getters},{"successfulNativeWrites",nativeWrites},{"scope","test-owned Win32 host; no Notepad, Chromium, DWM injection, Windows 10, or multi-monitor qualification; unreadable colors return to Windows defaults only with explicit setting"},{"diagnostics",runtime.Diagnostics()}}).dump(2)<<'\n';
}
}
int wmain(int argc,wchar_t** argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  try {if(argc==6&&std::wstring(argv[1])==L"--guardian")return GuardianMain(std::stoul(argv[2]),std::stoull(argv[3]),argv[4],argv[5]);GraphicsSession graphics;if(argc>1&&std::wstring(argv[1])==L"--fixture"){WireFixture();return 0;}if(argc>1&&std::wstring(argv[1])==L"windows")Windows(argc>2?argv[2]:L"evidence");else Contract();std::cout<<checks<<" checks passed\n";return 0;}
  catch(const std::exception& error){std::cerr<<"Failed after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
