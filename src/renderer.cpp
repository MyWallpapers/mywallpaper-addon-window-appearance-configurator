#include "appearance.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace appearance {
namespace {
using namespace Gdiplus;
Color ColorOf(const std::string& text, BYTE alpha=255) {
  const auto rgb = std::stoul(text.substr(1),nullptr,16);
  return Color(alpha,static_cast<BYTE>(rgb>>16),static_cast<BYTE>(rgb>>8),static_cast<BYTE>(rgb));
}
double Distance(double x,double y,double left,double top,double right,double bottom,double radius) {
  radius = std::max(0.0,std::min(radius,std::min(right-left,bottom-top)/2));
  const double qx = std::abs(x-(left+right)/2)-(right-left)/2+radius;
  const double qy = std::abs(y-(top+bottom)/2)-(bottom-top)/2+radius;
  return std::hypot(std::max(qx,0.0),std::max(qy,0.0)) + std::min(std::max(qx,qy),0.0)-radius;
}
uint32_t Premult(Color c, double coverage) {
  const auto a=static_cast<unsigned>(std::clamp(coverage,0.0,1.0)*c.GetA()+0.5);
  return (a<<24)|((c.GetR()*a/255)<<16)|((c.GetG()*a/255)<<8)|(c.GetB()*a/255);
}
Color Blend(Color a,Color b,double t) {
  auto lerp=[&](int p,int q){return static_cast<BYTE>(p+(q-p)*std::clamp(t,0.0,1.0));};
  return Color(lerp(a.GetA(),b.GetA()),lerp(a.GetR(),b.GetR()),lerp(a.GetG(),b.GetG()),lerp(a.GetB(),b.GetB()));
}
double Gradient(int x,int y,int width,int height,const Json& s) {
  const auto direction=s["gradientDirection"].get<std::string>();
  const double xx=static_cast<double>(x)/std::max(1,width-1), yy=static_cast<double>(y)/std::max(1,height-1);
  return direction=="vertical"?yy:direction=="diagonal"?(xx+yy)/2:xx;
}
double Radius(const Json& s,double scale) {
  const auto corner=s["corner"].get<std::string>();
  return scale*(corner=="square"?0:corner=="small"?4:corner=="custom"?s["cornerRadius"].get<double>():8);
}
std::vector<int> Buttons(const Json& s) {
  std::vector<int> result;
  if(s["buttonSide"]=="left") { if(s["buttonClose"].get<bool>())result.push_back(2); if(s["buttonMinimize"].get<bool>())result.push_back(0); if(s["buttonMaximize"].get<bool>())result.push_back(1); }
  else { if(s["buttonMinimize"].get<bool>())result.push_back(0); if(s["buttonMaximize"].get<bool>())result.push_back(1); if(s["buttonClose"].get<bool>())result.push_back(2); }
  return result;
}
}
Pixels DrawFrame(int width,int height,int margin,double scale,const Json& style,bool active) {
  if(width<1||height<1||width>16384||height>16384||static_cast<uint64_t>(width)*height>32u*1024*1024) throw std::runtime_error("Décoration trop grande.");
  Pixels p{width,height,std::vector<uint32_t>(static_cast<size_t>(width)*height)};
  const double border=(style["borderMode"]=="solid"||style["borderMode"]=="gradient")?style["borderWidth"].get<double>()*scale:0;
  const double radius=Radius(style,scale);
  const double opacity=style["shadowMode"]=="soft"?style["shadowOpacity"].get<double>()/100:0;
  const double blur=std::max(1.0,style["shadowBlur"].get<double>()*scale);
  const double extent=style["shadowSize"].get<double>()*scale;
  const double offset=style["shadowOffset"].get<double>()*scale;
  const auto shadow=ColorOf(style["shadowColor"]);
  const auto begin=ColorOf(active?style["borderColor"].get<std::string>():style["inactiveBorder"].get<std::string>());
  const auto end=ColorOf(active&&style["borderMode"]=="gradient"?style["borderEnd"].get<std::string>():active?style["borderColor"].get<std::string>():style["inactiveBorder"].get<std::string>());
  for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
    // The unpainted interior and WS_EX_TRANSPARENT preserve the application.
    if(y>=margin+radius&&y<height-margin-radius&&x==margin){x=width-margin;if(x>=width)break;}
    const auto distance=Distance(x+0.5,y+0.5,margin,margin,width-margin,height-margin,radius);
    if(distance<0)continue;
    double shadowAlpha=0;
    if(opacity>0&&extent>0) {
      const auto d=std::max(0.0,Distance(x+0.5,y+0.5-offset,margin,margin,width-margin,height-margin,radius));
      if(d<extent)shadowAlpha=opacity*std::exp(-(d*d)/(2*blur*blur))*std::min(1.0,(extent-d)/std::max(1.0,scale*4));
    }
    const auto edge=std::clamp(border+0.5-distance,0.0,1.0);
    const auto c=Blend(shadow,Blend(begin,end,Gradient(x,y,width,height,style)),edge);
    p.data[static_cast<size_t>(y)*width+x]=Premult(c,edge+(1-edge)*shadowAlpha);
  }
  return p;
}
int ButtonAt(int x,int width,double scale,const Json& s) {
  const auto buttons=Buttons(s);
  const int size=static_cast<int>(s["buttonSize"].get<double>()*scale);
  const int start=s["buttonSide"]=="left"?0:width-size*static_cast<int>(buttons.size());
  if(x<start||size==0||x>=start+size*static_cast<int>(buttons.size()))return -1;
  return buttons[static_cast<size_t>((x-start)/size)];
}
Pixels DrawTitle(int width,int height,double scale,const Json& s,bool active,const std::wstring& title,bool maximized,HICON icon,int hovered) {
  if(width<1||height<1||width>16384||height>256)throw std::runtime_error("Titre trop grand.");
  Pixels p{width,height,std::vector<uint32_t>(static_cast<size_t>(width)*height)};
  const auto begin=ColorOf(active?s["titleStart"].get<std::string>():s["inactiveCaption"].get<std::string>());
  const auto end=ColorOf(active&&s["titleFill"]=="gradient"?s["titleEnd"].get<std::string>():active?s["titleStart"].get<std::string>():s["inactiveCaption"].get<std::string>());
  const double opacity=s["titleFill"]=="translucent"?s["titleOpacity"].get<double>()/100:1;
  const double radius=Radius(s,scale);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
    const double coverage=std::clamp(0.5-Distance(x+0.5,y+0.5,0,0,width,height+radius,radius),0.0,1.0);
    p.data[static_cast<size_t>(y)*width+x]=Premult(Blend(begin,end,Gradient(x,y,width,height,s)),coverage*opacity);
  }
  Bitmap bitmap(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(p.data.data()));
  Graphics g(&bitmap); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
  const auto foreground=ColorOf(active?s["textColor"].get<std::string>():s["inactiveText"].get<std::string>());
  SolidBrush textBrush(foreground);
  const auto buttons=Buttons(s);
  const float buttonWidth=static_cast<float>(s["buttonSize"].get<double>()*scale);
  const float buttonStart=s["buttonSide"]=="left"?0:width-buttonWidth*static_cast<float>(buttons.size());
  for(size_t i=0;i<buttons.size();++i) {
    const float left=buttonStart+static_cast<float>(i)*buttonWidth, cx=left+buttonWidth/2, cy=height/2.0f;
    const int button=buttons[i];
    if(hovered==button) { SolidBrush hover(ColorOf(button==2?s["closeHover"].get<std::string>():s["buttonHover"].get<std::string>())); g.FillRectangle(&hover,left,0.0f,buttonWidth,static_cast<float>(height)); }
    if(s["buttonStyle"]=="circles") {
      const Color color=button==2?Color(255,240,108,105):button==0?Color(255,231,193,88):Color(255,101,186,140);
      SolidBrush brush(color); g.FillEllipse(&brush,cx-static_cast<float>(scale*5),cy-static_cast<float>(scale*5),static_cast<float>(scale*10),static_cast<float>(scale*10));
    } else {
      const float r=static_cast<float>((s["buttonStyle"]=="compact"?3:4)*scale);
      Pen pen(foreground,static_cast<REAL>(1.2*scale));
      if(button==0)g.DrawLine(&pen,cx-r,cy+2,cx+r,cy+2);
      else if(button==1) { g.DrawRectangle(&pen,cx-r,cy-r,r*2,r*2); if(maximized)g.DrawLine(&pen,cx-r,cy-r+static_cast<float>(scale*2),cx+r,cy-r+static_cast<float>(scale*2)); }
      else { g.DrawLine(&pen,cx-r,cy-r,cx+r,cy+r); g.DrawLine(&pen,cx+r,cy-r,cx-r,cy+r); }
    }
  }
  const float padding=static_cast<float>(s["titlePadding"].get<double>()*scale);
  float left=padding,right=static_cast<float>(width)-padding;
  if(s["buttonSide"]=="left")left=buttonWidth*static_cast<float>(buttons.size())+padding; else right=buttonStart-padding;
  if(icon&&s["titleIcon"].get<bool>()) { Bitmap image(icon); const float size=static_cast<float>(16*scale); g.DrawImage(&image,left,(height-size)/2,size,size); left+=size+padding/2; }
  std::wstring actual=title;
  if(s["titleText"]=="custom")actual=Wide(s["customTitle"]);
  else if(s["titleText"]=="hidden")actual.clear();
  Font font(Wide(s["titleFont"]).c_str(),static_cast<REAL>(s["titleSize"].get<double>()*scale),s["titleBold"].get<bool>()?FontStyleBold:FontStyleRegular,UnitPixel);
  StringFormat format; format.SetLineAlignment(StringAlignmentCenter); format.SetTrimming(StringTrimmingEllipsisCharacter); format.SetFormatFlags(StringFormatFlagsNoWrap);
  format.SetAlignment(s["titleAlign"]=="center"?StringAlignmentCenter:s["titleAlign"]=="right"?StringAlignmentFar:StringAlignmentNear);
  g.DrawString(actual.c_str(),-1,&font,RectF(left,0,std::max(0.0f,right-left),static_cast<float>(height)),&format,&textBrush);
  return p;
}
void SavePng(const Pixels& p,const std::filesystem::path& path) {
  UINT count=0,bytes=0; Gdiplus::GetImageEncodersSize(&count,&bytes);
  std::vector<BYTE> buffer(bytes);
  auto* codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
  Gdiplus::GetImageEncoders(count,bytes,codecs);
  const auto* codec=std::find_if(codecs,codecs+count,[](const auto& c){return std::wstring(c.MimeType)==L"image/png";});
  if(codec==codecs+count)throw std::runtime_error("Encodeur PNG indisponible.");
  Gdiplus::Bitmap image(p.width,p.height,p.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(p.data.data())));
  if(image.Save(path.c_str(),&codec->Clsid,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Impossible d’écrire l’image PNG.");
}
}
