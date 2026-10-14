#include "appearance.hpp"
#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace appearance {
namespace {
constexpr uint32_t kChunk=1024*1024;
bool Exact(HANDLE stream,void* bytes,DWORD count,bool allowEof) {
  auto* cursor=static_cast<BYTE*>(bytes);DWORD remaining=count;
  while(remaining) {
    DWORD read=0;
    if(!ReadFile(stream,cursor,remaining,&read,nullptr)) {const auto error=GetLastError();if(allowEof&&remaining==count&&(error==ERROR_BROKEN_PIPE||error==ERROR_OPERATION_ABORTED))return false;throw std::runtime_error("Lecture du companion interrompue.");}
    if(read==0) {if(allowEof&&remaining==count)return false;throw std::runtime_error("Trame tronquée.");}
    cursor+=read;remaining-=read;allowEof=false;
  }
  return true;
}
void Output(HANDLE stream,const void* bytes,DWORD count) {
  auto* cursor=static_cast<const BYTE*>(bytes);
  while(count) {DWORD sent{};if(!WriteFile(stream,cursor,count,&sent,nullptr)||!sent)throw std::runtime_error("Écriture du companion interrompue.");cursor+=sent;count-=sent;}
}
}
std::optional<Json> ReadRecord(HANDLE input) {
  std::string bytes;bool multi=false;
  for(;;) {
    uint32_t header{};
    if(!Exact(input,&header,4,!multi))return std::nullopt;
    const auto kind=header>>30,length=header&0x3fffffffu;
    if(!length||length>kChunk||(!multi&&(kind==2||kind==3))||(multi&&(kind==0||kind==1)))throw std::runtime_error("Ordre ou taille de chunks process-v2 invalide.");
    const size_t begin=bytes.size();bytes.resize(begin+length);Exact(input,bytes.data()+begin,length,false);
    if(kind==0||kind==3)return Json::parse(bytes);
    multi=true;
  }
}
void WriteRecord(HANDLE output,const Json& record) {
  const auto bytes=record.dump();size_t begin=0;
  while(begin<bytes.size()) {
    const auto size=static_cast<uint32_t>(std::min<size_t>(kChunk,bytes.size()-begin));
    const uint32_t kind=bytes.size()<=kChunk?0:begin==0?1:begin+size==bytes.size()?3:2;
    const uint32_t header=(kind<<30)|size;Output(output,&header,4);Output(output,bytes.data()+begin,size);begin+=size;
  }
}
void ValidateInit(const Json& frame) {
  static const std::vector<std::string> keys={"type","v","addonId","artifactDigest","version","targetArchitecture","scratchDirectory","layerSettings","deviceSettings"};
  if(!frame.is_object()||frame.value("type","")!="init"||frame.value("v",0)!=5)throw std::runtime_error("La première trame doit être init process-v2 version 5.");
  for(const auto& key:keys)if(!frame.contains(key))throw std::runtime_error("Champ init manquant : "+key);
  for(const auto& [key,value]:frame.items())if(std::find(keys.begin(),keys.end(),key)==keys.end())throw std::runtime_error("Champ init inconnu : "+key);
  if(frame["targetArchitecture"]!="windows-x86_64"||!frame["version"].is_string()||frame["version"].get<std::string>().empty())throw std::runtime_error("Architecture ou version init invalide.");
  const auto id=frame.at("addonId").get<std::string>(),digest=frame.at("artifactDigest").get<std::string>();
  const auto wideId=Wide(id);
  if(id.empty()||id.size()>256||std::iswspace(wideId.front())||std::iswspace(wideId.back())||std::any_of(wideId.begin(),wideId.end(),[](wchar_t c){return std::iswcntrl(c)!=0;})||digest.size()!=71||!digest.starts_with("sha256:")||!std::all_of(digest.begin()+7,digest.end(),[](unsigned char c){return std::isxdigit(c)!=0;}))throw std::runtime_error("Identité init invalide.");
  if(!frame["scratchDirectory"].is_string()||!std::filesystem::path(Wide(frame["scratchDirectory"])).is_absolute())throw std::runtime_error("Un répertoire scratch absolu est requis.");
  ParseConfig(frame);
}
bool StartGuardian(const std::filesystem::path& journal) {
  const auto birth=ProcessBirth(GetCurrentProcess());
  const auto eventName=L"Local\\MyWallpaper.Appearance.Guard."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(birth);
  HANDLE event=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());if(!event)return false;
  wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
  auto command=L"\""+std::wstring(exe)+L"\" --guardian "+std::to_wstring(GetCurrentProcessId())+L" "+std::to_wstring(birth)+L" \""+journal.wstring()+L"\" \""+eventName+L"\"";
  STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION child{};
  const bool started=CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE;
  if(started) {CloseHandle(child.hThread);CloseHandle(child.hProcess);}
  const bool ready=started&&WaitForSingleObject(event,3000)==WAIT_OBJECT_0;CloseHandle(event);return ready;
}
int GuardianMain(DWORD owner,uint64_t birth,const std::filesystem::path& journal,const std::wstring& eventName) {
  HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,owner);
  if(!process||ProcessBirth(process)!=birth) {if(process)CloseHandle(process);return 2;}
  HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,eventName.c_str());if(!event) {CloseHandle(process);return 3;}SetEvent(event);CloseHandle(event);
  WaitForSingleObject(process,INFINITE);CloseHandle(process);
  for(int attempt=0;attempt<20;++attempt) {
    try {Runtime::Recover(journal);if(!std::filesystem::exists(journal))return 0;}catch(...){ }
    Sleep(100);
  }
  return 4;
}
}
