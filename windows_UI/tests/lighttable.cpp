#include "Lighttable.hpp"
#include <QGuiApplication>
#include <QClipboard>
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QtTest/QTest>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void settle(Lighttable& table){QElapsedTimer timer;timer.start();while(table.busy()&&timer.elapsed()<30000)QTest::qWait(10);check(!table.busy(),"Worker timed out");}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
QByteArray bytes(const QString& path){QFile f(path);check(f.open(QIODevice::ReadOnly),"Cannot read fixture");return f.readAll();}
}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    try{
        QTemporaryDir fixture;check(fixture.isValid(),"Cannot create private fixture directory");
        const auto dir=fixture.path();const auto source=dir+"/original.png";
        QImage image(600,400,QImage::Format_RGB32);image.fill(QColor("#316ab7"));image.setColorSpace(QColorSpace::SRgb);
        check(image.save(source),"Cannot save input");const auto original=bytes(source);
        QString first,second;
        {
            Lighttable table(dir+"/board");
            check(table.formats().size()==10,"Missing film formats");
            table.addFiles({QUrl::fromLocalFile(source)},"135");settle(table);
            check(table.count()==1,"File import failed");first=table.selected();
            auto p=table.photo(first);check(near(p["width"].toDouble(),36)&&near(p["height"].toDouble(),24),"135 millimetre scale is wrong");
            table.addImage(image,"square gate","120_6x6");settle(table);second=table.selected();
            check(near(table.photo(second)["width"].toDouble(),56),"120 relative scale is wrong");
            table.move(first,60,40);table.rotate(first,450);table.resize(first,2);
            p=table.photo(first);check(near(p["x"].toDouble(),60)&&near(p["y"].toDouble(),40)&&near(p["angle"].toDouble(),90)&&near(p["width"].toDouble(),72),"Pose update failed");
            table.move(first,std::numeric_limits<double>::infinity(),0);
            check(near(table.photo(first)["x"].toDouble(),60),"Non-finite position accepted");
            table.resize(first,10);check(near(table.photo(first)["scale"].toDouble(),4),"Scale guard failed");table.resize(first,1);
            table.select(first);check(table.selected()==first,"Selection failed");
            table.arrange();check(near(table.photo(first)["angle"].toDouble(),0)&&near(table.photo(first)["scale"].toDouble(),1),"Arrange did not restore physical size");
            table.saveBoard(QUrl::fromLocalFile(dir+"/composite.png"));settle(table);
            QImage composite(dir+"/composite.png");check(!composite.isNull(),"PNG export failed");
            check(composite.width()<=4096&&composite.height()<=3054&&composite.colorSpace()==QColorSpace::SRgb,"Export size or color space is wrong");
            int blue=0;for(int y=0;y<composite.height();y+=8)for(int x=0;x<composite.width();x+=8)if(composite.pixelColor(x,y)==QColor("#316ab7"))++blue;
            check(blue>500,"Export contains no actual photo pixels");
            const auto saved=bytes(dir+"/composite.png");table.rotate(first,25);table.saveBoard(QUrl::fromLocalFile(dir+"/composite.png"));settle(table);
            check(bytes(dir+"/composite.png")==saved&&table.message().contains("已存在"),"Existing export was overwritten");
            table.copyBoard();settle(table);check(!QGuiApplication::clipboard()->image().isNull(),"Clipboard export failed");
            table.flush();
        }
        {
            Lighttable reopened(dir+"/board");check(reopened.count()==2&&near(reopened.photo(first)["angle"].toDouble(),25),"Saved board did not reopen");
            reopened.remove(first);check(reopened.count()==1&&!QFile::exists(dir+"/board/assets/"+first+".png"),"Removal did not clear owned cache");
            reopened.clear();check(reopened.count()==0&&bytes(source)==original,"Clear altered original photo");
        }
        {
            Lighttable pending(dir+"/pending");pending.addImage(image,"closing","135");
            // Closing during an import must persist the completed snapshot.
        }
        {Lighttable reopened(dir+"/pending");check(reopened.count()==1,"Close during import lost the snapshot");}
        for(const auto& invalid:{QByteArray("{bad"),QByteArray("{\"version\":999,\"photos\":[]}"),QByteArray("{\"version\":1,\"photos\":[{}]}")}){
            const auto badDir=dir+"/bad";QDir().mkpath(badDir);QFile file(badDir+"/board.json");check(file.open(QIODevice::WriteOnly),"Cannot create invalid fixture");file.write(invalid);file.close();
            {Lighttable bad(badDir);bad.addImage(image,"refused","135");bad.flush();check(bad.count()==0&&!bad.busy(),"Invalid store accepted a mutation");}
            check(bytes(badDir+"/board.json")==invalid,"Invalid or future store was overwritten");
        }
        std::cout<<"Lighttable passed "<<checks<<" checks (physical size, real pixels, persistence, worker shutdown, originals preserved)\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
