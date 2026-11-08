#include "FrameCanvas.hpp"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <iostream>

// Render the actual scene graph: pointer/state assertions cannot catch a
// comparison texture crashing the renderer or covering the wrong half.
bool pointerChecks(QQuickWindow& window,FrameCanvas& canvas){
    QImage image(1200,800,QImage::Format_ARGB32);image.fill(Qt::blue);canvas.setImage(image);canvas.resetView(true);
    const QPointF pointer(110,60);auto before=canvas.imageRect();
    const QPointF imagePoint((pointer.x()-before.x())/before.width(),(pointer.y()-before.y())/before.height());
    QWheelEvent wheel(pointer,window.mapToGlobal(pointer.toPoint()),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(&window,&wheel);auto after=canvas.imageRect();
    if(canvas.fit()||after.width()<=before.width()||std::abs((pointer.x()-after.x())/after.width()-imagePoint.x())>1e-8||std::abs((pointer.y()-after.y())/after.height()-imagePoint.y())>1e-8){
        std::cerr<<"Mouse wheel did not zoom about the pointer\n";return false;
    }
    auto mouse=[&](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
        QMouseEvent event(type,point,point,window.mapToGlobal(point.toPoint()),button,buttons,Qt::NoModifier);QCoreApplication::sendEvent(&window,&event);
    };
    auto drag=[&](Qt::MouseButton button,QPointF delta){
        const QPointF start(180,90);const auto old=canvas.imageRect();
        mouse(QEvent::MouseButtonPress,start,button,button);mouse(QEvent::MouseMove,start+delta,Qt::NoButton,button);mouse(QEvent::MouseButtonRelease,start+delta,button,Qt::NoButton);
        const auto moved=canvas.imageRect();return std::abs(moved.x()-old.x()-delta.x())<0.01&&std::abs(moved.y()-old.y()-delta.y())<0.01;
    };
    if(!drag(Qt::LeftButton,{15,8})||!drag(Qt::MiddleButton,{-10,-6})){std::cerr<<"Left/middle mouse drag did not pan the photograph\n";return false;}
    canvas.resetView(true);
    if(!drag(Qt::LeftButton,{15,8})||canvas.fit()){std::cerr<<"Dragging from fit mode is still blocked\n";return false;}
    const double previous=canvas.scalePercent();
    QWheelEvent smooth(pointer,window.mapToGlobal(pointer.toPoint()),QPoint(0,15),{},Qt::NoButton,Qt::NoModifier,Qt::ScrollBegin,false);
    QCoreApplication::sendEvent(&window,&smooth);
    if(canvas.scalePercent()<=previous){std::cerr<<"High-resolution wheel input was ignored\n";return false;}
    return true;
}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);QQuickWindow window;
    window.resize(400,200);window.setColor(Qt::gray);
    auto* canvas=new FrameCanvas(window.contentItem());canvas->setWidth(400);canvas->setHeight(200);
    QImage developed(400,200,QImage::Format_ARGB32);developed.fill(Qt::blue);
    QImage original(400,200,QImage::Format_ARGB32);original.fill(Qt::red);
    canvas->setImage(original);canvas->setComparisonImage(original);window.show();
    QTimer::singleShot(100,&app,[&]{
      canvas->setImage(developed);canvas->setSplit(true);
      QTimer::singleShot(100,&app,[&]{
        const auto capture=window.grabWindow();
        if(capture.isNull()||capture.pixelColor(100,100)!=QColor(Qt::red)||capture.pixelColor(300,100)!=QColor(Qt::blue)){
            std::cerr<<"Split did not display original left and developed right\n";app.exit(1);return;
        }
        canvas->setSplit(false);
        QTimer::singleShot(100,&app,[&]{
            const auto capture=window.grabWindow();
            if(capture.isNull()||capture.pixelColor(100,100)!=QColor(Qt::blue)){std::cerr<<"Split overlay survived disabling\n";app.exit(1);return;}
            if(!pointerChecks(window,*canvas)){app.exit(1);return;}
            std::cout<<"Native canvas comparison and mouse interaction passed\n";app.exit(0);
        });
      });
    });
    QTimer::singleShot(5000,&app,[&]{app.exit(2);});return app.exec();
}
