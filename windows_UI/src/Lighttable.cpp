#include "Lighttable.hpp"
#include <QtConcurrent/QtConcurrentRun>
#include <QClipboard>
#include <QColorSpace>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTransform>
#include <QUuid>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace {
struct Format{const char* id;const char* label;double w,h,film,along;};
const Format formatTable[]={{"135","135 · 36 × 24 mm",36,24,35,36.25},{"135_half","半格 · 18 × 24 mm",18,24,35,18},
    {"120_645","645 · 56 × 41.5 mm",41.5,56,61,41.5},{"120_6x6","6×6 · 56 × 56 mm",56,56,61,56},
    {"120_6x7","6×7 · 69.5 × 56 mm",69.5,56,61,69.5},{"120_6x8","6×8 · 76 × 56 mm",76,56,61,76},
    {"120_6x9","6×9 · 84 × 56 mm",84,56,61,84},{"135_xpan","XPan · 65 × 24 mm",65,24,35,65},
    {"120_6x12","6×12 · 112 × 56 mm",112,56,61,112},{"120_6x17","6×17 · 168 × 56 mm",168,56,61,168}};
const Format& formatFor(const QString& id){for(const auto& f:formatTable)if(id==f.id)return f;throw std::runtime_error("不支持的胶片画幅");}
bool validId(const QString& id){return !id.isEmpty()&&QUuid(id).toString(QUuid::WithoutBraces)==id;}
}
Lighttable::Lighttable(QString directory,QObject* parent):QAbstractListModel(parent),directory_(QDir(directory).absolutePath()){
    QDir().mkpath(directory_+"/assets");saveTimer_.setSingleShot(true);saveTimer_.setInterval(250);
    connect(&saveTimer_,&QTimer::timeout,this,&Lighttable::flush);
    connect(&watcher_,&QFutureWatcher<Result>::finished,this,&Lighttable::finishWork);load();
}
Lighttable::~Lighttable(){watcher_.waitForFinished();finishWork();flush();}
void Lighttable::finishWork(){
    if(!busy_)return;
    const auto r=watcher_.result();busy_=false;
    if(!r.photos.isEmpty()){
        beginInsertRows({},count(),count()+int(r.photos.size())-1);
        for(auto p:r.photos){p.x=(count()%5)*75;p.y=(count()/5)*90;p.z=count();photos_.append(p);selected_=p.id;}
        endInsertRows();emit countChanged();emit selectionChanged();saveTimer_.start();
    }
    if(!r.clipboard.isNull())QGuiApplication::clipboard()->setImage(r.clipboard);
    message_=r.message;emit stateChanged();
}
int Lighttable::rowCount(const QModelIndex& p)const{return p.isValid()?0:count();}
QHash<int,QByteArray> Lighttable::roleNames()const{return {{Id,"cardId"},{Caption,"caption"},{Source,"sourceUrl"},{W,"dimW"},{H,"dimH"},{X,"posX"},{Y,"posY"},{Angle,"angle"},{Scale,"cardScale"},{Z,"zOrder"}};}
QVariant Lighttable::data(const QModelIndex& i,int role)const{
    if(!i.isValid()||i.row()<0||i.row()>=count())return {};const auto& p=photos_[i.row()];
    switch(role){case Id:return p.id;case Caption:return p.name;case Source:return QUrl::fromLocalFile(directory_+"/assets/"+p.id+".png");case W:return p.w;case H:return p.h;case X:return p.x;case Y:return p.y;case Angle:return p.angle;case Scale:return p.scale;case Z:return p.z;default:return {};}
}
QVariantList Lighttable::formats()const{QVariantList result;for(const auto& f:formatTable)result.append(QVariantMap{{"value",f.id},{"label",QString::fromUtf8(f.label)}});return result;}
int Lighttable::indexOf(QString id)const{for(int i=0;i<count();++i)if(photos_[i].id==id)return i;return -1;}
QVariantMap Lighttable::photo(QString id)const{const int i=indexOf(id);if(i<0)return {};const auto& p=photos_[i];return {{"name",p.name},{"format",p.format},{"width",p.w*p.scale},{"height",p.h*p.scale},{"angle",p.angle},{"scale",p.scale},{"x",p.x},{"y",p.y}};}
void Lighttable::select(QString id){if(!id.isEmpty()&&indexOf(id)<0)return;selected_=id;const int i=indexOf(id);if(i>=0){int top=0;for(const auto& p:photos_)top=std::max(top,p.z);photos_[i].z=top+1;changed(i);}emit selectionChanged();}
void Lighttable::changed(int i){emit dataChanged(index(i),index(i));saveTimer_.start();emit selectionChanged();}
void Lighttable::move(QString id,double x,double y){const int i=indexOf(id);if(busy_||i<0||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>10000||std::abs(y)>10000)return;photos_[i].x=x;photos_[i].y=y;changed(i);}
void Lighttable::resize(QString id,double factor){const int i=indexOf(id);if(busy_||i<0||!std::isfinite(factor))return;photos_[i].scale=std::clamp(factor,0.25,4.0);changed(i);}
void Lighttable::rotate(QString id,double angle){const int i=indexOf(id);if(busy_||i<0||!std::isfinite(angle))return;photos_[i].angle=std::remainder(angle,360.0);changed(i);}
void Lighttable::remove(QString id){const int i=indexOf(id);if(busy_||i<0)return;beginRemoveRows({},i,i);photos_.removeAt(i);endRemoveRows();QFile::remove(directory_+"/assets/"+id+".png");selected_.clear();emit countChanged();emit selectionChanged();saveTimer_.start();}
void Lighttable::clear(){if(busy_)return;const auto old=photos_;beginResetModel();photos_.clear();endResetModel();for(const auto& p:old)QFile::remove(directory_+"/assets/"+p.id+".png");selected_.clear();emit countChanged();emit selectionChanged();saveTimer_.start();}
void Lighttable::arrange(){if(busy_)return;double x=0,y=0,rowHeight=0;for(int i=0;i<count();++i){auto& p=photos_[i];p.angle=0;p.scale=1;if(x+p.w>400&&x>0){x=0;y+=rowHeight+12;rowHeight=0;}p.x=x+p.w/2;p.y=y+p.h/2;p.z=i;x+=p.w+12;rowHeight=std::max(rowHeight,p.h);}if(count())emit dataChanged(index(0),index(count()-1));saveTimer_.start();emit selectionChanged();}
QSizeF Lighttable::physicalSize(QSize pixels,const QString& id,const QString& edge){
    const auto& f=formatFor(id);const double aspect=double(pixels.width())/pixels.height();double w=f.w,h=f.h;
    if(edge=="auto"){
        const double ratio=std::max(aspect,1/aspect);
        auto distance=[ratio](double a,double b){return std::abs(std::log(ratio/std::max(a/b,b/a)));};
        double best=distance(f.w,f.h);QString mode="plain";
        const double strip=distance(f.film,f.along+(f.along<20?.9:f.film==35?1.7:3.4));
        const double filed=distance((f.film==35?24.3:56)+2.2,f.along+1.8);
        if(strip<best&&strip<.025){best=strip;mode="strip";}
        if(filed<best&&filed<.025)mode="filed";
        return physicalSize(pixels,id,mode);
    }
    if((w>h)!=(aspect>1))std::swap(w,h);
    if(edge=="strip"||edge=="filed"){
        const double across=edge=="strip"?f.film:(f.film==35?24.3:56)+2.2;
        const double along=f.along+(edge=="filed"?1.8:(f.along<20?0.9:f.film==35?1.7:3.4));
        const bool landscape=std::abs(std::log(aspect/(along/across)))<std::abs(std::log(aspect/(across/along)));
        if(landscape)return QSizeF(across*aspect,across);return QSizeF(across,across/aspect);
    }
    const double factor=std::min(w/pixels.width(),h/pixels.height());return QSizeF(pixels.width()*factor,pixels.height()*factor);
}
Lighttable::Photo Lighttable::cacheImage(QImage image,QString name,QString format,QString edge,const QString& dir){
    if(image.isNull())throw std::runtime_error("图片无法读取");Photo p;p.id=QUuid::createUuid().toString(QUuid::WithoutBraces);p.name=name.left(160);p.format=format;
    const auto size=physicalSize(image.size(),format,edge);p.w=size.width();p.h=size.height();
    if(image.colorSpace().isValid())image.convertToColorSpace(QColorSpace::SRgb);
    else image.setColorSpace(QColorSpace::SRgb);
    if(std::max(image.width(),image.height())>4096)image=image.scaled(4096,4096,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QSaveFile file(dir+"/assets/"+p.id+".png");if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||!file.commit())throw std::runtime_error("无法保存观片台缓存");return p;
}
void Lighttable::start(std::function<Result()> work){if(busy_||storageBlocked_)return;busy_=true;message_="正在处理…";emit stateChanged();watcher_.setFuture(QtConcurrent::run([work]{try{return work();}catch(const std::exception& e){Result r;r.message=QString::fromUtf8(e.what());return r;}}));}
void Lighttable::addImage(QImage image,QString name,QString format,QString edge){if(count()>=100){message_="观片台最多保留 100 张照片";emit stateChanged();return;}const auto dir=directory_;start([=]{Result r;r.photos.append(cacheImage(image,name,format,edge,dir));r.message="已加入当前显影结果";return r;});}
void Lighttable::addFiles(QVariantList urls,QString format){const auto dir=directory_;const int available=100-count();start([=]{Result r;QStringList errors;for(const auto& value:urls){if(r.photos.size()>=available)break;const auto url=value.toUrl();if(!url.isLocalFile())continue;QImageReader reader(url.toLocalFile());reader.setAutoTransform(true);const auto size=reader.size();if(size.width()<=0||size.height()<=0||qint64(size.width())*size.height()>120000000){errors.append("图片尺寸不支持");continue;}if(std::max(size.width(),size.height())>4096)reader.setScaledSize(size.scaled(4096,4096,Qt::KeepAspectRatio));try{r.photos.append(cacheImage(reader.read(),QFileInfo(url.toLocalFile()).fileName(),format,"auto",dir));}catch(const std::exception& e){errors.append(QString::fromUtf8(e.what()));}}r.message=QString("已加入 %1 张图片").arg(r.photos.size());if(!errors.isEmpty())r.message+="；"+errors.join("；");return r;});}
QRectF Lighttable::boundsOf(const QList<Photo>& photos){QRectF bounds;for(const auto& p:photos){QTransform t;t.translate(p.x,p.y);t.rotate(p.angle);const auto r=t.mapRect(QRectF(-p.w*p.scale/2,-p.h*p.scale/2,p.w*p.scale,p.h*p.scale));bounds=bounds.isNull()?r:bounds.united(r);}return bounds.isNull()?QRectF(-50,-40,100,80):bounds;}
QRectF Lighttable::bounds()const{return boundsOf(photos_);}
QImage Lighttable::compose(const QList<Photo>& photos,const QString& dir){
    if(photos.isEmpty())throw std::runtime_error("观片台还没有照片");auto bounds=boundsOf(photos).adjusted(-12,-12,12,12);const double scale=std::min(4096/bounds.width(),3000/bounds.height());
    QImage image(int(std::ceil(bounds.width()*scale)),int(std::ceil(bounds.height()*scale))+54,QImage::Format_ARGB32);if(image.isNull())throw std::runtime_error("观片台图片过大");image.fill(QColor("#edf0e8"));QPainter painter(&image);painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.scale(scale,scale);painter.translate(-bounds.topLeft());auto sorted=photos;std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.z<b.z;});for(const auto& p:sorted){painter.save();painter.translate(p.x,p.y);painter.rotate(p.angle);const QRectF target(-p.w*p.scale/2,-p.h*p.scale/2,p.w*p.scale,p.h*p.scale);painter.fillRect(target.translated(1,1),QColor(0,0,0,35));painter.drawImage(target,QImage(dir+"/assets/"+p.id+".png"));painter.restore();}painter.resetTransform();painter.setPen(QColor("#41463e"));painter.setFont(QFont("Segoe UI",11));painter.drawText(QRect(20,image.height()-44,image.width()-40,32),Qt::AlignVCenter,"SpektraLab Windows · 观片台");painter.end();image.setColorSpace(QColorSpace::SRgb);return image;
}
void Lighttable::saveBoard(QUrl destination){if(!destination.isLocalFile())return;const auto photos=photos_;const auto dir=directory_;start([=]{Result r;const auto file=destination.toLocalFile();if(QFile::exists(file))throw std::runtime_error("目标文件已存在，请换一个文件名");auto image=compose(photos,dir);QTemporaryFile temp(QFileInfo(file).absolutePath()+"/.spektralab-table-XXXXXX");if(!temp.open()||!image.save(&temp,"PNG")||!temp.flush())throw std::runtime_error("保存观片台失败");temp.close();if(!temp.rename(file))throw std::runtime_error("无法写入目标文件，已有文件未覆盖");temp.setAutoRemove(false);r.message="观片台图片已保存";return r;});}
void Lighttable::copyBoard(){const auto photos=photos_;const auto dir=directory_;start([=]{Result r;r.clipboard=compose(photos,dir);r.message="观片台图片已复制，可自行分享";return r;});}
void Lighttable::flush(){
    saveTimer_.stop();if(storageBlocked_)return;
    QJsonArray items;
    for(const auto& p:photos_)items.append(QJsonObject{{"id",p.id},{"name",p.name},{"format",p.format},{"w",p.w},{"h",p.h},{"x",p.x},{"y",p.y},{"angle",p.angle},{"scale",p.scale},{"z",p.z}});
    QSaveFile file(directory_+"/board.json");
    if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(QJsonObject{{"version",1},{"photos",items}}).toJson())<0||!file.commit()){
        message_="观片台保存失败";emit stateChanged();
    }
}
void Lighttable::load(){
    QFile file(directory_+"/board.json");if(!file.exists())return;
    auto reject=[this]{storageBlocked_=true;photos_.clear();message_="观片台记录无法读取或版本不支持，原记录已保留";};
    if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){reject();return;}
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    const auto root=doc.object();
    if(error.error!=QJsonParseError::NoError||root["version"].toInt()!=1||!root["photos"].isArray()||root["photos"].toArray().size()>100){reject();return;}
    for(const auto& value:root["photos"].toArray()){
        const auto o=value.toObject();Photo p;p.id=o["id"].toString();
        if(!validId(p.id)||indexOf(p.id)>=0||!QFile::exists(directory_+"/assets/"+p.id+".png")){reject();return;}
        p.name=o["name"].toString().left(160);p.format=o["format"].toString();
        try{formatFor(p.format);}catch(...){reject();return;}
        for(const auto* key:{"w","h","x","y","angle","scale","z"}){
            if(!o[key].isDouble()||!std::isfinite(o[key].toDouble())){reject();return;}
        }
        p.w=o["w"].toDouble();p.h=o["h"].toDouble();p.x=o["x"].toDouble();p.y=o["y"].toDouble();
        p.angle=o["angle"].toDouble();p.scale=o["scale"].toDouble();p.z=o["z"].toInt();
        if(p.w<=0||p.h<=0||p.w>1000||p.h>1000||std::abs(p.x)>10000||std::abs(p.y)>10000||p.scale<.25||p.scale>4||std::abs(p.angle)>360||p.z<0||p.z>1000000){reject();return;}
        photos_.append(p);
    }
    emit countChanged();
}
