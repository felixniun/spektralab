// Exercise the same native controller used by the GUI against a copied RAW.
#include "PreviewController.hpp"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const QString& message){if(!ok)throw std::runtime_error(message.toStdString());}
QByteArray pixelsHash(const QImage& source){const auto image=source.convertToFormat(QImage::Format_RGBA8888);return QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),QCryptographicHash::Sha256);}
void settle(PreviewController& c){
    QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    poll.start();deadline.start(120000);loop.exec();
    check(!c.busy()&&!c.dirty()&&!c.failed(),c.status());
}
int film(PreviewController& c,const QString& id){
    const auto rows=c.films();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==id)return i;
    throw std::runtime_error("Missing film: "+id.toStdString());
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");QGuiApplication app(argc,argv);
    try {
        const auto args=app.arguments();check(args.size()==4,"Usage: color_workflow <resources> <RAW> <new-output-directory>");
        const auto out=QFileInfo(args[3]).absoluteFilePath();check(!QFileInfo::exists(out)&&QDir().mkpath(out),"Output must be new");
        const auto input=out+"/input."+QFileInfo(args[2]).suffix();check(QFile::copy(args[2],input),"Cannot copy RAW");
        PreviewController c(std::filesystem::path(args[1].toStdWString()),nullptr,{},out+"/edits");
        settle(c);check(c.ready(),"Engine unavailable");c.openRaw(QUrl::fromLocalFile(input));settle(c);
        c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);settle(c);
        const QStringList stocks={"kodak_ektachrome_100","kodak_portra_800","ilford_hp5_plus_400","kodak_tmax_100",
            "fujifilm_neopan_acros_100_ii","kodak_tri_x_400","kodak_portra_400"};
        QJsonArray results;
        for(const auto& id:stocks){
            c.setFilmIndex(film(c,id));settle(c);
            const bool bw=id.contains("ilford")||id.contains("tmax")||id.contains("acros")||id.contains("tri_x");
            check(c.frame()->scan_film==(id=="kodak_ektachrome_100"),"Stale slide scan mode: "+id);
            check((c.frame()->settings.print_stock=="ilford_multigrade_iv_rc")==bw,"Wrong paper family: "+id);
            auto image=c.previewImage();int spread=0,low=255,high=0;
            for(int y=0;y<image.height();y+=4)for(int x=0;x<image.width();x+=4){
                const auto p=image.pixel(x,y);spread=std::max(spread,std::max({qRed(p),qGreen(p),qBlue(p)})-std::min({qRed(p),qGreen(p),qBlue(p)}));
                low=std::min(low,qRed(p));high=std::max(high,qRed(p));
            }
            check(high-low>30,"Empty/flat image: "+id);check(bw?spread<=2:spread>20,"Wrong monochrome/colour output: "+id);
            check(c.histogram().size()==3,"Missing preview histogram");
            check(image.scaled(1200,1200,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(out+"/"+id+".png"),"Cannot save colour evidence");
            results.append(QJsonObject{{"film",id},{"paper",QString::fromStdString(c.frame()->settings.print_stock)},
                {"scan_film",c.frame()->scan_film},{"channel_spread",spread},{"render_ms",c.frame()->timings.render_ms}});
        }
        const auto baseFrame=c.frame();const auto baseImage=c.previewImage();const auto baseHash=pixelsHash(baseImage);
        c.setAdjustment("post_exposure",0.7);settle(c);
        check(c.frame()==baseFrame&&pixelsHash(c.previewImage())!=baseHash,"Post edit re-developed the film or did not update the image");
        c.setAdjustment("post_temperature",20.0);c.setAdjustment("post_master_saturation",0.15);settle(c);
        const auto gradeHash=pixelsHash(c.previewImage());
        c.exportImage(QUrl::fromLocalFile(out+"/post-export.png"),1,95,0);settle(c);
        check(pixelsHash(QImage(out+"/post-export.png"))==gradeHash,"Export did not capture the displayed post-development grade");
        c.resetPost();settle(c);check(c.frame()==baseFrame&&pixelsHash(c.previewImage())==baseHash,"Post reset compounded an edit or did not restore exact source pixels");
        const QRectF heldCrop(.2,.3,.5,.4);c.setCropRect(heldCrop);
        const auto gate=c.previewImage().size();
        c.setAdjustment("overscan_active",true);settle(c);
        check(c.previewImage().size()!=gate,"Film Edge switch did not reach the renderer: output still has no film border");
        check(c.uncroppedImage().size()==baseImage.size(),"Film Edge crop guide shows the border instead of the decoded photograph");
        check(c.previewImage().save(out+"/film-edge.png"),"Cannot save bordered preview");
        const auto bordered=c.previewImage().size();
        c.exportImage(QUrl::fromLocalFile(out+"/film-edge-export.png"),1,95,0);settle(c);
        check(QImage(out+"/film-edge-export.png").size()==bordered&&pixelsHash(QImage(out+"/film-edge-export.png"))==pixelsHash(c.previewImage()),"Export dropped, cropped or changed the film border");
        const auto edgeHash=pixelsHash(c.previewImage());
        c.setAdjustment("date_imprint_text",QStringLiteral("26.10.31"));c.setAdjustment("date_imprint_active",true);settle(c);
        check(pixelsHash(c.previewImage())!=edgeHash,"Date Back did not expose the date onto the film");
        c.setAdjustment("date_imprint_active",false);settle(c);
        c.setAdjustment("overscan_format",QStringLiteral("120_6x6"));settle(c);
        auto framing=c.cropRect();
        check(std::abs(framing.width()*baseImage.width()/(framing.height()*baseImage.height())-1)<1e-6,"Film format change did not hold the square gate");
        c.setCropRect(QRectF(.15,.2,.6,.5));settle(c);framing=c.cropRect();
        check(std::abs(framing.width()*baseImage.width()/(framing.height()*baseImage.height())-1)<1e-6,"Crop edit escaped the active film gate ratio");
        c.setAdjustment("overscan_active",false);settle(c);
        check(c.previewImage().size()==gate&&c.cropRect()==heldCrop,"Disabling Film Edge did not restore the user's original crop");
        c.resetGeometry();check(c.previewImage().size()==baseImage.size(),"Crop reset did not restore the picture");
        c.setAdjustment("raw_temperature",6500.0);settle(c);
        check(c.frame()->timings.decode_ms>0&&!std::get<bool>(c.frame()->settings.controls.at("raw_as_shot")),"Camera WB did not re-decode the RAW");
        c.setAdjustment("raw_as_shot",true);settle(c);
        // Folder import includes only supported RAW extensions, no subfolders.
        check(QDir().mkpath(out+"/roll"),"Cannot create roll");
        check(QFile::copy(input,out+"/roll/second.ARW"),"Cannot copy folder fixture");
        QFile ignored(out+"/roll/notes.txt");check(ignored.open(QIODevice::WriteOnly),"Cannot create ignored file");ignored.write("ignored");ignored.close();
        c.openFolder(QUrl::fromLocalFile(out+"/roll"));settle(c);check(c.libraryItems().size()==2,"Folder import included unsupported files");
        QFile report(out+"/color-workflow.json");check(report.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot create report");
        report.write(QJsonDocument(QJsonObject{{"status","passed"},{"cases",results},{"post_without_engine_render",true},{"post_export_matches_preview",true},{"cropped_film_edge_export_matches_preview",true},{"date_back_changes_pixels",true},{"camera_wb_redecodes",true}}).toJson());
        std::cout<<"Native colour/edge/date/post/WB workflow passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
