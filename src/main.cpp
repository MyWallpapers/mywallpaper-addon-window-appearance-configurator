#include "appearance.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

using namespace appearance;
namespace {
std::atomic<HWND> controller{};
std::atomic<bool> eventPending{};
void CALLBACK WindowEvent(HWINEVENTHOOK,DWORD event,HWND,LONG object,LONG,DWORD,DWORD) {
  if(event!=EVENT_SYSTEM_FOREGROUND&&event!=EVENT_OBJECT_CREATE&&event!=EVENT_OBJECT_DESTROY&&event!=EVENT_OBJECT_SHOW&&event!=EVENT_OBJECT_HIDE&&event!=EVENT_OBJECT_LOCATIONCHANGE&&event!=EVENT_OBJECT_NAMECHANGE)return;
  if(object!=OBJID_WINDOW&&object!=0)return;
  if(!eventPending.exchange(true))if(const HWND hwnd=controller.load())PostMessageW(hwnd,WM_APP+1,0,0);
}
LRESULT CALLBACK Controller(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_TIMER||msg==WM_APP+1||msg==WM_APP+2)return 0;
  return DefWindowProcW(hwnd,msg,wp,lp);
}
Json Error(const std::string& code,const std::string& message) {return {{"type","error"},{"v",5},{"code",code},{"message",message}};}
void FrameKeys(const Json& frame,const std::vector<std::string>& keys) {
  for(const auto& key:keys)if(!frame.contains(key))throw std::runtime_error("Champ process-v2 manquant : "+key);
  for(const auto& [key,value]:frame.items())if(std::find(keys.begin(),keys.end(),key)==keys.end())throw std::runtime_error("Champ process-v2 inconnu : "+key);
}
int Serve() {
  const HANDLE input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE singleton=CreateMutexW(nullptr,TRUE,L"Local\\MyWallpaper.WindowAppearanceConfigurator.v1");
  if(!singleton||GetLastError()==ERROR_ALREADY_EXISTS) {if(singleton)CloseHandle(singleton);WriteRecord(output,Error("addon_instance_conflict","Une instance Apparence des fenêtres est déjà active dans cette session Windows."));return 2;}
  struct MutexCleanup {HANDLE value;~MutexCleanup(){ReleaseMutex(value);CloseHandle(value);}} mutexCleanup{singleton};
  const auto initial=ReadRecord(input);if(!initial)return 0;ValidateInit(*initial);
  WriteRecord(output,{{"type","progress"},{"v",5}});
  const auto scratch=std::filesystem::path(Wide(initial->at("scratchDirectory")));
  std::filesystem::create_directories(scratch);
  const auto journal=scratch/(L"window-appearance-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(ProcessBirth(GetCurrentProcess()))+L".json");
  if(!StartGuardian(journal))throw std::runtime_error("Le gardien de restauration n’a pas démarré. Aucun effet n’a été appliqué.");
  GraphicsSession graphics;
  Runtime runtime(journal);
  WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MyWallpaper.Appearance.Controller";cls.lpfnWndProc=Controller;
  if(!RegisterClassW(&cls))throw std::runtime_error("Impossible de préparer l’observation des fenêtres.");
  controller=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
  if(!controller.load())throw std::runtime_error("Impossible de créer le coordinateur local.");
  runtime.Update(ParseConfig(*initial));
  SetTimer(controller,1,150,nullptr);
  HWINEVENTHOOK observation=SetWinEventHook(EVENT_MIN,EVENT_MAX,nullptr,&WindowEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  WriteRecord(output,{{"type","ready"},{"v",5}});
  std::mutex queueMutex;std::condition_variable available;std::deque<Json> queue;
  bool ending=false;
  std::thread reader([&] {
    try {
      for(;;) {
        auto frame=ReadRecord(input);
        if(!frame)frame=Json{{"type","shutdown"},{"v",5}};
        const bool last=frame->value("type","")=="shutdown";
        std::unique_lock lock(queueMutex);available.wait(lock,[&]{return ending||queue.size()<8;});if(ending)return;
        queue.push_back(std::move(*frame));PostMessageW(controller,WM_APP+2,0,0);lock.unlock();if(last)return;
      }
    }catch(const std::exception& e) {std::lock_guard lock(queueMutex);queue.push_back({{"type","_protocolError"},{"message",e.what()}});PostMessageW(controller,WM_APP+2,0,0);}
  });
  int result=0;bool running=true;
  try {
    MSG message{};
    while(running&&GetMessageW(&message,nullptr,0,0)>0) {
      TranslateMessage(&message);DispatchMessageW(&message);
      if(message.message==WM_TIMER||message.message==WM_APP+1) {eventPending=false;runtime.Tick();}
      if(message.message==WM_APP+2) {
        std::deque<Json> frames;{std::lock_guard lock(queueMutex);frames.swap(queue);}available.notify_all();
        for(const auto& frame:frames) {
          const auto type=frame.value("type","");
          if(type=="_protocolError") {WriteRecord(output,Error("protocol_error",frame.value("message","Lecture invalide.")));running=false;result=3;break;}
          if(frame.value("v",0)!=5)throw std::runtime_error("Version process-v2 invalide.");
          if(type=="shutdown") {FrameKeys(frame,{"type","v"});running=false;break;}
          if(type=="settings") {FrameKeys(frame,{"type","v","layerSettings","deviceSettings"});try{auto config=ParseConfig(frame);runtime.Update(std::move(config));}catch(const std::exception& e){WriteRecord(output,Error("addon_settings_rejected",e.what()));}}
          else if(type=="message") {FrameKeys(frame,{"type","v","surface","instanceId","displayIndex","payload"});WriteRecord(output,Error("addon_message_rejected","Cet add-on reçoit ses réglages via MyWallpaper et n’expose pas de commande métier par message."));}
          else throw std::runtime_error("Type de trame inconnu ou init répété.");
        }
      }
    }
  }catch(const std::exception& e) {result=3;try{WriteRecord(output,Error("companion_error",e.what()));}catch(...){}}
  {std::lock_guard lock(queueMutex);ending=true;}available.notify_all();
  CancelSynchronousIo(reader.native_handle());reader.join();
  if(observation)UnhookWinEvent(observation);KillTimer(controller,1);
  runtime.Stop();
  if(GetEnvironmentVariableW(L"MW_WINDOW_APPEARANCE_DIAGNOSTICS",nullptr,0))std::cerr<<runtime.Diagnostics().dump()<<'\n';
  DestroyWindow(controller);controller=nullptr;return result;
}
}
int wmain(int argc,wchar_t** argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  try {
    if(argc==6&&std::wstring(argv[1])==L"--guardian")return GuardianMain(std::stoul(argv[2]),std::stoull(argv[3]),argv[4],argv[5]);
    if(argc==2&&std::wstring(argv[1])==L"--manifest") {std::cout<<Manifest().dump(2)<<'\n';return 0;}
    if(argc!=1)throw std::runtime_error("Utilisation : companion (transport MyWallpaper sur stdin/stdout).");
    return Serve();
  }catch(const std::exception& e) {try{WriteRecord(GetStdHandle(STD_OUTPUT_HANDLE),Error("companion_error",e.what()));}catch(...){}return 1;}
}
