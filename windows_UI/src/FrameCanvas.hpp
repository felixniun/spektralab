#pragma once
#include <QImage>
#include <QQuickItem>

class FrameCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QImage comparisonImage READ comparisonImage WRITE setComparisonImage NOTIFY imageChanged)
    Q_PROPERTY(bool split READ split WRITE setSplit NOTIFY viewChanged)
    Q_PROPERTY(bool fit READ fit WRITE setFit NOTIFY viewChanged)
    Q_PROPERTY(double scalePercent READ scalePercent NOTIFY viewChanged)
    Q_PROPERTY(QRectF imageRect READ imageRect NOTIFY viewChanged)
public:
    explicit FrameCanvas(QQuickItem* parent=nullptr);
    QImage image() const { return image_; }
    void setImage(const QImage& image);
    QImage comparisonImage() const {return comparison_image_;}
    void setComparisonImage(const QImage& image);
    bool split() const {return split_;}
    void setSplit(bool value);
    bool fit() const { return fit_; }
    void setFit(bool fit);
    double scalePercent() const;
    QRectF imageRect() const;
    Q_INVOKABLE void resetView(bool fit=true);
    Q_INVOKABLE void panBy(double x,double y);
    Q_INVOKABLE void zoomBy(double factor);
signals:
    void imageChanged();
    void viewChanged();
protected:
    QSGNode* updatePaintNode(QSGNode* old,UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& next,const QRectF& previous) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseUngrabEvent() override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void itemChange(ItemChange,const ItemChangeData&) override;
private:
    double dpr() const;
    double scale() const;
    void clampPan();
    void zoomAt(QPointF position,double factor);
    QImage image_;
    QImage comparison_image_;
    quint64 comparison_revision_=0;
    bool split_=false;
    double zoom_=1.0;
    quint64 revision_=0;
    bool fit_=true,dragging_=false;
    QPointF pan_,last_mouse_;
};
