#include "PreviewController.hpp"
#include <QGuiApplication>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool ok,const QString& message){++checks;if(!ok)throw std::runtime_error(message.toStdString());}
template<class F>void waitFor(F condition){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<60000){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(5);}check(condition(),"Timed out");}
void settle(PreviewController& c){waitFor([&]{return !c.busy()&&!c.dirty();});check(!c.failed(),c.status());}
QByteArray bytes(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Cannot read fixture");return f.readAll();}
void write(const QString& path,const QByteArray& data){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(data)==data.size(),"Cannot create private fixture");}
QByteArray hash(const QImage& src){const auto im=src.convertToFormat(QImage::Format_RGB32);return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(im.constBits()),im.sizeInBytes()),QCryptographicHash::Sha256);}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QGuiApplication app(argc,argv);
    try{
        const auto args=app.arguments();check(args.size()==3,"Usage: raster_workflow <resources> <new-output-directory>");
        const auto out=QFileInfo(args[2]).absoluteFilePath();check(!QFileInfo::exists(out)&&QDir().mkpath(out+"/inputs"),"Output must be new");
        const auto jpeg=out+"/inputs/照片 测试.JPG";
        QImage neutral(240,160,QImage::Format_RGB32);neutral.fill(QColor(128,128,128));check(neutral.save(jpeg,"JPEG",100),"Cannot create JPEG");const auto original=bytes(jpeg);
        const auto exif=QByteArray("Exif\0\0",6)+QByteArray::fromHex("49492a0008000000010012010300010000000600000000000000");
        QByteArray marker=QByteArray::fromHex("ffe1");marker.append(char((exif.size()+2)>>8));marker.append(char((exif.size()+2)&255));marker+=exif;
        const auto oriented=out+"/inputs/oriented.jpeg";write(oriented,original.left(2)+marker+original.mid(2));
        const auto png=out+"/inputs/P3.png";QImage p3(200,120,QImage::Format_RGB32);p3.fill(QColor(160,90,40));p3.setColorSpace(QColorSpace::DisplayP3);check(p3.save(png),"Cannot create ICC PNG");
        const auto profileExpected=p3.convertedToColorSpace(QColorSpace(QColorSpace::Primaries::ProPhotoRgb,QColorSpace::TransferFunction::Linear),QImage::Format_RGBA32FPx4);
        const auto resources=std::filesystem::path(args[1].toStdWString());
        {
            PreviewController c(resources,nullptr,{},out+"/edits");waitFor([&]{return c.ready();});
            c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);
            c.openFiles({QUrl::fromLocalFile(jpeg),QUrl::fromLocalFile(oriented),QUrl::fromLocalFile(png)});settle(c);
            check(c.hasFrame()&&!c.sourceIsRaw()&&c.previewImage().size()==neutral.size(),"JPEG was not developed by the editor");
            const auto& rgb=c.frame()->original->rgb;const double expected=std::pow((128./255+.055)/1.055,2.4);
            for(int channel=0;channel<3;++channel)check(std::abs(rgb[channel]-expected)<.0005,"JPEG sRGB transfer decoding is wrong");
            const auto originalView=c.originalImage();check(std::abs(originalView.pixelColor(30,30).red()-128)<=1,"Original JPEG view has the wrong gamma");
            c.setDecodeMode(1);check(c.decodeMode()==0,"RAW headroom was enabled for JPEG");
            const auto temperature=c.adjustments()["raw_temperature"];c.setAdjustment("raw_temperature",7000);check(c.adjustments()["raw_temperature"]==temperature,"Camera RAW WB changed JPEG");
            c.requestThumbnail(1);waitFor([&]{return !c.libraryItems()[1].toMap()["thumbnail"].value<QImage>().isNull();});
            check(!c.libraryItems()[1].toMap()["cameraThumbnail"].toBool(),"JPEG thumbnail incorrectly marked as camera RAW");
            const auto first=c.previewImage();c.setAdjustment("filmExposureEv",.5);settle(c);check(hash(first)!=hash(c.previewImage()),"JPEG did not pass through film processing");
            c.undo();settle(c);check(hash(c.previewImage())==hash(first),"JPEG undo failed");c.redo();settle(c);
            c.setAdjustment("post_exposure",.3);settle(c);c.setCropRect(QRectF(.1,.1,.8,.8));
            const auto shown=c.previewImage();c.exportImage(QUrl::fromLocalFile(out+"/preview.png"),1,95,0);settle(c);
            check(hash(QImage(out+"/preview.png"))==hash(shown),"JPEG preview and exported PNG differ");
            c.exportTiff(QUrl::fromLocalFile(out+"/roundtrip.tif"));settle(c);
            c.selectImage(1);settle(c);check(c.previewImage().size()==QSize(160,240),"JPEG EXIF orientation was not applied");
            c.selectImage(2);settle(c);const float* target=reinterpret_cast<const float*>(profileExpected.constScanLine(0));
            for(int channel=0;channel<3;++channel)check(std::abs(c.frame()->original->rgb[channel]-target[channel])<.003,"PNG ICC to linear ProPhoto differs from independent Qt transform");
            c.openFiles({QUrl::fromLocalFile(out+"/roundtrip.tif")});settle(c);check(!c.sourceIsRaw()&&c.originalImage().size()==shown.size(),"Native TIFF decode failed");
            const auto retained=c.frame();const auto broken=out+"/inputs/broken.jpg";write(broken,"not a jpeg");c.openFiles({QUrl::fromLocalFile(broken)});waitFor([&]{return !c.busy();});
            check(c.failed()&&c.frame()==retained,"Corrupt JPEG replaced the previous good frame");
            QByteArray icc=QColorSpace(QColorSpace::SRgb).iccProfile();icc.replace(16,4,"CMYK");
            QByteArray profile=QByteArray("ICC_PROFILE\0",12)+QByteArray::fromHex("0101")+icc;
            QByteArray app2=QByteArray::fromHex("ffe2");app2.append(char((profile.size()+2)>>8));app2.append(char((profile.size()+2)&255));app2+=profile;
            const auto badProfile=out+"/unsupported-ICC.jpg";write(badProfile,original.left(2)+app2+original.mid(2));
            c.openFiles({QUrl::fromLocalFile(badProfile)});waitFor([&]{return !c.busy();});check(c.failed()&&c.frame()==retained&&c.status().contains("ICC"),"Unsupported ICC was silently assumed sRGB");
            c.selectImage(0);settle(c);check(hash(c.previewImage())==hash(shown),"JPEG per-photo adjustments were lost");
        }
        {
            PreviewController c(resources,nullptr,{},out+"/edits");waitFor([&]{return c.ready();});c.openRaw(QUrl::fromLocalFile(jpeg));settle(c);
            check(c.cropRect()==QRectF(.1,.1,.8,.8)&&std::abs(c.adjustments()["filmExposureEv"].toDouble()-.5)<1e-9,"JPEG saved edits did not reopen");
        }
        {PreviewController c(resources,nullptr,{},out+"/folder-edits");waitFor([&]{return c.ready();});c.openFolder(QUrl::fromLocalFile(out+"/inputs"));waitFor([&]{return !c.busy();});check(c.libraryCount()==4,"Folder import missed JPEG/PNG files");}
        check(bytes(jpeg)==original,"JPEG source was modified");
        QFile report(out+"/raster-workflow.json");check(report.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot write report");report.write(QJsonDocument(QJsonObject{{"passed",true},{"checks",checks},{"jpeg_transfer_and_exif",true},{"icc_png",true},{"tiff_roundtrip",true},{"preview_export_pixels",true},{"history_and_reopen",true}}).toJson());
        std::cout<<"Raster workflow passed "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
