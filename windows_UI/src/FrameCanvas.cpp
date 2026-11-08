#include "FrameCanvas.hpp"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTextureMaterial>
#include <algorithm>
#include <cmath>

namespace {class ImageNode final:public QSGSimpleTextureNode {public:quint64 revision=0,comparisonRevision=0;QSGSimpleTextureNode* comparison=nullptr;};}
FrameCanvas::FrameCanvas(QQuickItem* parent):QQuickItem(parent) {
    setFlag(ItemHasContents,true);setClip(true);setAcceptedMouseButtons(Qt::LeftButton|Qt::MiddleButton);
    setCursor(Qt::ArrowCursor);
}
void FrameCanvas::setImage(const QImage& image) {
    if(image.cacheKey()==image_.cacheKey())return;
    const bool changedSize=image.size()!=image_.size();image_=image;++revision_;
    if(changedSize)pan_={};clampPan();update();emit imageChanged();emit viewChanged();
    setCursor(image_.isNull()?Qt::ArrowCursor:Qt::OpenHandCursor);
}
void FrameCanvas::setFit(bool value) {if(value==fit_)return;fit_=value;pan_={};update();emit viewChanged();}
void FrameCanvas::resetView(bool value) {fit_=value;zoom_=1.0;pan_={};update();emit viewChanged();}
void FrameCanvas::setComparisonImage(const QImage& image){if(image.cacheKey()==comparison_image_.cacheKey())return;comparison_image_=image;++comparison_revision_;update();emit imageChanged();}
void FrameCanvas::setSplit(bool value){if(value==split_)return;split_=value;update();emit viewChanged();}
void FrameCanvas::zoomBy(double factor){zoomAt(QPointF(width()/2,height()/2),factor);}
void FrameCanvas::zoomAt(QPointF position,double factor){
    if(!std::isfinite(factor)||factor<=0||image_.isNull())return;
    const auto before=imageRect();if(before.isEmpty())return;
    const QPointF point((position.x()-before.x())/before.width(),(position.y()-before.y())/before.height());
    zoom_=std::clamp(scale()*factor,0.01,16.0);fit_=false;
    const QSizeF next(image_.width()*zoom_/dpr(),image_.height()*zoom_/dpr());
    pan_=QPointF(position.x()-point.x()*next.width()-(width()-next.width())/2,
                 position.y()-point.y()*next.height()-(height()-next.height())/2);
    clampPan();update();emit viewChanged();
}
double FrameCanvas::dpr() const {return window()?window()->effectiveDevicePixelRatio():1.0;}
double FrameCanvas::scale() const {
    if(image_.isNull()||width()<=0||height()<=0)return 1;
    return fit_?std::min(width()*dpr()/image_.width(),height()*dpr()/image_.height()):zoom_;
}
double FrameCanvas::scalePercent() const {return image_.isNull()?0:scale()*100;}
QRectF FrameCanvas::imageRect() const {
    if(image_.isNull())return {};
    const QSizeF size(image_.width()*scale()/dpr(),image_.height()*scale()/dpr());
    auto position=[](double extent,double canvas,double pan){
        const double centred=(canvas-extent)*0.5;
        if(extent>canvas+1e-6)return std::clamp(centred+pan,canvas-extent,0.0);
        const double visible=std::min({64.0,extent*0.25,canvas*0.25});
        return std::clamp(centred+pan,visible-extent,canvas-visible);
    };
    double x=position(size.width(),width(),fit_?0:pan_.x());
    double y=position(size.height(),height(),fit_?0:pan_.y());
    // At 100%, align the image origin to the physical pixel grid as well as
    // preserving its physical size. Fractional centring must not shift grain.
    if(std::abs(scale()-1.0)<1e-12){x=std::round(x*dpr())/dpr();y=std::round(y*dpr())/dpr();}
    return {x,y,size.width(),size.height()};
}
void FrameCanvas::clampPan() {
    const auto rect=imageRect();
    pan_.setX(rect.x()-(width()-rect.width())*0.5);
    pan_.setY(rect.y()-(height()-rect.height())*0.5);
}
void FrameCanvas::panBy(double x,double y) {
    if(image_.isNull())return;
    if(fit_){zoom_=scale();fit_=false;}
    pan_+=QPointF(x,y);clampPan();update();emit viewChanged();
}
QSGNode* FrameCanvas::updatePaintNode(QSGNode* old,UpdatePaintNodeData*) {
    auto* node=static_cast<ImageNode*>(old);
    if(image_.isNull()||!window()){delete node;return nullptr;}
    if(!node)node=new ImageNode;
    if(node->revision!=revision_) {
        auto* texture=window()->createTextureFromImage(image_,QQuickWindow::TextureHasMipmaps);
        if(!texture){delete node;return nullptr;}
        node->setTexture(texture);node->setOwnsTexture(true);node->revision=revision_;
    }
    node->setFiltering(scale()>=1.0?QSGTexture::Nearest:QSGTexture::Linear);
    const auto mip=scale()<1.0?QSGTexture::Linear:QSGTexture::None;
    if(auto* m=node->material())static_cast<QSGOpaqueTextureMaterial*>(m)->setMipmapFiltering(mip);
    if(auto* m=node->opaqueMaterial())static_cast<QSGOpaqueTextureMaterial*>(m)->setMipmapFiltering(mip);
    const auto rect=imageRect();node->setRect(rect);
    if(split_&&!comparison_image_.isNull()){
        const bool added=!node->comparison;
        if(added){node->comparison=new QSGSimpleTextureNode;node->comparisonRevision=0;}
        if(node->comparisonRevision!=comparison_revision_){auto* texture=window()->createTextureFromImage(comparison_image_,QQuickWindow::TextureHasMipmaps);
            if(!texture){if(added){delete node->comparison;node->comparison=nullptr;}return node;}
            node->comparison->setTexture(texture);node->comparison->setOwnsTexture(true);node->comparisonRevision=comparison_revision_;}
        node->comparison->setSourceRect(QRectF(0,0,comparison_image_.width()*0.5,comparison_image_.height()));
        node->comparison->setRect(QRectF(rect.x(),rect.y(),rect.width()*0.5,rect.height()));node->comparison->setFiltering(QSGTexture::Linear);
        if(auto* m=node->comparison->material())static_cast<QSGOpaqueTextureMaterial*>(m)->setMipmapFiltering(mip);
        if(auto* m=node->comparison->opaqueMaterial())static_cast<QSGOpaqueTextureMaterial*>(m)->setMipmapFiltering(mip);
        // An already-live parent notifies the software scene graph immediately
        // on insertion; that visitor requires the texture to be initialized.
        if(added)node->appendChildNode(node->comparison);
    }else if(node->comparison){delete node->comparison;node->comparison=nullptr;}
    return node;
}
void FrameCanvas::geometryChange(const QRectF& next,const QRectF& previous) {
    QQuickItem::geometryChange(next,previous);clampPan();update();emit viewChanged();
}
void FrameCanvas::itemChange(ItemChange change,const ItemChangeData& data) {
    QQuickItem::itemChange(change,data);
    if(change==ItemDevicePixelRatioHasChanged){clampPan();update();emit viewChanged();}
}
void FrameCanvas::mousePressEvent(QMouseEvent* e) {
    if(image_.isNull()||(e->button()!=Qt::LeftButton&&e->button()!=Qt::MiddleButton)){e->ignore();return;}
    forceActiveFocus(Qt::MouseFocusReason);dragging_=true;last_mouse_=e->position();setCursor(Qt::ClosedHandCursor);e->accept();
}
void FrameCanvas::mouseMoveEvent(QMouseEvent* e) {if(dragging_){const auto delta=e->position()-last_mouse_;last_mouse_=e->position();panBy(delta.x(),delta.y());e->accept();}else e->ignore();}
void FrameCanvas::mouseReleaseEvent(QMouseEvent* e) {dragging_=false;setCursor(image_.isNull()?Qt::ArrowCursor:Qt::OpenHandCursor);e->accept();}
void FrameCanvas::mouseUngrabEvent(){dragging_=false;setCursor(image_.isNull()?Qt::ArrowCursor:Qt::OpenHandCursor);}
void FrameCanvas::mouseDoubleClickEvent(QMouseEvent* e){if(e->button()!=Qt::LeftButton||image_.isNull()){e->ignore();return;}resetView(!fit_);e->accept();}
void FrameCanvas::wheelEvent(QWheelEvent* e){
    if(image_.isNull()){e->ignore();return;}
    const double amount=e->pixelDelta().isNull()?e->angleDelta().y():e->pixelDelta().y();
    if(amount!=0){zoomAt(e->position(),std::pow(1.2,std::clamp(amount/120.0,-20.0,20.0)));e->accept();return;}
    const double horizontal=e->pixelDelta().isNull()?e->angleDelta().x()/3.0:e->pixelDelta().x();
    if(horizontal!=0){panBy(horizontal,0);e->accept();return;}e->ignore();
}
