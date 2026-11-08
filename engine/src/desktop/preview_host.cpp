#include "preview_host.hpp"
#include "io/image_writer.hpp"
#include "json.hpp"
#include "params.hpp"
#include "../../host/white_balance.hpp"
#include "../../host/image_io.hpp"
#include <libraw/libraw.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace spk::desktop {
ProcessControls default_process_controls() {
    // Product defaults and names from Params.swift / FilmEdge.swift. These
    // controls cross the same C ABI as the Mac app, not a second effect model.
    return {{"auto_exposure",false},{"auto_exposure_method",std::string("balanced")},
        {"raw_as_shot",true},{"raw_temperature",5500.0},{"raw_tint",0.0},
        {"ui_print_effects",true},{"ui_scan_film",false},
        {"ui_edge_held_geometry",std::string("")},
        {"post_enabled",true},{"post_temperature",0.0},{"post_tint",0.0},{"post_exposure",0.0},
        {"post_contrast",0.0},{"post_brightness",0.0},{"post_saturation",0.0},
        {"post_highlights",0.0},{"post_shadows",0.0},{"post_black_point",0.0},{"post_white_point",0.0},
        {"post_vignette",0.0},{"post_vignette_midpoint",50.0},
        {"post_master_hue",0.0},{"post_master_saturation",0.0},{"post_master_luminance",0.0},
        {"post_shadows_hue",0.0},{"post_shadows_saturation",0.0},{"post_shadows_luminance",0.0},
        {"post_midtones_hue",0.0},{"post_midtones_saturation",0.0},{"post_midtones_luminance",0.0},
        {"post_highlights_hue",0.0},{"post_highlights_saturation",0.0},{"post_highlights_luminance",0.0},
        {"post_curves",std::string("{\"rgb\":[[0,0],[1,1]],\"luma\":[[0,0],[1,1]],\"r\":[[0,0],[1,1]],\"g\":[[0,0],[1,1]],\"b\":[[0,0],[1,1]]}")},
        {"overscan_active",false},{"overscan_format",std::string("135")},{"overscan_mode",std::string("strip")},
        {"overscan_gate",std::string("auto")},{"overscan_holes",std::string("white")},{"overscan_carrier",std::string("none")},
        {"overscan_camera_seed",1.0},{"overscan_frame_seed",1.0},{"overscan_frame_number",0.0},
        {"overscan_fog",1.0},{"overscan_leaks",0.0},{"overscan_edge_text",std::string("")},{"overscan_f_number",0.0},
        {"date_imprint_active",false},{"date_imprint_text",std::string("")},{"date_imprint_style",std::string("lcd")},
        {"date_imprint_placement",std::string("frame")},{"date_imprint_corner",std::string("br")},
        {"date_imprint_ev",3.5},{"date_imprint_size",1.0},{"date_imprint_inset_x",3.0},{"date_imprint_inset_y",2.4},
        {"antihalation_removed",false},{"grain_sublayers_active",true},{"halation_scatter_amount",1.0},
        {"halation_boost_ev",0.0},{"dir_couplers_active",true},{"dir_couplers_amount",1.0},
        {"density_curve_gamma",1.0},{"preflash_exposure",0.0},{"filter_shift_scale",40.0},
        {"extended_dynamic_range",false},{"digital_intermediate",false},
        {"scanner_white_correction",false},{"scanner_black_correction",false},
        {"scene_latitude_active",false},{"scene_latitude_norm",std::string("power")},
        {"scene_latitude_highlight_knee",2.0},{"scene_latitude_highlight_room",0.0},
        {"scene_latitude_shadow_knee",-2.0},{"scene_latitude_shadow_room",0.0},
        {"scene_latitude_rolloff",2.0},{"scene_latitude_max_lift",4.0},
        {"ui_highlight_pull_back",0.0},{"ui_shadow_pull_back",0.0},
        {"camera_filter",std::string("")},{"lens_blur_um",0.0},
        {"geometry_crop_x",0.0},{"geometry_crop_y",0.0},{"geometry_crop_w",1.0},{"geometry_crop_h",1.0},
        {"geometry_rotation_deg",0.0},{"geometry_quarter_turns",0.0},{"geometry_flip_h",false},{"geometry_flip_v",false}};
}
namespace {
Json control_json(const ControlValue& value) {return std::visit([](const auto& v){return Json(v);},value);}
}
bool validate_process_controls(const ProcessControls& controls,std::string& error) {
    const auto defaults=default_process_controls();Json delta=Json::object();
    for(const auto& [key,value]:controls){
        const auto found=defaults.find(key);
        if(found==defaults.end()||found->second.index()!=value.index()){error="Unsupported process control: "+key;return false;}
        if(key=="raw_temperature"){const double v=std::get<double>(value);if(!std::isfinite(v)||v<2000||v>12000){error="Temperature must be 2000..12000 K";return false;}}
        else if(key=="raw_tint"){const double v=std::get<double>(value);if(!std::isfinite(v)||v< -150||v>150){error="Tint must be -150..150";return false;}}
        else if(key.starts_with("ui_")){if(std::holds_alternative<double>(value)){const double v=std::get<double>(value);if(!std::isfinite(v)||v<0||v>12){error="Placement must be 0..12 stops";return false;}}}
        else if(key.starts_with("post_")){
            if(key=="post_curves"){
                const auto& text=std::get<std::string>(value);Json curves;std::string reason;
                if(text.size()>16000||!Json::parse(text,curves,reason)||curves.fields().size()!=5){error="Invalid curve document";return false;}
                for(const auto* channel:{"rgb","luma","r","g","b"}){
                    const auto& points=curves.at(channel).items();if(points.size()<2||points.size()>32){error="Curve requires 2..32 points";return false;}
                    double previous=-1;
                    for(const auto& point:points){if(point.items().size()!=2||!point.items()[0].is_number()||!point.items()[1].is_number()){error="Invalid curve point";return false;}
                        const double x=point.items()[0].as_double(),y=point.items()[1].as_double();if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1||x<=previous){error="Curve points must be ordered within 0..1";return false;}previous=x;}
                    if(points.front().items()[0].as_double()!=0||points.back().items()[0].as_double()!=1){error="Curve endpoints must be at 0 and 1";return false;}
                }
            }else if(std::holds_alternative<double>(value)){
                const double n=std::get<double>(value);double lo=-100,hi=100;
                if(key=="post_exposure"){lo=-3;hi=3;}
                else if(key=="post_contrast"||key=="post_brightness"){lo=-50;hi=50;}
                else if(key=="post_black_point"||key=="post_white_point"){lo=0;hi=50;}
                else if(key=="post_vignette_midpoint"){lo=0;hi=100;}
                else if(key.ends_with("_hue")){lo=0;hi=360;}
                else if(key.ends_with("_luminance")){lo=-1;hi=1;}
                else if(key!="post_saturation"&&key.ends_with("_saturation")){lo=0;hi=1;}
                if(!std::isfinite(n)||n<lo||n>hi){error="Post-development value out of range: "+key;return false;}
            }
        }
        else if(!key.starts_with("raw_"))delta.set(key,control_json(value));
    }
    std::string parameter;return validate_delta(delta,error,parameter);
}
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using EnginePtr = std::unique_ptr<spk_engine, decltype(&spk_engine_destroy)>;
using SessionPtr = std::unique_ptr<spk_session, decltype(&spk_session_release)>;

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string utf8(const fs::path& path) {
    const auto s = path.u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}

Json read_json(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "Cannot read bundled stock profile: " + utf8(path));
    const auto length = file.tellg();
    require(length > 0 && length <= 16 * 1024 * 1024, "Invalid bundled stock profile size");
    std::string text(std::size_t(length), '\0');
    file.seekg(0);
    require(bool(file.read(text.data(), std::streamsize(length))), "Cannot read complete stock profile");
    Json value;
    std::string error;
    const bool ok = Json::parse(text, value, error);
    require(ok, "Bundled stock profile: " + error);
    return value;
}

Catalog read_catalog(const fs::path& resources) {
    Catalog catalog;
    std::set<std::string> ids;
    for (const auto& entry : fs::directory_iterator(resources / "profiles")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const Json profile = read_json(entry.path());
        const auto& info = profile.at("info");
        Stock stock{info.at("stock").as_string(), info.at("name").as_string()};
        const auto& stage = info.at("stage").as_string();
        const auto& type = info.at("type").as_string();
        require(!stock.id.empty() && stock.id == utf8(entry.path().stem()) &&
                    std::all_of(stock.id.begin(), stock.id.end(), [](unsigned char c) {
                        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                               (c >= '0' && c <= '9') || c == '_';
                    }), "Invalid stock identifier in bundled profile");
        require(!stock.label.empty() && ids.insert(stock.id).second,
                "Missing or duplicate bundled stock metadata");
        require(type == "negative" || type == "positive", "Unknown bundled stock type: " + type);
        stock.is_positive = type == "positive";
        stock.monochrome = info.at("channel_model").as_string() == "bw";
        stock.target_print = info.at("target_print").as_string();
        if (stage == "filming") catalog.films.push_back(std::move(stock));
        else if (stage == "printing") catalog.papers.push_back(std::move(stock));
        else throw std::runtime_error("Unknown bundled stock stage: " + stage);
    }
    const auto by_label = [](const Stock& a, const Stock& b) { return a.label < b.label; };
    std::sort(catalog.films.begin(), catalog.films.end(), by_label);
    std::sort(catalog.papers.begin(), catalog.papers.end(), by_label);
    require(!catalog.films.empty() && !catalog.papers.empty(), "Bundled film/paper catalog is empty");
    return catalog;
}

bool validate_settings(const RenderSettings& settings, const Catalog& catalog) {
    std::string controls_error;
    require(validate_process_controls(settings.controls,controls_error),controls_error);
    const auto contains = [](const std::vector<Stock>& stocks, const std::string& id) {
        return std::any_of(stocks.begin(), stocks.end(), [&](const Stock& s) { return s.id == id; });
    };
    const auto film = std::find_if(catalog.films.begin(), catalog.films.end(),
        [&](const Stock& stock) { return stock.id == settings.film_stock; });
    require(film != catalog.films.end(), "Unknown film stock: " + settings.film_stock);
    require(contains(catalog.papers, settings.print_stock), "Unknown paper stock: " + settings.print_stock);
    const auto paper = std::find_if(catalog.papers.begin(), catalog.papers.end(),
        [&](const Stock& stock) { return stock.id == settings.print_stock; });
    const bool scan=film->is_positive||std::get<bool>(settings.controls.at("ui_scan_film"));
    require(scan || film->monochrome == paper->monochrome,
            "Film and paper must both be colour or both be black and white");
    require(std::isfinite(settings.print_exposure) && settings.print_exposure >= 0.05 &&
                settings.print_exposure <= 20.0, "Print exposure must be between 0.05 and 20");
    const auto range = [](double value, double low, double high, const char* label) {
        require(std::isfinite(value) && value >= low && value <= high,
                std::string(label) + " must be between " + std::to_string(low) + " and " + std::to_string(high));
    };
    // Reject before touching the live session, including NaN/Inf values
    // that cannot be represented faithfully in the JSON parameter delta.
    range(settings.film_exposure_ev, -8.0, 8.0, "Film exposure (EV)");
    range(settings.film_format_mm, 4.0, 200.0, "Film format (mm)");
    range(settings.grain_amount, 0.0, 2.0, "Grain amount");
    range(settings.halation_amount, 0.0, 4.0, "Halation amount");
    range(settings.glare_amount, 0.0, 30.0, "Glare amount");
    range(settings.y_filter_shift, -1.0, 1.0, "Yellow filter shift");
    range(settings.m_filter_shift, -1.0, 1.0, "Magenta filter shift");
    return scan;
}

double grain_amount(const RenderSettings& settings) {
    double own = 1.0;
    if (settings.film_stock == "kodak_tri_x_400") own = 1.70;
    else if (settings.film_stock == "ilford_hp5_plus_400") own = 1.49;
    else if (settings.film_stock == "kodak_tmax_100") own = 0.74;
    else if (settings.film_stock == "fujifilm_neopan_acros_100_ii") own = 0.64;
    return std::clamp(settings.grain_amount * own, 0.0, 2.0);
}
void paper_defaults(Json& params, const RenderSettings& settings) {
    if (settings.print_stock != "ilford_multigrade_iv_rc") return;
    // Params.swift: grade 2, before the user's M/Y offsets. Profile arrays
    // remain unchanged; this is the application's enlarger configuration.
    params.set("c_filter_neutral", Json(0.0));
    params.set("m_filter_neutral", Json(0.0));
    params.set("y_filter_neutral", Json(68.0));
}
Json opening_params(const RenderSettings& settings, bool scan_film) {
    // Unexposed effects keep their engine defaults. The exposed settings
    // start at those same defaults, so opening an unedited image retains
    // the existing look, including the physical 35 mm grain/halation scale.
    Json params = Json::object();
    params.set("input_color_space", Json(std::string("ProPhoto RGB")));
    params.set("input_cctf_decoding", Json(false));
    params.set("output_color_space", Json(std::string("sRGB")));
    params.set("output_cctf_encoding", Json(true));
    params.set("extended_dynamic_range", Json(false));
    params.set("film_stock", Json(settings.film_stock));
    // Desktop policy matches the macOS session: a slide is already a
    // positive and must bypass negative-to-paper printing. The generic
    // engine API still permits deliberate positive-to-paper experiments.
    params.set("scan_film", Json(scan_film));
    params.set("print_stock", Json(settings.print_stock));
    params.set("print_exposure", Json(settings.print_exposure));
    params.set("exposure_compensation_ev", Json(settings.film_exposure_ev));
    params.set("film_format_mm", Json(settings.film_format_mm));
    params.set("grain_active", Json(settings.grain_active));
    params.set("grain_amount", Json(grain_amount(settings)));
    params.set("halation_active", Json(settings.halation_active));
    params.set("halation_amount", Json(settings.halation_amount));
    params.set("glare_active", Json(settings.glare_active));
    params.set("glare_amount", Json(settings.glare_amount));
    params.set("y_filter_shift", Json(settings.y_filter_shift));
    params.set("m_filter_shift", Json(settings.m_filter_shift));
    paper_defaults(params, settings);
    for(const auto& [key,value]:settings.controls)if(!key.starts_with("raw_")&&!key.starts_with("ui_")&&!key.starts_with("post_"))params.set(key,control_json(value));
    // These stage rules are resolved from material metadata; a carried paper
    // setting cannot put a slide back through the enlarger or enable B&W EDR.
    if(scan_film)params.set("digital_intermediate",Json(false));
    if(settings.print_stock=="ilford_multigrade_iv_rc")params.set("extended_dynamic_range",Json(false));
    if(!std::get<bool>(settings.controls.at("ui_print_effects"))){params.set("glare_active",Json(false));params.set("preflash_exposure",Json(0.0));params.set("extended_dynamic_range",Json(false));}
    return params;
}

Json changed_params(const RenderSettings& before, const RenderSettings& after, bool scan_film) {
    Json delta = Json::object();
    // Sending an unchanged film_stock still invalidates the negative in the
    // C API. A print-only edit must never include it incidentally.
    if (before.film_stock != after.film_stock) {
        delta.set("film_stock", Json(after.film_stock));
        // Send false too when returning to negative film, otherwise the
        // previous slide's direct-scan mode would survive the stock change.
        delta.set("scan_film", Json(scan_film));
    }
    if (before.print_stock != after.print_stock) delta.set("print_stock", Json(after.print_stock));
    if (before.print_exposure != after.print_exposure) delta.set("print_exposure", Json(after.print_exposure));
    // The C API retains carried user controls when loading another stock.
    // Send only changed fields: an unchanged SHOOT field in a paper-only
    // delta would unnecessarily invalidate the cached grain realization.
    if (before.film_exposure_ev != after.film_exposure_ev) delta.set("exposure_compensation_ev", Json(after.film_exposure_ev));
    if (before.film_format_mm != after.film_format_mm) delta.set("film_format_mm", Json(after.film_format_mm));
    if (before.grain_active != after.grain_active) delta.set("grain_active", Json(after.grain_active));
    if (before.grain_amount != after.grain_amount || before.film_stock != after.film_stock)
        delta.set("grain_amount", Json(grain_amount(after)));
    if (before.halation_active != after.halation_active) delta.set("halation_active", Json(after.halation_active));
    if (before.halation_amount != after.halation_amount) delta.set("halation_amount", Json(after.halation_amount));
    if (before.glare_active != after.glare_active) delta.set("glare_active", Json(after.glare_active));
    if (before.glare_amount != after.glare_amount) delta.set("glare_amount", Json(after.glare_amount));
    if (before.y_filter_shift != after.y_filter_shift) delta.set("y_filter_shift", Json(after.y_filter_shift));
    if (before.m_filter_shift != after.m_filter_shift) delta.set("m_filter_shift", Json(after.m_filter_shift));
    if (before.film_stock != after.film_stock || before.print_stock != after.print_stock) paper_defaults(delta, after);
    for(const auto& [key,value]:after.controls){const auto it=before.controls.find(key);if(!key.starts_with("raw_")&&!key.starts_with("ui_")&&!key.starts_with("post_")&&(it==before.controls.end()||it->second!=value))delta.set(key,control_json(value));}
    if(scan_film)delta.set("digital_intermediate",Json(false));
    if(after.print_stock=="ilford_multigrade_iv_rc")delta.set("extended_dynamic_range",Json(false));
    if(before.controls.at("ui_scan_film")!=after.controls.at("ui_scan_film"))delta.set("scan_film",Json(scan_film));
    if(before.controls.at("ui_print_effects")!=after.controls.at("ui_print_effects")){
        const bool on=std::get<bool>(after.controls.at("ui_print_effects"));
        delta.set("glare_active",Json(on&&after.glare_active));delta.set("preflash_exposure",on?control_json(after.controls.at("preflash_exposure")):Json(0.0));
        delta.set("extended_dynamic_range",Json(on&&after.print_stock!="ilford_multigrade_iv_rc"&&std::get<bool>(after.controls.at("extended_dynamic_range"))));
    }
    if(!std::get<bool>(after.controls.at("ui_print_effects"))){delta.set("glare_active",Json(false));delta.set("preflash_exposure",Json(0.0));delta.set("extended_dynamic_range",Json(false));}
    return delta;
}

SessionPtr open_session(spk_engine* engine, const io::DecodedRaw& decoded,
                        const RenderSettings& settings, bool scan_film) {
    const spk_image input{decoded.rgb.data(), decoded.width, decoded.height, 3};
    const auto params = opening_params(settings, scan_film).dump();
    // No reply is needed here; the host has no allocator or JSON lifetime to
    // share with the UI, and the C API validates the complete delta itself.
    SessionPtr session(spk_open(engine, &input, params.c_str(), nullptr), &spk_session_release);
    require(bool(session), "Open image: " + std::string(spk_last_error(engine)));
    return session;
}
std::string latitude(spk_session* session) {
    char* reply=nullptr;const auto status=spk_scene_latitude(session,"{}",&reply);
    std::string value=reply?reply:"{}";spk_string_free(reply);
    return status==SPK_OK?value:"{}";
}
}  // namespace

Frame::~Frame() { spk_result_free(&result_); }
RenderSettings compatible_paper(RenderSettings settings, const Catalog& catalog, bool prefer_target) {
    const auto film = std::find_if(catalog.films.begin(), catalog.films.end(),
        [&](const Stock& stock) { return stock.id == settings.film_stock; });
    if (film == catalog.films.end() || film->is_positive) return settings;
    const Stock* current = nullptr;
    const Stock* target = nullptr;
    const Stock* fallback = nullptr;
    for (const auto& paper : catalog.papers) {
        if (paper.monochrome != film->monochrome) continue;
        if (!fallback) fallback = &paper;
        if (paper.id == settings.print_stock) current = &paper;
        if (paper.id == film->target_print) target = &paper;
    }
    const auto* paper = prefer_target && target ? target : current ? current : target ? target : fallback;
    if (paper) settings.print_stock = paper->id;
    return settings;
}

std::vector<std::uint8_t> rgba16_to_bgra8(const spk_result& result) {
    require(result.rgba16 && result.width && result.height && result.row_stride_px >= result.width,
            "Invalid RGBA16 display dimensions or row pitch");
    constexpr auto max_size = std::numeric_limits<std::size_t>::max();
    // Both the source address calculation and destination allocation must fit;
    // checking pixels alone would miss overflow of the row padding.
    require(std::size_t(result.height) <= max_size / result.row_stride_px / 4 / sizeof(std::uint16_t) &&
                std::size_t(result.height) <= max_size / result.width / 4,
            "RGBA16 display dimensions overflow address space");
    const std::size_t count = std::size_t(result.width) * result.height * 4;
    require(count <= std::vector<std::uint8_t>().max_size(), "BGRA8 display allocation is too large");
    std::vector<std::uint8_t> bgra(count);
    const auto byte = [](std::uint16_t value) {
        return std::uint8_t((std::uint32_t(value) + 128u) / 257u);
    };
    for (std::size_t y = 0; y < result.height; ++y) {
        const auto* src = result.rgba16 + y * result.row_stride_px * 4;
        auto* dst = bgra.data() + y * result.width * 4;
        for (std::size_t x = 0; x < result.width; ++x) {
            if (src[4 * x + 3] != 65535)
                throw std::runtime_error("Display requires opaque RGBA16 pixels");
            dst[4 * x] = byte(src[4 * x + 2]);
            dst[4 * x + 1] = byte(src[4 * x + 1]);
            dst[4 * x + 2] = byte(src[4 * x]);
            dst[4 * x + 3] = 255;
        }
    }
    return bgra;
}

struct PreviewHost::Impl {
    fs::path resources;
    Catalog catalog;
    double engine_create_ms = 0.0;
    EnginePtr engine{nullptr, &spk_engine_destroy};
    // Declaration order makes session release precede engine destruction.
    SessionPtr session{nullptr, &spk_session_release};
    // Retained only for reopening after a failed settings/render operation.
    // Ordinary parameter edits reuse the current GPU source and negative.
    std::shared_ptr<const io::DecodedRaw> decoded;
    FramePtr frame;
};

PreviewHost::PreviewHost(const fs::path& resources) : impl_(std::make_unique<Impl>()) {
    impl_->resources = fs::absolute(resources);
    const auto path = utf8(impl_->resources);
    require(std::all_of(path.begin(), path.end(), [](unsigned char c) { return c < 128; }),
            "Engine resources directory currently requires an ASCII path");
    require(fs::is_directory(impl_->resources), "Engine resources directory is missing");
    impl_->catalog = read_catalog(impl_->resources);
    const auto started = Clock::now();
    impl_->engine.reset(spk_engine_create(path.c_str(), nullptr));
    impl_->engine_create_ms = elapsed(started);
    require(bool(impl_->engine), "Engine creation failed: " + std::string(spk_last_error(nullptr)));
}

PreviewHost::~PreviewHost() = default;
const Catalog& PreviewHost::catalog() const noexcept { return impl_->catalog; }
double PreviewHost::engine_create_ms() const noexcept { return impl_->engine_create_ms; }
FramePtr PreviewHost::current() const noexcept { return impl_->frame; }

FramePtr PreviewHost::open_raw(const fs::path& source, DecodeMode mode, const RenderSettings& settings) {
    return open_image(source,mode,settings);
}
FramePtr PreviewHost::open_image(const fs::path& source, DecodeMode mode, const RenderSettings& settings) {
    const auto started = Clock::now();
    const bool scan_film = validate_settings(settings, impl_->catalog);
    require(mode == DecodeMode::compatible16 || mode == DecodeMode::headroom, "Unknown RAW decode mode");
    const auto path = fs::absolute(source);
    require(fs::is_regular_file(path), "Image input is missing: " + utf8(path));
    auto next = std::shared_ptr<Frame>(new Frame());
    next->source = path;
    next->decode_mode = mode;
    next->settings = settings;
    next->scan_film = scan_film;
    next->is_raw = spkhost::sniff_kind(path)==spkhost::FileKind::Raw;
    io::DecodedRaw decoded;
    std::string error;
    auto t = Clock::now();
    if(!next->is_raw){
        spkhost::FloatImage raster;spkhost::Probe probe;
        spkhost::DecodeOptions options;options.strict_icc=true;
        const bool ok=spkhost::decode_linear_prophoto(path,options,raster,probe,error);
        require(ok,"Image decode: "+error);
        decoded.width=raster.width;decoded.height=raster.height;decoded.rgb=std::move(raster.rgb);
        decoded.metadata.make=probe.metadata.make;decoded.metadata.model=probe.metadata.model;
        decoded.metadata.decoder_name=spkhost::kind_name(probe.kind);
        decoded.timings.total_ms=elapsed(t);
    }else{
    std::array<float,4> user_mul{};const bool custom_wb=!std::get<bool>(settings.controls.at("raw_as_shot"));
    auto metadata=std::make_unique<LibRaw>();
    if(metadata->open_file(path.c_str())==LIBRAW_SUCCESS){
        next->capture_timestamp=static_cast<std::int64_t>(metadata->imgdata.other.timestamp);
        spkhost::as_shot_temperature_tint(*metadata,next->as_shot_temperature,next->as_shot_tint);
        if(custom_wb)require(spkhost::multipliers_for(*metadata,std::get<double>(settings.controls.at("raw_temperature")),std::get<double>(settings.controls.at("raw_tint")),user_mul,error),error);
    }else require(!custom_wb,"Cannot read camera matrix for custom white balance");
    metadata.reset();
    require(!custom_wb||mode==DecodeMode::compatible16,"Custom camera white balance currently requires compatible RAW decode");
    const bool decoded_ok = mode == DecodeMode::headroom
        ? io::decode_raw_headroom(path, decoded, error)
        : io::decode_raw_compatible(path, custom_wb?&user_mul:nullptr, decoded, error);
    require(decoded_ok, "RAW decode: " + error);
    }
    next->timings.decode_ms = elapsed(t);
    next->metadata = decoded.metadata;
    next->decode_timings = decoded.timings;
    t = Clock::now();
    auto session = open_session(impl_->engine.get(), decoded, settings, scan_film);
    next->timings.open_ms = elapsed(t);
    t = Clock::now();
    const auto status = spk_render(session.get(), "full", &next->result_);
    require(status == SPK_OK, "Render: " + std::string(spk_last_error(impl_->engine.get())));
    next->timings.render_ms = elapsed(t);
    next->timings.engine_render_ms = next->result_.elapsed_ms;
    t = Clock::now();
    next->bgra8 = rgba16_to_bgra8(next->result_);
    next->timings.display_ms = elapsed(t);
    // Commit only after all allocations and GPU/CPU work succeeded.
    impl_->session = std::move(session);
    impl_->decoded = std::make_shared<const io::DecodedRaw>(std::move(decoded));
    next->original = impl_->decoded;
    next->latitude_json = latitude(impl_->session.get());
    impl_->frame = next;
    next->timings.total_ms = elapsed(started);
    return next;
}

FramePtr PreviewHost::render(const RenderSettings& settings) {
    const auto started = Clock::now();
    require(bool(impl_->frame), "Open an image before rendering");
    for(const auto& [key,value]:settings.controls)if(impl_->frame->is_raw&&key.starts_with("raw_")&&impl_->frame->settings.controls.at(key)!=value)
        return open_image(impl_->frame->source,impl_->frame->decode_mode,settings);
    const bool scan_film = validate_settings(settings, impl_->catalog);
    auto next = std::shared_ptr<Frame>(new Frame());
    next->source = impl_->frame->source;
    next->decode_mode = impl_->frame->decode_mode;
    next->settings = settings;
    next->scan_film = scan_film;
    next->is_raw = impl_->frame->is_raw;
    next->metadata = impl_->frame->metadata;
    next->decode_timings = impl_->frame->decode_timings;
    next->original = impl_->frame->original;
    next->as_shot_temperature=impl_->frame->as_shot_temperature;next->as_shot_tint=impl_->frame->as_shot_tint;
    next->capture_timestamp=impl_->frame->capture_timestamp;
    const auto delta = changed_params(impl_->frame->settings, settings, scan_film);
    try {
        auto t = Clock::now();
        if (!impl_->session) {
            // C API settings changes may fail after applying part of their
            // state. Never reuse such a session; rebuild from the retained
            // decoded input on the next request, without decoding the file.
            impl_->session = open_session(impl_->engine.get(), *impl_->decoded, settings, scan_film);
            next->timings.open_ms = elapsed(t);
        } else if (!delta.fields().empty()) {
            const auto text = delta.dump();
            const auto status = spk_set_params(impl_->session.get(), text.c_str(), nullptr);
            require(status == SPK_OK, "Apply settings: " + std::string(spk_last_error(impl_->engine.get())));
            next->timings.params_ms = elapsed(t);
        }
        t = Clock::now();
        const auto status = spk_reprint(impl_->session.get(), "full", &next->result_);
        require(status == SPK_OK, "Render: " + std::string(spk_last_error(impl_->engine.get())));
        next->timings.render_ms = elapsed(t);
        next->timings.engine_render_ms = next->result_.elapsed_ms;
        t = Clock::now();
        next->bgra8 = rgba16_to_bgra8(next->result_);
        next->timings.display_ms = elapsed(t);
    } catch (...) {
        impl_->session.reset();
        throw;
    }
    next->latitude_json = latitude(impl_->session.get());
    impl_->frame = next;
    next->timings.total_ms = elapsed(started);
    return next;
}

RenderSettings PreviewHost::place_scene(const RenderSettings& settings,double highlights,double shadows) {
    require(bool(impl_->frame),"Open an image before placing the scene");
    if(impl_->frame->settings!=settings)render(settings);
    Json request=Json::object();request.set("highlight_pull_back",Json(highlights));request.set("shadow_pull_back",Json(shadows));
    char* reply=nullptr;const auto status=spk_scene_latitude(impl_->session.get(),request.dump().c_str(),&reply);
    std::string value=reply?reply:"{}";spk_string_free(reply);
    require(status==SPK_OK,"Cannot measure scene placement");Json result;std::string error;require(Json::parse(value,result,error),error);
    const auto& fit=result.at("fit");require(fit.at("valid").as_bool(),"Scene placement cannot fit these settings");
    auto next=settings;next.controls["ui_highlight_pull_back"]=highlights;next.controls["ui_shadow_pull_back"]=shadows;
    for(const auto& [key,value]:fit.at("params_delta").fields()){
        if(value.is_bool())next.controls[key]=value.as_bool();else if(value.is_number())next.controls[key]=value.as_double();else if(value.is_string())next.controls[key]=value.as_string();
    }
    return next;
}
RenderSettings PreviewHost::solve_settings(const RenderSettings& settings) {
    require(bool(impl_->frame),"Open an image before solving");
    if(impl_->frame->settings!=settings)render(settings);
    char* reply=nullptr;
    const auto status=spk_solve(impl_->session.get(),"both",&reply);
    std::string text=reply?reply:"";spk_string_free(reply);
    require(status==SPK_OK,"Solve: "+std::string(spk_last_error(impl_->engine.get())));
    Json result;std::string error;require(Json::parse(text,result,error),error);
    auto next=settings;
    next.film_exposure_ev=result.at("solved_params").at("exposure_compensation_ev").as_double(settings.film_exposure_ev);
    next.controls["auto_exposure"]=false;next.y_filter_shift=0;next.m_filter_shift=0;next.print_exposure=1;
    return next;
}
void PreviewHost::export_tiff(const Frame& frame, const fs::path& destination) const {
    const auto& pixels = frame.pixels();
    std::string error;
    const bool written = io::write_srgb_tiff(destination, pixels.rgba16, pixels.width, pixels.height,
                                            pixels.row_stride_px, impl_->resources / "io" / "sRGB.icc", error);
    require(written, "TIFF export: " + error);
}

}  // namespace spk::desktop
