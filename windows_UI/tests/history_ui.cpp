// The real Main.qml, controller, RAW and Qt keyboard shortcut dispatcher.
// QTest lives only in this test executable, never in the shipped application.
#include "PreviewController.hpp"
#include "FrameCanvas.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest/QTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void waitFor(F condition){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<120000)QTest::qWait(10);check(condition(),"Timed out waiting for controller");}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
void menuChecks(QQuickWindow& window,PreviewController& preview,const QString& out){
    struct OpenMenu {QQuickItem* combo;QObject* popup;QObject* list;};QJsonArray cases;
    auto click=[&](QQuickItem* item){check(item&&item->isVisible(),"Menu item is not visible");QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());};
    auto itemAt=[](QObject* list,int index){QQuickItem* item=nullptr;check(QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,item),Q_ARG(int,index)),"Cannot inspect popup row");return item;};
    auto inspect=[&](const QString& name,const QStringList& labels,const QString& tag){
        auto* combo=window.findChild<QQuickItem*>("railMenu_"+name);check(combo&&combo->isEnabled(),"Combo missing or disabled");click(combo);QTest::qWait(40);
        auto* popup=combo->property("popup").value<QObject*>();check(popup&&popup->property("visible").toBool(),"Mouse click did not open the popup");
        auto* list=popup->property("contentItem").value<QObject*>();check(list&&list->property("count").toInt()==labels.size(),"Popup has the wrong option count");
        const auto capture=window.grabWindow();check(!capture.isNull()&&capture.save(out+"/"+tag+".png"),"Cannot capture popup");
        for(int i=0;i<labels.size();++i){auto* item=itemAt(list,i);
            if(!item||item->property("text").toString()!=labels[i])throw std::runtime_error((QString("Popup %1 option %2 is blank/wrong; expected %3").arg(name).arg(i).arg(labels[i])).toStdString());
            auto* caption=item->property("contentItem").value<QQuickItem*>();
            check(caption&&caption->height()>=10&&caption->width()>10,"Popup has option data but its text area is collapsed to zero height");
            const auto rect=caption->mapRectToScene(caption->boundingRect());const auto dpr=window.effectiveDevicePixelRatio();int visiblePixels=0;
            for(int y=int(rect.top()*dpr);y<int(rect.bottom()*dpr);++y)for(int x=int((rect.left()+1)*dpr);x<int((rect.right()-1)*dpr);++x)
                if(capture.rect().contains(x,y)&&qGray(capture.pixel(x,y))>110)++visiblePixels;
            check(visiblePixels>=3,"Popup option text has no visible high-contrast pixels");
        }
        cases.append(QJsonObject{{"menu",name},{"visible_options",labels.size()}});return OpenMenu{combo,popup,list};
    };
    auto choose=[&](const OpenMenu& menu,int index){auto* item=itemAt(menu.list,index);check(item&&item->isEnabled(),"Chosen option is unavailable");click(item);check(!menu.popup->property("visible").toBool(),"Choosing an option did not close the popup");};
    auto metering=inspect("测光方式",{"自定义","均衡","居中","保护高光","保护阴影","中央重点","平均","中值"},"metering-open");
    QTest::keyClick(&window,Qt::Key_Down);QTest::keyClick(&window,Qt::Key_Return);
    check(preview.adjustments()["auto_exposure"].toBool()&&preview.adjustments()["auto_exposure_method"]=="balanced","Keyboard choice did not set metering");
    inspect("RAW 解码",{"兼容模式","高光扩展"},"raw-mode-open");QTest::keyClick(&window,Qt::Key_Escape);
    auto size=inspect("filmFormatMm",{"半格 · 18 mm","半格 · 24 mm","135 · 35 mm","135 · 36 mm","120 · 56 mm","120 · 70 mm","120 · 90 mm","6×17 · 168 mm"},"film-size-open");choose(size,3);
    check(near(preview.adjustments()["filmFormatMm"].toDouble(),36),"Numeric option did not reach the controller");
    for(const auto* name:{"navigatorSection","historyPanel","clipboardSection","filmSection"}){auto* section=window.findChild<QObject*>(name);check(section&&section->setProperty("collapsed",true),"Cannot reveal Film Edge controls");}
    preview.setAdjustment("overscan_active",true);QTest::qWait(30);
    auto format=inspect("overscan_format",{"135","135 半格","135 XPan","120 · 645","120 · 6×6","120 · 6×7","120 · 6×8","120 · 6×9","120 · 6×12","120 · 6×17"},"film-edge-format-open");choose(format,4);
    check(preview.adjustments()["overscan_format"]=="120_6x6","Film Edge format selection did not reach the controller");
    auto view=inspect("overscan_mode",{"片条","片框"},"film-edge-view-open");choose(view,1);check(preview.adjustments()["overscan_mode"]=="filed","Film Edge view selection failed");
    auto gate=inspect("overscan_gate",{"自动","方角","圆角","耳形","肩形","斜角"},"film-edge-gate-open");choose(gate,1);check(preview.adjustments()["overscan_gate"]=="square","Gate selection failed");
    auto holes=inspect("overscan_holes",{"透光","遮光"},"film-edge-holes-open");choose(holes,1);check(preview.adjustments()["overscan_holes"]=="black","Hole selection failed");
    auto carrier=inspect("overscan_carrier",{"无","黑色片夹","开放片夹"},"film-edge-carrier-open");choose(carrier,1);check(preview.adjustments()["overscan_carrier"]=="black","Carrier selection failed");
    preview.setAdjustment("overscan_active",false);preview.setAdjustment("date_imprint_text","26.10.31");preview.setAdjustment("date_imprint_active",true);QTest::qWait(30);
    auto face=inspect("date_imprint_style",{"LCD","点阵","拍摄数据"},"date-face-open");
    check(!itemAt(face.list,2)->isEnabled(),"Unavailable data-back face should be disabled without an edge");click(itemAt(face.list,2));check(face.popup->property("visible").toBool()&&preview.adjustments()["date_imprint_style"]=="lcd","Disabled date option was selectable");choose(face,1);
    check(preview.adjustments()["date_imprint_style"]=="dots","Date style selection failed");
    auto placement=inspect("date_imprint_placement",{"画面内","片边"},"date-placement-open");check(!itemAt(placement.list,1)->isEnabled(),"Unavailable date placement was enabled");QTest::keyClick(&window,Qt::Key_Escape);
    auto corner=inspect("date_imprint_corner",{"右下","左下","右上","左上"},"date-corner-open");choose(corner,3);check(preview.adjustments()["date_imprint_corner"]=="tl","Date corner selection failed");
    waitFor([&]{return !preview.busy()&&!preview.dirty();});check(!preview.failed(),"Menu changes failed to render");
    QFile report(out+"/menus-ui.json");check(report.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot write menu report");
    report.write(QJsonDocument(QJsonObject{{"passed",true},{"menus",cases},{"visible_text_pixels",true},{"keyboard_and_mouse_selection",true},{"disabled_options_rejected",true}}).toJson());
}
}
int main(int argc,char** argv){
    qputenv("QT_QPA_PLATFORM","offscreen");qputenv("QT_QUICK_BACKEND","software");
    if(qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))qputenv("QT_QPA_FONTDIR",(qEnvironmentVariable("WINDIR")+"/Fonts").toUtf8());
    QGuiApplication app(argc,argv);QQuickStyle::setStyle("Basic");
    try{
        const auto args=app.arguments();check(args.size()==4||(args.size()==5&&args[4]=="menus"),"Usage: history_ui <resources> <RAW> <new-output-directory> [menus]");
        const auto out=QFileInfo(args[3]).absoluteFilePath();check(!QFileInfo::exists(out)&&QDir().mkpath(out),"Output directory must be new");
        const auto source=out+"/input."+QFileInfo(args[2]).suffix();check(QFile::copy(args[2],source),"Cannot copy RAW fixture");
        PreviewController preview(std::filesystem::path(args[1].toStdWString()),nullptr,{},out+"/edits");
        qmlRegisterType<FrameCanvas>("SpektraLab.Native",1,0,"FrameCanvas");
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("preview",&preview);
        engine.rootContext()->setContextProperty("workspaceSettingsUrl",QUrl::fromLocalFile(out+"/workspace.ini"));
        engine.loadFromModule("SpektraLab","Main");check(!engine.rootObjects().empty(),"Real editor QML did not load");
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());check(window,"Editor window missing");window->requestActivate();
        waitFor([&]{return preview.ready();});preview.openRaw(QUrl::fromLocalFile(source));waitFor([&]{return !preview.busy()&&preview.hasFrame();});
        if(args.size()==5){menuChecks(*window,preview,out);std::cout<<"Real editor menus passed\n";return 0;}
        auto* postTab=window->findChild<QQuickItem*>("postTab");check(postTab,"Post tab missing");
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,postTab->mapToScene(QPointF(postTab->width()/2,postTab->height()/2)).toPoint());QTest::qWait(30);
        auto* field=window->findChild<QQuickItem*>("postExposureSliderField");check(field,"Post exposure entry missing");
        field->forceActiveFocus();QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        for(const auto key:{Qt::Key_0,Qt::Key_Period,Qt::Key_7})QTest::keyClick(window,key);
        QTest::keyClick(window,Qt::Key_Return);
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7),"Enter did not commit the parameter");
        QTest::keyClick(window,Qt::Key_Z,Qt::ControlModifier);
        check(near(preview.adjustments()["post_exposure"].toDouble(),0),"Ctrl+Z after numeric commit was swallowed by text focus");
        QTest::keyClick(window,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7),"Ctrl+Shift+Z did not redo the photo adjustment");
        QTest::keyClick(window,Qt::Key_Z,Qt::ControlModifier);QTest::keyClick(window,Qt::Key_Y,Qt::ControlModifier);
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7),"Ctrl+Y did not redo the photo adjustment");
        field->forceActiveFocus();QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);
        for(const auto key:{Qt::Key_1,Qt::Key_Period,Qt::Key_1})QTest::keyClick(window,key);
        QTest::keyClick(window,Qt::Key_Z,Qt::ControlModifier);
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7)&&field->property("text").toString()!="1.1","Typing undo unexpectedly undid the photograph");
        QTest::keyClick(window,Qt::Key_A,Qt::ControlModifier);for(const auto key:{Qt::Key_0,Qt::Key_Period,Qt::Key_7})QTest::keyClick(window,key);
        QTest::keyClick(window,Qt::Key_Return);
        auto* list=window->findChild<QQuickItem*>("historyList");check(list&&list->property("count").toInt()==2,"Visible history list does not match the two edit states");
        auto click=[&](QQuickItem* item){check(item&&item->isEnabled()&&item->isVisible(),"History control is unavailable");QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());};
        auto row=[&](int index){QQuickItem* item=nullptr;check(QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,item),Q_ARG(int,index)),"Cannot find history row");return item;};
        QTest::qWait(30);click(row(0));
        check(near(preview.adjustments()["post_exposure"].toDouble(),0)&&preview.historyIndex()==0&&preview.canRedo(),"Clicking an earlier history row did not restore the photo");
        click(window->findChild<QQuickItem*>("redoToolbarButton"));
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7),"Toolbar redo did not restore the edit");
        click(window->findChild<QQuickItem*>("undoToolbarButton"));
        check(near(preview.adjustments()["post_exposure"].toDouble(),0),"Toolbar undo did not restore the earlier state");
        QTest::qWait(30);click(row(1));
        check(near(preview.adjustments()["post_exposure"].toDouble(),0.7)&&preview.historyIndex()==1,"Clicking a future history row did not redo");
        waitFor([&]{return !preview.busy()&&!preview.dirty();});
        check(!preview.failed()&&window->grabWindow().save(out+"/history-ui.png"),"Cannot capture history editor");
        QFile report(out+"/history-ui.json");check(report.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot write report");
        report.write(QJsonDocument(QJsonObject{{"passed",true},{"real_qml_keyboard_dispatch",true},{"parameter_commit_then_undo_redo",true},
            {"typing_keeps_text_undo",true},{"toolbar_undo_redo",true},{"click_history_past_and_future",true}}).toJson());
        std::cout<<"Real editor keyboard history passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
