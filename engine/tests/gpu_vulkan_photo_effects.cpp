// Independent double references for the newly ported photo kernels. This is
// numerical validation on the local Vulkan device, not Metal image parity.
#include "gpu/gpu.hpp"
#include "gpu_vulkan_read.hpp"
#include "latitude_fit.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>

namespace {
using spk::gpu::Arg;
using spk::gpu::Gpu;
constexpr float guard = -123456.0f;
size_t cases = 0;

bool run(Gpu* gpu, const char* kernel, const std::vector<float>& input,
         const std::vector<float>& params, const std::vector<double>& expected,
         std::string& error, double bar = 2e-6) {
    const uint32_t count = uint32_t(expected.size() / 3);
    std::vector<float> initial(expected.size() + 17, guard);
    auto src = gpu->upload(input.data(), input.size() * sizeof(float), error);
    auto dst = gpu->upload(initial.data(), initial.size() * sizeof(float), error);
    if (!src || !dst || !gpu->dispatch(kernel, {Arg::buf(src),
        Arg::inline_bytes(params.data(), params.size()), Arg::inline_bytes(&count, 1),
        Arg::buf(dst)}, std::max(count, 1u), error)) return false;
    std::vector<float> actual;
    if (!read_test_buffer(gpu, dst, actual, error)) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        const double ref = expected[i], got = actual[i];
        if ((std::isnan(ref) && std::isnan(got)) || ref == got) continue;
        if (!std::isfinite(got) || !std::isfinite(ref) || std::abs(got-ref) > bar * std::max(1.0, std::abs(ref))) {
            char detail[200];
            std::snprintf(detail, sizeof detail, "%s[%zu]: %.9g != %.17g", kernel, i, got, ref);
            error = detail; return false;
        }
    }
    for (size_t i = expected.size(); i < initial.size(); ++i)
        if (actual[i] != guard) { error = std::string(kernel) + " output overrun"; return false; }
    ++cases;
    return true;
}

bool latitude(Gpu* gpu, std::string& error) {
    for (unsigned norm = 0; norm < 3; ++norm) for (float m : {1.f, 2.f, 3.5f})
    for (unsigned shape = 0; shape < 3; ++shape) {
        const uint32_t width = shape == 2 ? 7 : 37, height = shape == 2 ? 37 : 7;
        const uint32_t count = width * height;
        std::vector<float> p{2,1.5f,-3,2,m,4,float(norm),0.288f,0.712f,0,0.184f,
            float(width),float(height),shape ? 0.5f : 0.f,1,0.75f,-1,1,shape == 2 ? 4.f : 0.f};
        std::vector<float> input(count * 3);
        std::vector<double> expected(count * 3);
        for (uint32_t i = 0; i < count; ++i) {
            std::array<double, 3> c{};
            for (unsigned j = 0; j < 3; ++j) {
                float v = float(0.184 * std::exp2(-25.0 + (i % 101) * 0.5) * (1.0 + j * 0.2));
                if (i % 41 == 0) v = -v;
                if (i == 1) v = 0;
                if (i == 2) v = std::numeric_limits<float>::infinity();
                if (i == 3) v = std::numeric_limits<float>::quiet_NaN();
                input[3*i+j] = v;
                c[j] = std::fmax(double(v), 0.0);
            }
            double n;
            if (norm == 1) n = double(p[7])*c[0] + double(p[8])*c[1] + double(p[9])*c[2];
            else if (norm == 2) n = std::max({c[0],c[1],c[2]});
            else {
                const double s = c[0]*c[0]+c[1]*c[1]+c[2]*c[2];
                n = s > 0 ? (c[0]*c[0]*c[0]+c[1]*c[1]*c[1]+c[2]*c[2]*c[2])/s : 0;
                // Metal's float norm overflows for sufficiently bright input.
                float sf = float(c[0])*float(c[0])+float(c[1])*float(c[1])+float(c[2])*float(c[2]);
                if (!std::isfinite(sf)) n = std::numeric_limits<double>::quiet_NaN();
            }
            spk::SceneLatitudeParams q;
            const bool second = shape && (width >= height ? (double(i%width)+0.5)/width :
                (double(i/width)+p[18]+0.5)/height) >= p[13];
            q.highlight_knee=p[second?14:0]; q.highlight_room=p[second?15:1];
            q.shadow_knee=p[second?16:2]; q.shadow_room=p[second?17:3];
            q.rolloff=m; q.max_lift=p[5];
            double gain=1;
            if (n > 0 && std::isfinite(n)) gain=std::exp2(spk::slm::delta(std::log2(n/double(p[10])),q));
            for (unsigned j=0;j<3;++j) expected[3*i+j]=double(input[3*i+j])*gain;
        }
        // At +/-24 EV, float stop-domain subtraction and exp2 accumulate
        // several ulps. Bound the complete gain, not a single arithmetic op;
        // this remains below the engine's 3e-5 full-image reference bound.
        if (!run(gpu,"spk_scene_latitude",input,p,expected,error,8e-6)) {
            error += " norm=" + std::to_string(norm) + " m=" + std::to_string(m) + " shape=" + std::to_string(shape);
            return false;
        }
    }
    return true;
}

std::array<double,3> di_reference(const float* y, const std::vector<float>& p) {
    std::array<double,3> pos{},lin{};
    for (unsigned c=0;c<3;++c) pos[c]=(std::pow(10.0,double(y[c]))-double(p[40]))*std::pow(10.0,double(p[9+c]));
    for (unsigned c=0;c<3;++c) for(unsigned j=0;j<3;++j) lin[c]+=pos[j]*double(p[3*j+c]);
    if (p[30]) {
        double lms[3]{};
        for(unsigned c=0;c<3;++c) {
            for(unsigned j=0;j<3;++j) lms[c]+=double(p[12+3*c+j])*lin[j];
            lms[c]=std::cbrt(std::max(lms[c],0.0));
        }
        double L=.2104542553*lms[0]+.7936177850*lms[1]-.0040720468*lms[2];
        double a=1.9779984951*lms[0]-2.4285922050*lms[1]+.4505937099*lms[2];
        double b=.0259040371*lms[0]+.7827717662*lms[1]-.8086757660*lms[2];
        double C=std::hypot(a,b),h=std::atan2(b,a)*180/std::acos(-1.0);
        double d=std::fmod(h-p[33]+540,360)-180;
        double w=std::abs(d)<p[34] ? .5*(1+std::cos(std::acos(-1.0)*d/p[34])) : 0;
        double g=std::clamp((C-p[35])/(p[36]-p[35]),0.0,1.0); w*=g*g*(3-2*g);
        if(w>0) {
            double h2=(h+w*p[31])*std::acos(-1.0)/180,C2=C*(1+w*(p[32]-1));
            a=C2*std::cos(h2); b=C2*std::sin(h2);
            double v[3]={L+.3963377774*a+.2158037573*b,L-.1055613458*a-.0638541728*b,L-.0894841775*a-1.2914855480*b};
            for(unsigned c=0;c<3;++c) { lin[c]=0; for(unsigned j=0;j<3;++j) lin[c]+=p[21+3*c+j]*v[j]*v[j]*v[j]; }
        }
    }
    for(auto& v:lin) v=std::clamp((p[37]+p[38]*std::log10(std::max(v,0.0)*(1-p[41])+p[41]))/p[39],0.0,1.0);
    return lin;
}

bool cineon(Gpu* gpu,std::string& error) {
    std::vector<float> p(42,0);
    p[0]=1.1f;p[3]=.1f;p[6]=-.2f;p[1]=-.05f;p[4]=1.05f;p[8]=1;
    p[9]=.12f;p[10]=-.08f;p[11]=.03f;
    const float to[9]={.4122214708f,.5363325363f,.0514459929f,.2119034982f,.6806995451f,.1073969566f,.0883024619f,.2817188376f,.6299787005f};
    const float from[9]={4.0767416621f,-3.3077115913f,.2309699292f,-1.2684380046f,2.6097574011f,-.3413193965f,-.0041960863f,-.7034186147f,1.7076147010f};
    std::copy(to,to+9,p.begin()+12);std::copy(from,from+9,p.begin()+21);
    p[31]=15;p[32]=.8f;p[33]=-90;p[34]=75;p[35]=.01f;p[36]=.12f;
    p[37]=685;p[38]=300;p[39]=1023;p[40]=.01f;p[41]=float(std::pow(10.0,(95.0-685.0)/300.0));
    for(unsigned mode=0;mode<2;++mode) for(uint32_t count:{0u,1u,255u,256u,257u,4097u}) {
        p[30]=float(mode);
        std::vector<float> input(std::max(count,1u)*3);
        std::vector<double> expected(count*3);
        for(uint32_t i=0;i<count;++i) {
            for(unsigned c=0;c<3;++c) input[3*i+c]=-5.f+float((i*(7+c*3)+c*11)%211)/24.f;
            auto ref=di_reference(input.data()+3*i,p);
            std::copy(ref.begin(),ref.end(),expected.begin()+3*i);
        }
        // The compensated path includes two colour matrices, cube roots,
        // hue trigonometry and a log; bound their accumulated float error.
        if(!run(gpu,"spk_di_encode",input,p,expected,error,mode?8e-6:2e-6))return false;
    }
    return true;
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::string error;
    std::unique_ptr<Gpu> gpu(Gpu::create_vulkan(std::string(argv[1])+"/vulkan",error));
    if(!gpu||!latitude(gpu.get(),error)||!cineon(gpu.get(),error)) { std::fprintf(stderr,"%s\n",error.c_str());return 1; }
    std::printf("%zu photo kernel reference cases passed on %s\n",cases,gpu->device_name().c_str());
    return 0;
}
