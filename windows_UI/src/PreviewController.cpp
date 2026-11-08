#include "PreviewController.hpp"
#include <QColorSpace>
#include <QMetaObject>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QBuffer>
#include <QImageReader>
#include <QTransform>
#include <libraw/libraw.h>
#include <QStandardPaths>
#include <cmath>
#include <array>
#include <type_traits>

namespace {
namespace desktop=spk::desktop;
namespace output=desktop::output;
namespace editing=desktop::editing;
QString text(const std::string& s) { return QString::fromUtf8(s.data(),qsizetype(s.size())); }
std::filesystem::path path(const QUrl& u) { return std::filesystem::path(u.toLocalFile().toStdWString()); }
QVariantList list(const std::vector<desktop::Stock>& stocks,const std::filesystem::path& resources) {
    QVariantList result;
    for(const auto& s:stocks){const auto cover=resources/"film_covers"/(s.id+".jpg");
        result.push_back(QVariantMap{{"id",text(s.id)},{"name",text(s.label)},{"positive",s.is_positive},{"monochrome",s.monochrome},
            {"cover",std::filesystem::exists(cover)?QUrl::fromLocalFile(QString::fromStdWString(cover.wstring())):QUrl{}}});}
    return result;
}
bool sameSettings(desktop::RenderSettings a, desktop::RenderSettings b) {
    const bool exposure=std::abs(a.print_exposure-b.print_exposure)<1e-12;
    a.print_exposure=b.print_exposure;
    return exposure&&a==b;
}
bool sameLayerOne(desktop::RenderSettings a,desktop::RenderSettings b){
    std::erase_if(a.controls,[](const auto& item){return item.first.starts_with("post_");});
    std::erase_if(b.controls,[](const auto& item){return item.first.starts_with("post_");});
    return sameSettings(std::move(a),std::move(b));
}
bool sameState(const editing::State& a,const editing::State& b) {
    return sameSettings(a.settings,b.settings)&&a.mode==b.mode&&a.geometry==b.geometry;
}
struct HistoryDescription {QString label,detail;};
HistoryDescription describeEdit(const editing::State& before,const editing::State& after,const desktop::Catalog& catalog){
    QStringList changes;const auto& a=before.settings;const auto& b=after.settings;
    auto number=[](double v){return QString::number(v,'f',2);};
    auto stock=[](const auto& stocks,const std::string& id){for(const auto& stock:stocks)if(stock.id==id)return text(stock.label);return QStringLiteral("材料选择");};
    if(a.film_stock!=b.film_stock)changes.append("胶片："+stock(catalog.films,b.film_stock));
    if(a.print_stock!=b.print_stock)changes.append("相纸："+stock(catalog.papers,b.print_stock));
    if(a.film_exposure_ev!=b.film_exposure_ev)changes.append("胶片曝光："+number(b.film_exposure_ev)+" EV");
    if(std::abs(a.print_exposure-b.print_exposure)>1e-12)changes.append("放大机亮度："+number(-std::log2(b.print_exposure))+" EV");
    if(a.film_format_mm!=b.film_format_mm)changes.append("胶片画幅："+number(b.film_format_mm)+" mm");
    if(a.y_filter_shift!=b.y_filter_shift)changes.append("黄滤镜："+number(b.y_filter_shift));
    if(a.m_filter_shift!=b.m_filter_shift)changes.append("品红滤镜："+number(b.m_filter_shift));
    auto effect=[&](const char* name,bool oldOn,bool on,double oldAmount,double amount){
        if(oldOn!=on)changes.append(QString::fromUtf8(name)+(on?"：开启":"：关闭"));else if(oldAmount!=amount)changes.append(QString::fromUtf8(name)+"："+number(amount));
    };
    effect("颗粒",a.grain_active,b.grain_active,a.grain_amount,b.grain_amount);
    effect("光晕",a.halation_active,b.halation_active,a.halation_amount,b.halation_amount);
    effect("耀光",a.glare_active,b.glare_active,a.glare_amount,b.glare_amount);
    if(before.mode!=after.mode)changes.append(after.mode==desktop::DecodeMode::compatible16?"RAW 解码：兼容模式":"RAW 解码：高光扩展");
    if(before.geometry.crop!=after.geometry.crop)changes.append("裁剪取景");
    if(before.geometry.quarterTurns!=after.geometry.quarterTurns)changes.append("旋转照片");
    if(before.geometry.flipHorizontal!=after.geometry.flipHorizontal||before.geometry.flipVertical!=after.geometry.flipVertical)changes.append("翻转照片");
    if(before.geometry.straightenDegrees!=after.geometry.straightenDegrees)changes.append("拉直："+number(after.geometry.straightenDegrees)+"°");
    static const QHash<QString,QString> names={{"post_enabled","显影后调整"},{"post_exposure","显影后曝光"},{"post_contrast","对比度"},
        {"post_brightness","亮度"},{"post_temperature","显影后色温"},{"post_tint","显影后色调"},{"post_saturation","饱和度"},
        {"post_highlights","高光"},{"post_shadows","阴影"},{"post_black_point","黑点"},{"post_white_point","白点"},
        {"post_vignette","暗角"},{"post_vignette_midpoint","暗角中心"},{"raw_as_shot","相机拍摄白平衡"},
        {"raw_temperature","相机色温"},{"raw_tint","相机色调"},{"auto_exposure","自动测光"},
        {"ui_print_effects","相纸效果"},{"ui_scan_film","直接扫描"},{"extended_dynamic_range","扩展动态范围"},
        {"camera_filter","镜头滤镜"},{"overscan_active","片边"},{"overscan_format","片边画幅"},{"date_imprint_active","日期后背"},
        {"date_imprint_text","日期后背文字"},{"ui_highlight_pull_back","高光置位"},{"ui_shadow_pull_back","阴影置位"}};
    for(const auto& [key,value]:b.controls){
        if(key.starts_with("geometry_")||key=="ui_edge_held_geometry")continue;
        const auto old=a.controls.find(key);if(old!=a.controls.end()&&old->second==value)continue;
        if(key=="post_curves"){changes.append("曲线调整");continue;}
        const auto label=names.value(text(key));
        if(!label.isEmpty())changes.append(label+"："+std::visit([&](const auto& v)->QString{
            using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,bool>)return v?"开启":"关闭";
            else if constexpr(std::is_same_v<T,double>)return number(v);else return text(v).left(48);
        },value));
        else if(key.starts_with("post_"))changes.append("色彩平衡");
        else if(key.starts_with("overscan_"))changes.append("片边参数");
        else if(key.starts_with("date_imprint_"))changes.append("日期后背参数");
        else if(key.starts_with("scene_latitude_"))changes.append("场景置位");
        else changes.append("胶片与相纸参数");
    }
    changes.removeDuplicates();if(changes.isEmpty())changes.append("照片调整");
    return {changes.size()==1?changes.first():changes.first()+QString(" 等 %1 项").arg(changes.size()),changes.join('\n')};
}
output::Geometry gateFramed(output::Geometry g,QSize source,const std::string& format){
    // Session+FilmEdge.gateFramed: retain centre/area, then fit the gate ratio.
    static const std::map<std::string,double> ratios={{"135",36.0/24},{"135_half",24.0/18},{"135_xpan",65.0/24},
        {"120_645",56.0/41.5},{"120_6x6",1},{"120_6x7",69.5/56},{"120_6x8",76.0/56},{"120_6x9",84.0/56},{"120_6x12",2},{"120_6x17",3}};
    const auto found=ratios.find(format);if(source.isEmpty()||found==ratios.end())return g;
    const double pw=g.crop.width()*source.width(),ph=g.crop.height()*source.height();
    const double ratio=pw>=ph?found->second:1/found->second;
    const double area=std::max(1.0,pw*ph);double w=std::sqrt(area*ratio)/source.width(),h=std::sqrt(area/ratio)/source.height();
    const double fit=std::min({1.0,1/w,1/h});w*=fit;h*=fit;
    const auto centre=g.crop.center();g.crop={std::clamp(centre.x()-w/2,0.0,1-w),std::clamp(centre.y()-h/2,0.0,1-h),w,h};return g;
}
std::string packGeometry(const output::Geometry& g){
    return QJsonDocument(QJsonArray{g.crop.x(),g.crop.y(),g.crop.width(),g.crop.height(),g.quarterTurns,g.flipHorizontal,g.flipVertical,g.straightenDegrees}).toJson(QJsonDocument::Compact).toStdString();
}
std::optional<output::Geometry> unpackGeometry(const std::string& text,QSize size){
    const auto a=QJsonDocument::fromJson(QByteArray::fromStdString(text)).array();if(a.size()!=8)return {};
    output::Geometry g;g.crop={a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble()};
    g.quarterTurns=a[4].toInt();g.flipHorizontal=a[5].toBool();g.flipVertical=a[6].toBool();g.straightenDegrees=a[7].toDouble();
    try{output::validateGeometry(size,g);return g;}catch(const std::exception&){return {};}
}
output::Geometry gateForSource(const desktop::RenderSettings& settings,output::Geometry g,const std::filesystem::path& source){
    if(!std::get<bool>(settings.controls.at("overscan_active")))return g;
    auto raw=std::make_unique<LibRaw>();
    if(raw->open_file(source.c_str())!=LIBRAW_SUCCESS||raw->adjust_sizes_info_only()!=LIBRAW_SUCCESS)return g;
    return gateFramed(g,QSize(raw->imgdata.sizes.iwidth,raw->imgdata.sizes.iheight),std::get<std::string>(settings.controls.at("overscan_format")));
}
desktop::RenderSettings withGeometry(desktop::RenderSettings s,const output::Geometry& geometry) {
    const auto g=std::get<bool>(s.controls.at("overscan_active"))?geometry:output::Geometry{};
    s.controls["geometry_crop_x"]=g.crop.x();s.controls["geometry_crop_y"]=g.crop.y();
    s.controls["geometry_crop_w"]=g.crop.width();s.controls["geometry_crop_h"]=g.crop.height();
    s.controls["geometry_quarter_turns"]=double(g.quarterTurns);s.controls["geometry_rotation_deg"]=g.straightenDegrees;
    s.controls["geometry_flip_h"]=g.flipHorizontal;s.controls["geometry_flip_v"]=g.flipVertical;
    return s;
}
QImage visibleImage(const desktop::FramePtr& frame,const QImage& full,const output::Geometry& geometry) {
    if(std::get<bool>(frame->settings.controls.at("overscan_active")))return full;
    if(geometry.quarterTurns==0&&!geometry.flipHorizontal&&!geometry.flipVertical&&geometry.straightenDegrees==0)
        return output::displayImage(full,geometry.crop);
    static const auto neutral=desktop::default_process_controls();
    return output::displayFrame(frame,geometry,&neutral);
}
QImage cameraThumbnail(const QUrl& url){
    auto raw=std::make_unique<LibRaw>();
    if(raw->open_file(path(url).c_str())!=LIBRAW_SUCCESS||raw->unpack_thumb()!=LIBRAW_SUCCESS)return {};
    int error=0;std::unique_ptr<libraw_processed_image_t,decltype(&LibRaw::dcraw_clear_mem)> memory(raw->dcraw_make_mem_thumb(&error),&LibRaw::dcraw_clear_mem);
    if(!memory||error!=LIBRAW_SUCCESS||memory->data_size>32*1024*1024)return {};
    QImage image;bool orientationApplied=false;
    if(memory->type==LIBRAW_IMAGE_JPEG){
        QByteArray data=QByteArray::fromRawData(reinterpret_cast<const char*>(memory->data),memory->data_size);
        QBuffer buffer(&data);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"jpeg");reader.setAutoTransform(true);
        image=reader.read();orientationApplied=reader.transformation()!=QImageIOHandler::TransformationNone;
    }else if(memory->type==LIBRAW_IMAGE_BITMAP&&memory->bits==8&&memory->colors==3){
        image=QImage(memory->data,memory->width,memory->height,qsizetype(memory->width)*3,QImage::Format_RGB888).copy();
    }
    if(image.isNull())return {};
    if(!orientationApplied){const int flip=raw->imgdata.sizes.flip;if(flip==3)image=image.transformed(QTransform().rotate(180));else if(flip==5)image=image.transformed(QTransform().rotate(270));else if(flip==6)image=image.transformed(QTransform().rotate(90));}
    image=image.scaled(180,120,Qt::KeepAspectRatio,Qt::SmoothTransformation);image.setColorSpace(QColorSpace::SRgb);return image;
}
output::ExportOptions exportOptions(int format,int quality,int maxEdge) {
    if(format<0||format>3||quality<1||quality>100||maxEdge<0||maxEdge>100000)
        throw std::runtime_error("Invalid export options");
    return {static_cast<output::Format>(format),quality,maxEdge};
}
}
QImage PreviewController::imageFromFrame(const desktop::FramePtr& frame) {
    // QImage is implicitly shared; the cleanup holder keeps the independently
    // owned engine result and its display bytes alive on the scene-graph side.
    auto held=std::make_unique<desktop::FramePtr>(frame);
    QImage image(frame->bgra8.data(),int(frame->width()),int(frame->height()),
                 qsizetype(frame->width())*4,QImage::Format_ARGB32,
                 [](void* p){delete static_cast<desktop::FramePtr*>(p);},held.get());
    if(image.isNull()) throw std::runtime_error("Cannot create the display image");
    held.release();
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

PreviewController::Reply PreviewController::preparePresentation(Reply reply){
    if(reply.error.isEmpty()&&reply.frame){try{
        const auto& controls=reply.wanted?reply.wanted->controls:reply.frame->settings.controls;
        if(!output::postIsNeutral(controls))reply.post_pixels=output::postPixels(reply.frame,controls);
    }catch(const std::exception& e){reply.error=text(e.what());}}
    return reply;
}
QImage PreviewController::geometryImage(const output::Geometry& geometry)const{
    if(post_pixels_.isNull())return visibleImage(frame_,full_image_,geometry);
    const auto effective=std::get<bool>(frame_->settings.controls.at("overscan_active"))?output::Geometry{}:geometry;
    return output::exportImage(post_pixels_,effective,{}).convertToFormat(QImage::Format_ARGB32);
}
PreviewController::PreviewController(std::filesystem::path resources,QObject* parent,ImageFactory factory,QString editDirectory)
    :QObject(parent),resources_(resources),
     edit_store_(editDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Edits":editDirectory),
     image_factory_(factory?std::move(factory):ImageFactory(imageFromFrame)) {
    auto_apply_.setSingleShot(true);auto_apply_.setInterval(180);
    connect(&auto_apply_,&QTimer::timeout,this,[this]{if(dirty())apply();});
    save_timer_.setSingleShot(true);save_timer_.setInterval(350);
    connect(&save_timer_,&QTimer::timeout,this,&PreviewController::flushEdits);
    connect(this,&PreviewController::libraryChanged,this,[this]{library_model_.setRows(libraryItems());});
    worker_=std::thread([this,resources=std::move(resources)]{workerLoop(resources);});
}
PreviewController::~PreviewController() {
    auto_apply_.stop();
    endEditGesture();flushEdits();
    if(cancel_batch_)cancel_batch_->store(true);
    ++generation_;
    {std::lock_guard lock(mutex_);stopping_=true;jobs_.clear();}
    wake_.notify_one();
    if(worker_.joinable()) worker_.join();
}
void PreviewController::workerLoop(std::filesystem::path resources) {
    std::unique_ptr<desktop::PreviewHost> host;
    Reply init;init.operation="init";
    try {host=std::make_unique<desktop::PreviewHost>(resources);init.catalog=host->catalog();}
    catch(const std::exception& e){init.error=text(e.what());}
    deliver(std::move(init));
    if(!host)return;
    for(;;) {
        Job job;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_||!jobs_.empty();});
         if(stopping_)break;job=std::move(jobs_.front());jobs_.pop_front();}
        deliver(job(*host));
    }
}
void PreviewController::deliver(Reply reply) {
    QMetaObject::invokeMethod(this,[this,reply=std::move(reply)]() mutable {accept(std::move(reply));},Qt::QueuedConnection);
}
void PreviewController::submit(Job job,QString status,QString operation) {
    auto_apply_.stop();active_operation_=std::move(operation);
    busy_=true;failed_=false;status_=std::move(status);emit stateChanged();emit historyChanged();
    {std::lock_guard lock(mutex_);jobs_.push_front(std::move(job));}
    wake_.notify_one();
}
void PreviewController::accept(Reply reply) {
    if(reply.operation=="thumbnail"){
        thumbnail_running_=false;const int i=reply.library_index;
        if(i>=0&&i<int(library_.size())&&library_[i].url==reply.thumbnail_source&&library_[i].thumbnail.isNull()&&!reply.thumbnail.isNull()){
            library_[i].thumbnail=std::move(reply.thumbnail);library_[i].thumbnail_from_camera=true;emit libraryChanged();
        }
        dispatchThumbnail();return;
    }
    if(reply.generation!=generation_)return; // a retired generation never replaces the canvas
    const bool superseded=reply.operation=="render"&&reply.settings_revision!=settings_revision_;
    busy_=false;active_operation_.clear();failed_=!superseded&&!reply.error.isEmpty();
    if(superseded) {
        // The worker may finish an earlier choice after the next click. Keep
        // both the displayed frame and the newest desired controls; never flash
        // the retired result or restore its settings over the user's last edit.
        status_=dirty()?QStringLiteral("正在更新预览…"):QStringLiteral("预览已是当前选择");
    } else if(failed_) {
        status_=reply.error;
        if(frame_&&reply.operation!="export"&&reply.operation!="batch-export")restoreSettings();
    } else if(reply.operation=="init") {
        ready_=true;catalog_=std::move(reply.catalog);films_=list(catalog_.films,resources_);papers_=list(catalog_.papers,resources_);
        restoreSettings();emit catalogChanged();status_=QStringLiteral("就绪 · 打开 RAW 开始");
    } else if(reply.frame) {
        // Allocate the presentation wrapper before publishing any new UI state.
        try {
            auto next=image_factory_(reply.frame);
            if(next.isNull())throw std::runtime_error("Cannot create the display image");
            const auto geometry=reply.operation=="open"?reply.geometry:geometry_;
            QImage visible;
            if(reply.post_pixels.isNull())visible=visibleImage(reply.frame,next,geometry);
            else {
                const auto effective=std::get<bool>(reply.frame->settings.controls.at("overscan_active"))?output::Geometry{}:geometry;
                visible=output::exportImage(reply.post_pixels,effective,{}).convertToFormat(QImage::Format_ARGB32);
                next=reply.post_pixels.convertToFormat(QImage::Format_ARGB32);
            }
            shown_settings_=reply.wanted?*reply.wanted:reply.frame->settings;
            frame_=std::move(reply.frame);post_pixels_=std::move(reply.post_pixels);full_image_=std::move(next);image_=std::move(visible);
            if(original_source_!=frame_->original){original_source_.reset();original_image_={};original_view_={};}
            geometry_=geometry;
            if(reply.operation=="open")active_index_=reply.library_index;
        }
        catch(const std::exception& e){failed_=true;status_=text(e.what());if(frame_)restoreSettings();}
        if(!failed_) {
            if(reply.operation=="solve"||reply.operation=="placement"){loadSettings(shown_settings_,frame_->decode_mode);saveActiveSettings(true);}
            else restoreSettings();
            updateThumbnail();
            status_=reply.post_only?QStringLiteral("显影后调整已更新"):QStringLiteral("渲染完成 %1 秒%2")
                .arg(frame_->timings.render_ms/1000,0,'f',2)
                .arg(frame_->reprint()?QStringLiteral(" · 已复用胶片"):QString());
            emit frameChanged();emit geometryChanged();if(reply.operation=="open")emit sourceOpened();
        }
    } else if(reply.operation=="export") status_=QStringLiteral("已导出当前画面");
    else if(reply.operation=="batch-export")status_=reply.message;
    if(failed_&&reply.operation=="open"&&reply.library_index>=0&&reply.library_index<int(library_.size()))
        library_[reply.library_index].error=status_;
    if(failed_&&reply.operation!="export"&&reply.operation!="batch-export"&&active_index_>=0){
        auto& history=library_[active_index_].undo;
        while(!history.empty()&&sameState(history.back(),currentState()))history.pop_back();
    }
    if(reply.operation=="batch-export")cancel_batch_.reset();
    emit libraryChanged();
    emit historyChanged();
    emit stateChanged();
    if(reply.operation=="init")emit initialized(!failed_);
    else emit operationFinished(reply.operation,!failed_);
    // Export holds an immutable displayed frame. An edit made just before it
    // began still needs applying, including after an overwrite refusal.
    if((superseded||reply.operation=="export"||reply.operation=="batch-export")&&!busy_)schedulePreview();
}
desktop::RenderSettings PreviewController::settings() const {
    desktop::RenderSettings s=extra_settings_;
    if(film_index_>=0&&std::size_t(film_index_)<catalog_.films.size())s.film_stock=catalog_.films[film_index_].id;
    if(paper_index_>=0&&std::size_t(paper_index_)<catalog_.papers.size())s.print_stock=catalog_.papers[paper_index_].id;
    s.print_exposure=std::exp2(-exposure_ev_);return withGeometry(std::move(s),geometry_);
}
void PreviewController::restoreSettings() {
    const auto s=frame_?shown_settings_:desktop::RenderSettings{};
    loadSettings(s,frame_?frame_->decode_mode:desktop::DecodeMode::compatible16);
    saveActiveSettings();
}
void PreviewController::loadSettings(const desktop::RenderSettings& s,desktop::DecodeMode mode) {
    extra_settings_=s;
    for(std::size_t i=0;i<catalog_.films.size();++i)if(catalog_.films[i].id==s.film_stock)film_index_=int(i);
    for(std::size_t i=0;i<catalog_.papers.size();++i)if(catalog_.papers[i].id==s.print_stock)paper_index_=int(i);
    exposure_ev_=-std::log2(s.print_exposure);
    decode_mode_=mode==desktop::DecodeMode::headroom?1:0;
    emit settingsChanged();
}
bool PreviewController::dirty() const {
    if(!frame_)return false;
    return !sameSettings(settings(),shown_settings_)||
        decode_mode_!=(frame_->decode_mode==desktop::DecodeMode::headroom?1:0);
}
bool PreviewController::filmIsPositive() const {
    return film_index_>=0&&std::size_t(film_index_)<catalog_.films.size()&&catalog_.films[film_index_].is_positive;
}
QString PreviewController::fileName() const {return frame_?QString::fromStdWString(frame_->source.filename().wstring()):QString();}
QUrl PreviewController::sourceFolder() const {return frame_?QUrl::fromLocalFile(QString::fromStdWString(frame_->source.parent_path().wstring())):QUrl();}
QVariantMap PreviewController::latitude() const {
    if(!frame_)return {};
    return QJsonDocument::fromJson(QByteArray::fromStdString(frame_->latitude_json)).object().toVariantMap();
}
void PreviewController::placeScene(double highlights,double shadows){
    if(!frame_||busy_||dialog_open_||!std::isfinite(highlights)||!std::isfinite(shadows)||highlights<0||highlights>12||shadows<0||shadows>12)return;
    const auto wanted=settings();const auto shown=frame_;const auto gen=++generation_;
    submit([wanted,shown,gen,highlights,shadows](auto& host){Reply r;r.generation=gen;r.operation="placement";
        try{if(!host.current()||host.current()->source!=shown->source||host.current()->decode_mode!=shown->decode_mode)host.open_raw(shown->source,shown->decode_mode,wanted);
            r.frame=host.render(host.place_scene(wanted,highlights,shadows));}catch(const std::exception& e){r.error=text(e.what());}return preparePresentation(std::move(r));
    },QStringLiteral("正在拟合场景置位…"),"placement");
}
QImage PreviewController::originalImage() const {
    if(!frame_||!frame_->original)return {};
    if(original_source_!=frame_->original){
        const auto& raw=*frame_->original;
        QImage linear(int(raw.width),int(raw.height),QImage::Format_RGBX32FPx4);
        if(linear.isNull())return {};
        for(unsigned y=0;y<raw.height;++y){auto* dst=reinterpret_cast<float*>(linear.scanLine(int(y)));
            const auto* src=raw.rgb.data()+size_t(y)*raw.width*3;
            for(unsigned x=0;x<raw.width;++x){for(int c=0;c<3;++c)dst[4*x+c]=src[3*x+c];dst[4*x+3]=1.0f;}}
        linear.setColorSpace(QColorSpace(QColorSpace::Primaries::ProPhotoRgb,QColorSpace::TransferFunction::Linear));
        original_image_=linear.convertedToColorSpace(QColorSpace::SRgb,QImage::Format_ARGB32);
        original_source_=frame_->original;
        original_view_={};
    }
    if(original_view_.isNull()||original_geometry_!=geometry_){
        original_view_=output::displayImage(original_image_,geometry_);original_geometry_=geometry_;
    }
    return original_view_;
}
QImage PreviewController::uncroppedImage() const {
    if(!std::get<bool>(extra_settings_.controls.at("overscan_active")))return full_image_;
    originalImage();return original_image_;
}
void PreviewController::reconcileFilmEdge(desktop::RenderSettings& next){
    if(!frame_||!frame_->original)return;
    const bool was=std::get<bool>(extra_settings_.controls.at("overscan_active")),now=std::get<bool>(next.controls.at("overscan_active"));
    const QSize size(int(frame_->original->width),int(frame_->original->height));auto g=geometry_;
    if(!was&&now)next.controls["ui_edge_held_geometry"]=packGeometry(g);
    else if(was&&!now){
        if(auto held=unpackGeometry(std::get<std::string>(extra_settings_.controls.at("ui_edge_held_geometry")),size))g=*held;
        next.controls["ui_edge_held_geometry"]=std::string("");
    }
    if(now)g=gateFramed(g,size,std::get<std::string>(next.controls.at("overscan_format")));
    if(g!=geometry_){geometry_=g;emit geometryChanged();}
}
void PreviewController::setMetering(const QString& method){
    if(!settingsEditable())return;
    auto next=extra_settings_.controls;next["auto_exposure"]=method!="custom";
    if(method!="custom")next["auto_exposure_method"]=method.toStdString();
    std::string error;if(!desktop::validate_process_controls(next,error))return;
    extra_settings_.controls=std::move(next);changed("metering");
}
void PreviewController::resetPost(){
    if(!settingsEditable())return;
    const auto defaults=desktop::default_process_controls();
    for(auto& [key,value]:extra_settings_.controls)if(key.starts_with("post_"))value=defaults.at(key);
    changed("post_reset");
}
void PreviewController::resetSection(const QString& section){
    if(!settingsEditable())return;
    auto next=settings();const auto defaults=desktop::RenderSettings{};
    const auto group=section.toStdString();
    for(auto& [key,value]:next.controls){
        bool reset=false;
        if(group=="edge")reset=key.starts_with("overscan_");
        else if(group=="date")reset=key.starts_with("date_imprint_");
        else if(group=="camera")reset=key.starts_with("raw_")||key.starts_with("auto_exposure")||key=="post_vignette";
        else if(group=="placement")reset=key.starts_with("scene_latitude_")||key=="ui_highlight_pull_back"||key=="ui_shadow_pull_back";
        else if(group=="post_wb")reset=key=="post_temperature"||key=="post_tint";
        else if(group=="post_exposure")reset=key=="post_exposure"||key=="post_contrast"||key=="post_brightness"||key=="post_saturation"||key=="post_highlights"||key=="post_shadows"||key=="post_black_point"||key=="post_white_point";
        else if(group=="post_curves")reset=key=="post_curves";
        else if(group=="post_balance")reset=key.ends_with("_hue")||key.ends_with("_luminance")||(key.starts_with("post_")&&key!="post_saturation"&&key.ends_with("_saturation"));
        if(reset)value=defaults.controls.at(key);
    }
    if(group=="camera")next.film_exposure_ev=0;
    if(group=="enlarger"){next.print_exposure=1;next.y_filter_shift=0;next.m_filter_shift=0;next.controls["preflash_exposure"]=0.0;}
    if(group=="format"){next.film_format_mm=defaults.film_format_mm;next.grain_active=true;next.grain_amount=1;next.halation_active=true;next.halation_amount=1;next.glare_active=true;next.glare_amount=1;}
    if(group=="edge")reconcileFilmEdge(next);
    loadSettings(next,decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16);changed(section.startsWith("post_")?"post_reset":"section_reset");
}
void PreviewController::copySettings(){if(!frame_||busy_)return;clipboard_=currentState();emit clipboardChanged();}
void PreviewController::pasteSettings(const QVariantList& groups){
    if(!clipboard_||!frame_||!settingsEditable())return;
    const auto& source=clipboard_->settings;auto next=settings();
    auto has=[&](const char* group){return groups.contains(QString::fromLatin1(group));};
    if(has("film")){next.film_stock=source.film_stock;next.print_stock=source.print_stock;next.film_format_mm=source.film_format_mm;}
    if(has("exposure")){next.film_exposure_ev=source.film_exposure_ev;next.print_exposure=source.print_exposure;}
    if(has("whiteBalance")){next.y_filter_shift=source.y_filter_shift;next.m_filter_shift=source.m_filter_shift;}
    if(has("filmEffects")){next.grain_active=source.grain_active;next.grain_amount=source.grain_amount;next.halation_active=source.halation_active;next.halation_amount=source.halation_amount;}
    if(has("printEffects")){next.glare_active=source.glare_active;next.glare_amount=source.glare_amount;}
    for(const auto& [key,value]:source.controls){
        const bool film=key.starts_with("overscan_")||key.starts_with("date_imprint_")||key=="digital_intermediate";
        const bool exposure=key.starts_with("auto_exposure")||(key.starts_with("post_")&&key!="post_temperature"&&key!="post_tint");
        const bool wb=key.starts_with("raw_")||key=="post_temperature"||key=="post_tint";
        const bool effects=key.starts_with("grain_")||key.starts_with("halation_")||key.starts_with("dir_couplers_")||key=="antihalation_removed"||key=="density_curve_gamma";
        const bool paper=key.starts_with("scanner_")||key=="preflash_exposure"||key=="extended_dynamic_range"||key=="ui_print_effects";
        if((film&&has("film"))||(key=="ui_scan_film"&&has("film"))||(exposure&&has("exposure"))||(wb&&has("whiteBalance"))||(effects&&has("filmEffects"))||(paper&&has("printEffects"))||((key.starts_with("scene_latitude_")||key=="ui_highlight_pull_back"||key=="ui_shadow_pull_back")&&has("scenePlacement")))next.controls[key]=value;
    }
    if(has("film"))reconcileFilmEdge(next);
    loadSettings(desktop::compatible_paper(next,catalog_),clipboard_->mode);changed("paste");
}
void PreviewController::solve(){
    if(!frame_||busy_||dialog_open_||filmIsPositive()||std::get<bool>(extra_settings_.controls.at("ui_scan_film")))return;
    const auto wanted=settings();const auto shown=frame_;const auto gen=++generation_;
    submit([wanted,shown,gen](auto& host){Reply r;r.generation=gen;r.operation="solve";
        try{if(!host.current()||host.current()->source!=shown->source||host.current()->decode_mode!=shown->decode_mode)host.open_raw(shown->source,shown->decode_mode,wanted);
            r.frame=host.render(host.solve_settings(wanted));}catch(const std::exception& e){r.error=text(e.what());}return preparePresentation(std::move(r));
    },QStringLiteral("正在测光并求解放大机配光…"),"solve");
}
QVariantList PreviewController::histogram() const {
    if(image_.isNull())return {};
    std::array<std::array<unsigned,128>,3> bins{};
    const int step=std::max(1,int(std::ceil(std::sqrt(double(image_.width())*image_.height()/65536.0))));
    unsigned peak=0;
    for(int y=0;y<image_.height();y+=step)for(int x=0;x<image_.width();x+=step){
        const auto pixel=image_.pixel(x,y);
        peak=std::max(peak,++bins[0][qRed(pixel)/2]);
        peak=std::max(peak,++bins[1][qGreen(pixel)/2]);
        peak=std::max(peak,++bins[2][qBlue(pixel)/2]);
    }
    QVariantList channels;
    const double scale=std::log1p(double(peak));
    for(const auto& channel:bins){QVariantList values;for(auto count:channel)values.append(scale?std::log1p(double(count))/scale:0.0);channels.append(QVariant(values));}
    return channels;
}
QVariantMap PreviewController::metadata() const {
    if(!frame_)return {};
    return {{"width",image_.width()},{"height",image_.height()},
        {"sourceWidth",frame_->original?frame_->original->width:frame_->width()},{"sourceHeight",frame_->original?frame_->original->height:frame_->height()},
        {"captureDate",frame_->capture_timestamp>0?QDateTime::fromSecsSinceEpoch(frame_->capture_timestamp).toString("yy.MM.dd"):QString{}},
        {"camera",text(frame_->metadata.make+" "+frame_->metadata.model)},
        {"decodeMs",frame_->timings.decode_ms},{"renderMs",frame_->timings.render_ms},
        {"totalMs",frame_->timings.total_ms},{"cached",frame_->reprint()},{"scanFilm",frame_->scan_film},
        {"mode",frame_->decode_mode==desktop::DecodeMode::headroom?"headroom":"compatible16"}};
}
void PreviewController::schedulePreview(){if(!dialog_open_&&gesture_key_!="straightenDegrees"&&frame_&&dirty()&&!auto_apply_.isActive())auto_apply_.start();}
void PreviewController::setDialogOpen(bool open){
    if(dialog_open_==open)return;
    dialog_open_=open;
    if(open){endEditGesture();auto_apply_.stop();}else schedulePreview();
    emit stateChanged();emit historyChanged();
}
void PreviewController::changed(const QString& editKey){
    if(gesture_index_>=0&&gesture_key_!=editKey&&!editKey.startsWith(gesture_key_+"_"))endEditGesture();
    saveActiveSettings(true);
    ++settings_revision_;auto_apply_.stop();auto_apply_.setInterval(editKey.startsWith("post_")?24:180);schedulePreview();
    if(frame_)status_=QStringLiteral("正在更新预览…");
    failed_=false;emit settingsChanged();emit stateChanged();
}
void PreviewController::setFilmIndex(int v){
    if(!settingsEditable()||v<0||v>=films_.size()||v==film_index_)return;
    film_index_=v;
    extra_settings_.controls["ui_scan_film"]=false;
    const auto next=desktop::compatible_paper(settings(),catalog_);
    for(int i=0;i<int(catalog_.papers.size());++i)if(catalog_.papers[i].id==next.print_stock)paper_index_=i;
    changed("filmIndex");
}
bool PreviewController::paperAvailable(int v) const {
    return !filmIsPositive() && v>=0 && v<int(catalog_.papers.size()) && film_index_>=0 && film_index_<int(catalog_.films.size()) &&
        catalog_.papers[v].monochrome==catalog_.films[film_index_].monochrome;
}
void PreviewController::setPaperIndex(int v){if(!settingsEditable()||!paperAvailable(v)||(v==paper_index_&&!std::get<bool>(extra_settings_.controls.at("ui_scan_film"))))return;paper_index_=v;extra_settings_.controls["ui_scan_film"]=false;changed("paperIndex");}
void PreviewController::setDecodeMode(int v){if(!settingsEditable()||v<0||v>1||v==decode_mode_)return;decode_mode_=v;if(v==1)extra_settings_.controls["raw_as_shot"]=true;changed("decodeMode");}
void PreviewController::setExposureEv(double v){if(!settingsEditable()||filmIsPositive()||!std::isfinite(v)||v< -2||v>2||v==exposure_ev_)return;exposure_ev_=v;changed("exposureEv");}
void PreviewController::resetSettings(){if(!settingsEditable())return;desktop::RenderSettings next;reconcileFilmEdge(next);loadSettings(next,desktop::DecodeMode::compatible16);changed();}
void PreviewController::openRaw(const QUrl& url) {
    // Changing sources and exporting remain single flight. Parameter edits
    // can be coalesced during a render, but cannot retarget an in-flight open.
    if(!ready_||busy_||!url.isLocalFile())return;
    saveActiveSettings();
    const int index=addFile(url);if(index<0)return;
    // A known photo restores its own saved settings. A genuinely new photo
    // still starts with the current preset; opening never overwrites a sidecar.
    openItem(index);
}
void PreviewController::openItem(int index) {
    endEditGesture();
    auto item=library_.at(index);const auto p=path(item.url);
    const auto gen=++generation_;
    submit([p,item,gen,index](auto& host){Reply r;r.generation=gen;r.operation="open";r.library_index=index;r.geometry=item.geometry;
        try{r.geometry=gateForSource(item.settings,item.geometry,p);r.frame=host.open_raw(p,item.mode,withGeometry(item.settings,r.geometry));}catch(const std::exception& e){r.error=text(e.what());}return preparePresentation(std::move(r));},QStringLiteral("正在解码并渲染 RAW…"),"open");
}
void PreviewController::apply() {
    if(!frame_||busy_||dialog_open_)return;
    const auto s=settings();const auto shown=frame_;const auto gen=++generation_;const auto revision=settings_revision_;
    const auto mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    const bool postOnly=sameLayerOne(s,shown_settings_)&&!sameSettings(s,shown_settings_)&&mode==shown->decode_mode;
    submit([s,shown,gen,revision,mode,postOnly](auto& host){Reply r;r.generation=gen;r.settings_revision=revision;r.operation="render";r.wanted=s;r.post_only=postOnly;
        try {
            // Publication can fail after a successful worker open (for example,
            // a display allocation failure). Reconcile against the image the
            // user is actually editing before reusing the worker's session.
            if(postOnly)r.frame=shown;
            else {const auto active=host.current();
                r.frame=active&&active->source==shown->source&&active->decode_mode==mode
                    ?host.render(s):host.open_raw(shown->source,mode,s);}
        }catch(const std::exception& e){r.error=text(e.what());}return preparePresentation(std::move(r));},postOnly?QStringLiteral("正在更新显影后调整…"):QStringLiteral("正在更新胶片效果…"),"render");
}
void PreviewController::exportTiff(const QUrl& url) {
    exportImage(url,0,95,0);
}
void PreviewController::exportImage(const QUrl& url,int format,int quality,int maxLongEdge) {
    if(!frame_||busy_||!url.isLocalFile())return;
    const auto p=path(url);const auto held=frame_;const auto gen=++generation_;const auto geometry=geometry_;const auto resources=resources_;
    const auto controls=shown_settings_.controls;
    submit([p,held,gen,geometry,resources,format,quality,maxLongEdge,controls](auto&){Reply r;r.generation=gen;r.operation="export";
        try{output::exportFrame(held,p,geometry,exportOptions(format,quality,maxLongEdge),resources,&controls);}catch(const std::exception& e){r.error=text(e.what());}return r;},QStringLiteral("正在导出当前画面…"),"export");
}

QVariantMap PreviewController::adjustments() const {
    const auto& s=extra_settings_;
    QVariantMap result={{"filmExposureEv",s.film_exposure_ev},{"filmFormatMm",s.film_format_mm},
        {"grainActive",s.grain_active},{"grainAmount",s.grain_amount},
        {"halationActive",s.halation_active},{"halationAmount",s.halation_amount},
        {"glareActive",s.glare_active},{"glareAmount",s.glare_amount},
        {"yFilterShift",s.y_filter_shift},{"mFilterShift",s.m_filter_shift}};
    for(const auto& [key,value]:s.controls)result.insert(text(key),std::visit([](const auto& v)->QVariant{
        using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::string>)return QString::fromStdString(v);else return QVariant(v);
    },value));
    if(frame_&&std::get<bool>(s.controls.at("raw_as_shot"))){result["raw_temperature"]=frame_->as_shot_temperature;result["raw_tint"]=frame_->as_shot_tint;}
    return result;
}
void PreviewController::setAdjustment(const QString& name,const QVariant& value) {
    if(!settingsEditable())return;
    auto next=extra_settings_;
    if(const auto field=next.controls.find(name.toStdString());field!=next.controls.end()){
        if((name=="raw_temperature"||name=="raw_tint")&&std::get<bool>(next.controls.at("raw_as_shot"))){
            next.controls["raw_as_shot"]=false;
            if(frame_){next.controls["raw_temperature"]=frame_->as_shot_temperature;next.controls["raw_tint"]=frame_->as_shot_tint;}
        }
        if(std::holds_alternative<bool>(field->second))field->second=value.toBool();
        else if(std::holds_alternative<double>(field->second)){bool ok=false;const double n=value.toDouble(&ok);if(!ok||!std::isfinite(n))return;field->second=n;}
        else field->second=value.toString().toStdString();
        std::string reason;if(!desktop::validate_process_controls(next.controls,reason)){status_=text(reason);emit stateChanged();return;}
    }
    else if(name=="grainActive")next.grain_active=value.toBool();
    else if(name=="halationActive")next.halation_active=value.toBool();
    else if(name=="glareActive")next.glare_active=value.toBool();
    else {
        bool ok=false;const double v=value.toDouble(&ok);if(!ok||!std::isfinite(v))return;
        double* field=nullptr;double low=0,high=0;
        if(name=="filmExposureEv"){field=&next.film_exposure_ev;low=-8;high=8;}
        else if(name=="filmFormatMm"){field=&next.film_format_mm;low=4;high=200;}
        else if(name=="grainAmount"){field=&next.grain_amount;high=2;}
        else if(name=="halationAmount"){field=&next.halation_amount;high=4;}
        else if(name=="glareAmount"){field=&next.glare_amount;high=30;}
        else if(name=="yFilterShift"){field=&next.y_filter_shift;low=-1;high=1;}
        else if(name=="mFilterShift"){field=&next.m_filter_shift;low=-1;high=1;}
        else return;
        if(v<low||v>high)return;*field=v;
    }
    if(next==extra_settings_)return;
    if(name=="overscan_active"||name=="overscan_format")reconcileFilmEdge(next);
    extra_settings_=next;changed(name);
}
QVariantList PreviewController::libraryItems() const {
    QVariantList result;
    for(int i=0;i<int(library_.size());++i) {
        const auto& item=library_[i];
        result.push_back(QVariantMap{{"name",QFileInfo(item.url.toLocalFile()).fileName()},
            {"url",item.url},{"thumbnail",item.thumbnail},{"active",i==active_index_},
            {"selected",item.selected},{"error",item.error},{"cameraThumbnail",item.thumbnail_from_camera},
            {"aspect",item.thumbnail.isNull()?1.5:double(item.thumbnail.width())/item.thumbnail.height()}});
    }
    return result;
}
int PreviewController::selectedCount() const {
    int count=0;for(const auto& item:library_)count+=item.selected;return count;
}
void PreviewController::saveActiveSettings(bool recordHistory) {
    if(active_index_<0||active_index_>=int(library_.size()))return;
    auto& item=library_[active_index_];const auto desired=settings();
    const auto mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    // A saved pending edit has no rendered thumbnail yet. Do not let the
    // strip present an older look as if it represented the new settings.
    const bool changed=!sameSettings(item.settings,desired)||item.mode!=mode||item.geometry!=geometry_;
    if(changed){
        if(recordHistory)recordEdit(item,itemState(item));
        item.thumbnail={};item.thumbnail_requested=false;item.pending_save=true;
    }
    item.settings=desired;item.mode=mode;
    item.geometry=geometry_;
    if(changed){scheduleSave();emit libraryChanged();emit historyChanged();}
}
void PreviewController::updateThumbnail() {
    if(active_index_<0||active_index_>=int(library_.size()))return;
    auto& item=library_[active_index_];
    item.thumbnail=dirty()?QImage{}:image_.scaled(180,120,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    item.thumbnail_from_camera=false;item.thumbnail_requested=false;
    item.error.clear();emit libraryChanged();
}
void PreviewController::requestThumbnail(int index){
    if(!ready_||index<0||index>=int(library_.size()))return;
    auto& item=library_[index];if(!item.thumbnail.isNull()||item.thumbnail_requested||!item.error.isEmpty())return;
    item.thumbnail_requested=true;thumbnail_requests_.push_back(index);dispatchThumbnail();
}
void PreviewController::dispatchThumbnail(){
    if(thumbnail_running_)return;
    while(!thumbnail_requests_.empty()){
        const int index=thumbnail_requests_.front();thumbnail_requests_.pop_front();
        if(index<0||index>=int(library_.size())||!library_[index].thumbnail.isNull()||index==active_index_)continue;
        const auto source=library_[index].url;thumbnail_running_=true;
        Job job=[index,source](auto&){Reply r;r.operation="thumbnail";r.library_index=index;r.thumbnail_source=source;
            try{r.thumbnail=cameraThumbnail(source);}catch(const std::exception&){}return r;};
        {std::lock_guard lock(mutex_);jobs_.push_back(std::move(job));}wake_.notify_one();return;
    }
}
int PreviewController::addFile(const QUrl& url) {
    if(!url.isLocalFile())return -1;
    const QFileInfo info(url.toLocalFile());
    if(!info.isFile())return -1;
    const auto local=QUrl::fromLocalFile(info.canonicalFilePath());
    for(int i=0;i<int(library_.size());++i)
        if(library_[i].url.toLocalFile().compare(local.toLocalFile(),Qt::CaseInsensitive)==0)return i;
    LibraryItem item;item.url=local;item.settings=settings();
    item.mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    const auto stored=edit_store_.load(local,catalog_);
    item.save_blocked=stored.blocked;item.save_warning=stored.warning;
    if(stored.state){item.settings=stored.state->settings;item.mode=stored.state->mode;item.geometry=stored.state->geometry;item.has_stored=true;}
    item.settings=desktop::compatible_paper(item.settings,catalog_);
    library_.push_back(std::move(item));emit libraryChanged();return int(library_.size())-1;
}
void PreviewController::openFiles(const QVariantList& urls) {
    if(!ready_||busy_)return;
    saveActiveSettings();int first=-1;
    for(const auto& url:urls){const int index=addFile(url.toUrl());if(first<0&&index>=0)first=index;}
    if(first>=0)openItem(first);
}
void PreviewController::openFolder(const QUrl& folder) {
    if(!ready_||busy_||dialog_open_||!folder.isLocalFile())return;
    const QDir directory(folder.toLocalFile());
    const auto files=directory.entryInfoList({"*.arw","*.nef","*.nrw","*.dng","*.cr2","*.cr3","*.raf","*.rw2","*.orf","*.pef","*.srw"},QDir::Files,QDir::Name|QDir::IgnoreCase);
    QVariantList urls;for(const auto& file:files)urls.append(QUrl::fromLocalFile(file.absoluteFilePath()));
    if(urls.isEmpty()){status_=QStringLiteral("此文件夹没有支持的 RAW 照片");emit stateChanged();return;}
    openFiles(urls);
}
void PreviewController::selectImage(int index) {
    if(!ready_||busy_||dialog_open_||index<0||index>=int(library_.size())||index==active_index_)return;
    saveActiveSettings();openItem(index);
}
void PreviewController::setItemSelected(int index,bool selected) {
    if(busy_||dialog_open_||index<0||index>=int(library_.size()))return;
    library_[index].selected=selected;emit libraryChanged();
}
void PreviewController::selectAllPhotos(bool selected) {
    if(busy_||dialog_open_)return;
    for(auto& item:library_)item.selected=selected;emit libraryChanged();
}
void PreviewController::syncSelectedSettings(bool includeCrop) {
    if(!frame_||busy_||dialog_open_)return;
    saveActiveSettings();int count=0;
    for(int i=0;i<int(library_.size());++i)if(i!=active_index_&&library_[i].selected){
        auto& item=library_[i];auto next=currentState();
        if(!includeCrop){
            next.geometry=item.geometry;
            const bool was=std::get<bool>(item.settings.controls.at("overscan_active")),now=std::get<bool>(next.settings.controls.at("overscan_active"));
            next.settings.controls["ui_edge_held_geometry"]=item.settings.controls.at("ui_edge_held_geometry");
            if(!was&&now)next.settings.controls["ui_edge_held_geometry"]=packGeometry(item.geometry);
            if(was&&!now){
                if(auto held=unpackGeometry(std::get<std::string>(item.settings.controls.at("ui_edge_held_geometry")),QSize(10000,10000)))next.geometry=*held;
                next.settings.controls["ui_edge_held_geometry"]=std::string("");
            }
        }
        if(sameState(itemState(item),next))continue;
        // Each recipient owns its own undo history, just like its settings.
        item.undo.push_back(itemState(item));if(item.undo.size()>64)item.undo.erase(item.undo.begin());item.redo.clear();
        item.settings=next.settings;item.mode=next.mode;item.geometry=next.geometry;item.pending_save=true;
        item.thumbnail={};item.thumbnail_requested=false;item.error.clear();++count;
    }
    status_=QStringLiteral("已将当前参数同步至 %1 张照片%2").arg(count)
        .arg(includeCrop?QStringLiteral("（含裁切）"):QStringLiteral("（各自保留裁切）"));
    scheduleSave();emit libraryChanged();emit historyChanged();emit stateChanged();
}
void PreviewController::setCropRect(const QRectF& crop) {
    auto next=geometry_;next.crop=crop;setGeometry(next,"cropRect");
}
void PreviewController::setGeometry(const output::Geometry& geometry,const QString& editKey) {
    if(!frame_||busy_||dialog_open_||geometry==geometry_)return;
    try {
        if(std::get<bool>(extra_settings_.controls.at("overscan_active"))){
            const QSize size(int(frame_->original->width),int(frame_->original->height));output::validateGeometry(size,geometry);
            geometry_=gateFramed(geometry,size,std::get<std::string>(extra_settings_.controls.at("overscan_format")));changed(editKey);emit geometryChanged();return;
        }
        auto next=geometryImage(geometry);
        if(gesture_index_>=0&&gesture_key_!=editKey)endEditGesture();
        image_=std::move(next);geometry_=geometry;saveActiveSettings(true);updateThumbnail();
        failed_=false;emit geometryChanged();emit frameChanged();emit stateChanged();
    } catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::resetCrop(){setCropRect(QRectF(0,0,1,1));}
void PreviewController::setCropAspect(double ratio) {
    if(!frame_||!std::isfinite(ratio)||ratio<0)return;
    if(ratio==0){resetCrop();return;}
    // Ratios describe the displayed result, even after a quarter turn.
    if(geometry_.quarterTurns%2)ratio=1/ratio;
    const double original=frame_->original?double(frame_->original->width)/frame_->original->height:double(frame_->width())/frame_->height();
    double w=1,h=1;
    if(original>ratio)w=ratio/original;else h=original/ratio;
    setCropRect(QRectF((1-w)/2,(1-h)/2,w,h));
}
void PreviewController::cancelExport(){if(cancel_batch_)cancel_batch_->store(true);}

editing::State PreviewController::itemState(const LibraryItem& item) {
    return {item.settings,item.mode,item.geometry};
}
editing::State PreviewController::currentState() const {
    return {settings(),decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16,geometry_};
}
bool PreviewController::canUndo() const {
    return settingsEditable()&&active_index_>=0&&!library_[active_index_].undo.empty();
}
bool PreviewController::canRedo() const {
    return settingsEditable()&&active_index_>=0&&!library_[active_index_].redo.empty();
}
int PreviewController::historyIndex()const{return frame_&&active_index_>=0?int(library_[active_index_].undo.size()):-1;}
QVariantList PreviewController::historyItems()const{
    if(historyIndex()<0)return {};
    const auto& item=library_[active_index_];std::vector<editing::State> states=item.undo;states.push_back(currentState());
    for(auto i=item.redo.rbegin();i!=item.redo.rend();++i)states.push_back(*i);
    QVariantList rows;const int current=historyIndex();
    for(int i=0;i<int(states.size());++i){
        const auto description=i?describeEdit(states[i-1],states[i],catalog_):HistoryDescription{"历史起点","本次运行保留的最早状态"};
        rows.append(QVariantMap{{"step",i},{"label",description.label},{"detail",description.detail},{"current",i==current},{"future",i>current}});
    }
    return rows;
}
QString PreviewController::undoLabel()const{
    if(historyIndex()<=0)return {};
    return describeEdit(library_[active_index_].undo.back(),currentState(),catalog_).label;
}
QString PreviewController::redoLabel()const{
    if(historyIndex()<0||library_[active_index_].redo.empty())return {};
    return describeEdit(currentState(),library_[active_index_].redo.back(),catalog_).label;
}
void PreviewController::recordEdit(LibraryItem& item,const editing::State& before) {
    if(gesture_index_!=active_index_||!gesture_recorded_) {
        item.undo.push_back(before);
        if(item.undo.size()>64)item.undo.erase(item.undo.begin());
        item.redo.clear();
        if(gesture_index_==active_index_)gesture_recorded_=true;
    }
}
void PreviewController::beginEditGesture(const QString& key) {
    if(!settingsEditable()||active_index_<0||key.isEmpty())return;
    endEditGesture();gesture_index_=active_index_;gesture_key_=key;gesture_recorded_=false;gesture_redo_=library_[active_index_].redo;
    if(key=="straightenDegrees")auto_apply_.stop();
}
void PreviewController::endEditGesture() {
    const bool heldGeometry=gesture_key_=="straightenDegrees";
    if(gesture_index_>=0&&gesture_index_<int(library_.size())&&gesture_recorded_) {
        auto& item=library_[gesture_index_];
        // A drag returning to its starting value is not an undo step.
        if(!item.undo.empty()&&sameState(item.undo.back(),itemState(item))){item.undo.pop_back();item.redo=std::move(gesture_redo_);}
    }
    gesture_index_=-1;gesture_key_.clear();gesture_recorded_=false;gesture_redo_.clear();emit historyChanged();
    if(heldGeometry)schedulePreview();
}
void PreviewController::undo(){restoreHistory(false);}
void PreviewController::redo(){restoreHistory(true);}
void PreviewController::restoreHistory(bool redo) {
    if(!frame_||!settingsEditable()||active_index_<0)return;
    endEditGesture();
    jumpHistory(historyIndex()+(redo?1:-1));
}
void PreviewController::jumpHistory(int index){
    if(!frame_||!settingsEditable()||active_index_<0)return;
    endEditGesture();auto& item=library_[active_index_];const int current=historyIndex();
    const int count=int(item.undo.size()+item.redo.size())+1;
    if(index<0||index>=count||index==current)return;
    try {
        std::vector<editing::State> states=item.undo;states.push_back(currentState());
        for(auto i=item.redo.rbegin();i!=item.redo.rend();++i)states.push_back(*i);
        const auto target=states[index];
        // Prepare geometry before moving the history cursor. Allocation or
        // validation failure leaves the frame and both stacks intact.
        const bool geometryChangedNow=target.geometry!=geometry_;
        auto next=geometryChangedNow?geometryImage(target.geometry):image_;
        std::vector<editing::State> undo(states.begin(),states.begin()+index),redo;
        for(int i=count-1;i>index;--i)redo.push_back(states[i]);
        item.undo=std::move(undo);item.redo=std::move(redo);
        geometry_=target.geometry;image_=std::move(next);
        loadSettings(target.settings,target.mode);saveActiveSettings();
        ++settings_revision_;auto_apply_.stop();schedulePreview();updateThumbnail();
        failed_=false;status_=index>current?QStringLiteral("已重做当前照片调整"):QStringLiteral("已回到选中的历史状态");
        if(geometryChangedNow){emit geometryChanged();emit frameChanged();}
        emit settingsChanged();emit stateChanged();emit historyChanged();
    }catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::scheduleSave() {
    save_timer_.start();emit stateChanged();
}
void PreviewController::flushEdits() {
    save_timer_.stop();
    for(auto& item:library_)if(item.pending_save&&!item.save_blocked) {
        try {edit_store_.save(item.url,itemState(item));item.pending_save=false;item.has_stored=true;item.save_warning.clear();}
        catch(const std::exception& e){item.save_warning=text(e.what());}
    }
    emit stateChanged();
}
QString PreviewController::persistenceWarning() const {
    QStringList warnings;
    for(const auto& item:library_)if(!item.save_warning.isEmpty())
        warnings.append(QFileInfo(item.url.toLocalFile()).fileName()+": "+item.save_warning);
    return warnings.join("\n");
}
QString PreviewController::savedStatus() const {
    if(!persistenceWarning().isEmpty())return QStringLiteral("部分设置未保存，请查看下方提示");
    for(const auto& item:library_)if(item.pending_save)return QStringLiteral("正在自动保存设置…");
    if(active_index_>=0&&library_[active_index_].has_stored)return QStringLiteral("当前照片设置已自动保存");
    return QStringLiteral("调整将自动保存，重新打开照片时恢复");
}
// Flips are stored in output axes. Rotating the displayed image also swaps
// those axes; without this a mirrored photo would turn the opposite way.
void PreviewController::rotateClockwise(){auto next=geometry_;next.quarterTurns=(next.quarterTurns+1)%4;std::swap(next.flipHorizontal,next.flipVertical);setGeometry(next,"quarterTurns");}
void PreviewController::rotateCounterClockwise(){auto next=geometry_;next.quarterTurns=(next.quarterTurns+3)%4;std::swap(next.flipHorizontal,next.flipVertical);setGeometry(next,"quarterTurns");}
void PreviewController::setFlipHorizontal(bool value){auto next=geometry_;next.flipHorizontal=value;setGeometry(next,"flipHorizontal");}
void PreviewController::setFlipVertical(bool value){auto next=geometry_;next.flipVertical=value;setGeometry(next,"flipVertical");}
void PreviewController::setStraightenDegrees(double value){auto next=geometry_;next.straightenDegrees=value;setGeometry(next,"straightenDegrees");}
void PreviewController::resetGeometry(){setGeometry(output::Geometry{});}

void PreviewController::exportBatch(const QUrl& folder,int format,int quality,int maxLongEdge) {
    if(!ready_||busy_||!folder.isLocalFile()||!selectedCount())return;
    saveActiveSettings();
    std::vector<LibraryItem> items;
    for(const auto& item:library_)if(item.selected)items.push_back(item);
    // Strip thumbnails from the job snapshot. Only one developed frame is
    // retained by the worker, regardless of the number of imported photos.
    for(auto& item:items)item.thumbnail={};
    const auto held=frame_;const auto resources=resources_;const auto destination=path(folder);
    const auto gen=++generation_;cancel_batch_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=cancel_batch_;batch_progress_=0;batch_total_=int(items.size());
    submit([this,items=std::move(items),held,resources,destination,gen,cancel,format,quality,maxLongEdge](auto& host){
        Reply r;r.generation=gen;r.operation="batch-export";
        int done=0,skipped=0,errors=0,processed=0;QStringList details;
        try {
            const auto options=exportOptions(format,quality,maxLongEdge);
            if(!std::filesystem::is_directory(destination))throw std::runtime_error("Export destination is not a folder");
            const QString extension=format==0?".tif":format==3?".jpg":".png";
            QSet<QString> names;
            for(const auto& item:items) {
                if(cancel->load())break;
                const auto source=path(item.url);
                const QString stem=QFileInfo(item.url.toLocalFile()).completeBaseName();
                QString name=stem+extension;int suffix=2;
                while(names.contains(name.toCaseFolded()))name=stem+"-"+QString::number(suffix++)+extension;
                names.insert(name.toCaseFolded());
                const auto target=destination/std::filesystem::path(name.toStdWString());
                try {
                    if(std::filesystem::exists(target))++skipped;
                    else {
                        const auto framing=gateForSource(item.settings,item.geometry,source);
                        const auto wanted=withGeometry(item.settings,framing);
                        const bool useShown=held&&held->source==source&&held->decode_mode==item.mode&&sameSettings(held->settings,wanted);
                        const auto developed=useShown?held:host.open_raw(source,item.mode,wanted);
                        output::exportFrame(developed,target,item.geometry,options,resources,&wanted.controls);++done;
                    }
                }catch(const std::exception& e){++errors;details.append(name+": "+text(e.what()));}
                ++processed;
                QMetaObject::invokeMethod(this,[this,gen,processed]{if(generation_==gen){batch_progress_=processed;emit stateChanged();}},Qt::QueuedConnection);
            }
            r.message=QStringLiteral("%1：导出 %2 张，跳过已有 %3 张，失败 %4 张")
                .arg(cancel->load()?QStringLiteral("批量导出已取消"):QStringLiteral("批量导出完成")).arg(done).arg(skipped).arg(errors);
            if(errors)r.error=r.message+"\n"+details.join("\n");
        }catch(const std::exception& e){r.error=text(e.what());}
        return r;
    },QStringLiteral("正在批量导出…"),"batch-export");
}
