#include "FrameOutput.hpp"
#include "core/json.hpp"
#include <QColorSpace>
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>

// Native client-side Layer 2, transcribed from Adjustments.swift,
// Mask.swift::tone, CurveMath.swift and Shaders.metal::layer2Tone/layer2.
// The current Windows engine returns encoded sRGB. The non-neutral path
// converts that SDR result to encoded ProPhoto for the original tone maths,
// then back to sRGB; it does not recover gamut already compressed by Layer 1.
namespace spk::desktop::output {
namespace {
double number(const ProcessControls& c,const std::string& key){return std::get<double>(c.at(key));}
double clamp(double v){return std::clamp(v,0.0,1.0);}
double luma(const std::array<double,3>& c){return c[0]*0.2126+c[1]*0.7152+c[2]*0.0722;}
double position(double l){constexpr double m=0.3857;return l<=m?0.5*l/m:0.5+0.5*(l-m)/(1-m);}
Json curves(const ProcessControls& c){Json out;std::string error;Json::parse(std::get<std::string>(c.at("post_curves")),out,error);return out;}
struct Spline {
    std::vector<double> x,y,m;
    explicit Spline(const Json& points){
        for(const auto& p:points.items()){x.push_back(p.items()[0].as_double());y.push_back(p.items()[1].as_double());}
        const auto n=x.size();m.resize(n);std::vector<double> d(n-1);
        for(size_t i=0;i+1<n;++i)d[i]=(y[i+1]-y[i])/std::max(x[i+1]-x[i],1e-6);
        m[0]=d[0];m[n-1]=d[n-2];
        for(size_t i=1;i+1<n;++i)m[i]=d[i-1]*d[i]<=0?0:(d[i-1]+d[i])*0.5;
        for(size_t i=0;i+1<n;++i)if(d[i]!=0){const double a=m[i]/d[i],b=m[i+1]/d[i],s=a*a+b*b;if(s>9){const double t=3/std::sqrt(s);m[i]=t*a*d[i];m[i+1]=t*b*d[i];}}
    }
    double evaluate(double v)const{
        size_t i=0;while(i+2<x.size()&&v>x[i+1])++i;
        const double h=std::max(x[i+1]-x[i],1e-6),t=(v-x[i])/h,t2=t*t,t3=t2*t;
        return clamp((2*t3-3*t2+1)*y[i]+(t3-2*t2+t)*h*m[i]+(-2*t3+3*t2)*y[i+1]+(t3-t2)*h*m[i+1]);
    }
};
bool identityCurves(const Json& value){for(const auto& [key,points]:value.fields())for(const auto& p:points.items())if(p.items()[0].as_double()!=p.items()[1].as_double())return false;return true;}
}
bool postIsNeutral(const ProcessControls& c){
    if(!std::get<bool>(c.at("post_enabled")))return true;
    for(const auto* key:{"post_temperature","post_tint","post_exposure","post_contrast","post_brightness","post_saturation","post_highlights","post_shadows","post_black_point","post_white_point","post_vignette"})if(number(c,key)!=0)return false;
    for(const auto* zone:{"master","shadows","midtones","highlights"})for(const auto* part:{"saturation","luminance"})if(number(c,std::string("post_")+zone+"_"+part)!=0)return false;
    return identityCurves(curves(c));
}
QImage applyPost(const QImage& input,const ProcessControls& controls){
    std::string reason;if(!validate_process_controls(controls,reason))throw std::runtime_error(reason);
    if(postIsNeutral(controls))return input;
    auto tagged=input;tagged.setColorSpace(QColorSpace::SRgb);
    auto image=tagged.convertedToColorSpace(QColorSpace::ProPhotoRgb,QImage::Format_RGBA64);
    if(image.isNull())throw std::runtime_error("Cannot allocate post-development working image");
    const double t=number(controls,"post_temperature")/100,g=number(controls,"post_tint")/100;
    const std::array<double,3> wb={1+0.18*t,1-0.12*g,1-0.18*t};
    const double exposure=std::exp2(number(controls,"post_exposure")/2.2);
    const double contrast=number(controls,"post_contrast")/100,brightness=number(controls,"post_brightness")/100;
    const double saturation=1+number(controls,"post_saturation")/100,shadows=number(controls,"post_shadows")/100,highlights=number(controls,"post_highlights")/100;
    const double black=number(controls,"post_black_point")/100,white=number(controls,"post_white_point")/100,denominator=std::max(1-black-white,0.05);
    const double vignette=number(controls,"post_vignette")/100,midpoint=number(controls,"post_vignette_midpoint")/100;
    std::array<std::array<double,3>,4> offsets{};std::array<double,4> luminosity{};
    const std::array<const char*,4> zones={"master","shadows","midtones","highlights"};bool balance=false;
    for(size_t i=0;i<zones.size();++i){const auto key=std::string("post_")+zones[i];
        const double h=number(controls,key+"_hue")*3.14159265358979323846/180,s=number(controls,key+"_saturation")*0.25;
        luminosity[i]=number(controls,key+"_luminance")*0.25;balance=balance||s!=0||luminosity[i]!=0;
        for(int channel=0;channel<3;++channel)offsets[i][channel]=std::cos(h-channel*2*3.14159265358979323846/3)*s;
    }
    const auto points=curves(controls);const bool useCurves=!identityCurves(points);
    std::array<std::array<double,256>,5> tables{};
    if(useCurves){const std::array<const char*,5> names={"rgb","luma","r","g","b"};for(size_t i=0;i<5;++i){Spline spline(points.at(names[i]));for(size_t j=0;j<256;++j)tables[i][j]=spline.evaluate(double(j)/255);}}
    auto lookup=[&](size_t row,double v){const double p=clamp(v)*255;const auto a=static_cast<size_t>(p);const auto b=std::min(size_t(255),a+1);return tables[row][a]+(tables[row][b]-tables[row][a])*(p-a);};
    for(int y=0;y<image.height();++y){auto* row=reinterpret_cast<QRgba64*>(image.scanLine(y));
        for(int x=0;x<image.width();++x){auto& pixel=row[x];std::array<double,3> c={pixel.red()/65535.0,pixel.green()/65535.0,pixel.blue()/65535.0};
            for(int k=0;k<3;++k){c[k]*=wb[k];if(exposure!=1)c[k]=std::max(c[k],0.0)*exposure;if(contrast!=0)c[k]=(c[k]-0.3857)*(1+contrast*1.2)+0.3857;if(brightness!=0)c[k]=std::pow(std::max(c[k],0.0),1/(1+brightness*0.8));}
            double l=position(luma(c));
            for(int k=0;k<3;++k){if(shadows!=0)c[k]+=shadows*0.25*(1-l)*(1-l)*(1-c[k]);if(highlights!=0)c[k]+=highlights*0.25*l*l*(highlights>0?1-c[k]:c[k]);c[k]=(c[k]-black)/denominator;}
            l=luma(c);if(saturation!=1)for(auto& v:c)v=l+(v-l)*saturation;
            if(balance){const double p=clamp(position(l)),ws=(1-p)*(1-p),wh=p*p,wm=std::max(1-ws-wh,0.0);
                const double gain=1+luminosity[0]+luminosity[1]*ws+luminosity[2]*wm+luminosity[3]*wh;
                for(int k=0;k<3;++k)c[k]=(c[k]+offsets[0][k]+offsets[1][k]*ws+offsets[2][k]*wm+offsets[3][k]*wh)*gain;
            }
            if(useCurves){for(auto& v:c)v=clamp(v);const double ly=luma(c),mapped=lookup(1,ly),gain=ly>1e-4?mapped/ly:1;
                for(int k=0;k<3;++k)c[k]=lookup(size_t(k+2),lookup(0,clamp(c[k]*gain)));
            }
            if(vignette!=0){const double px=(double(x)+0.5)/image.width()*2-1,py=(double(y)+0.5)/image.height()*2-1;
                const double q=clamp((std::hypot(px,py)-midpoint*1.2)/(1.5-midpoint*1.2)),fall=q*q*(3-2*q),gain=1+vignette*fall*0.9;for(auto& v:c)v*=gain;
            }
            pixel=QRgba64::fromRgba64(quint16(std::lround(clamp(c[0])*65535)),quint16(std::lround(clamp(c[1])*65535)),quint16(std::lround(clamp(c[2])*65535)),65535);
        }
    }
    auto output=image.convertedToColorSpace(QColorSpace::SRgb,QImage::Format_RGBA64);
    if(output.isNull())throw std::runtime_error("Cannot convert the post-development image to sRGB");
    return output;
}
} // namespace spk::desktop::output
