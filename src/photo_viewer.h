#pragma once
#include "image_decoder.h"
#include <QWidget>
#include <QPointF>
#include "watermark.h"
#include "photo_edits.h"
#include <QThreadPool>
#include <optional>
#include <QTimer>

class QPainter;

namespace photocull {
class PhotoViewer : public QWidget {
    Q_OBJECT
public:
    explicit PhotoViewer(QWidget* parent = nullptr);
    ~PhotoViewer() override;
    void resetPhoto();
    void setPreview(const DecodedImage& result);
    void setDetail(const DecodedImage& result);
    void setMessage(QString text);
    void setLoading(bool loading);
    void showToast(const QString& text);
    void setWatermark(const WatermarkSnapshot& snapshot);
    void setEdits(const PhotoEdits& edits);
    const PhotoEdits& edits() const {return edits_;}
    QSize editedSourceSize() const {return source_;}
    void beginCrop();
    void cancelCrop();
    void confirmCrop();
    bool isCropping() const {return cropping_;}
    void setCropAspect(double ratio);
    QRectF cropSelection() const {return cropSelection_;} // Rotated, normalized draft.
    QSize displayTarget() const;
    bool isFit() const { return fit_; }
    bool hasDetail() const { return !detail_.isNull(); }
    bool hasPhoto() const {return !preview_.isNull();}
    QImage histogramImage() const;
    QImage sortingSnapshot() const;
    QRect sortingBounds() const;
    double zoomPercent() const { return scale_ * devicePixelRatioF() * 100.0; }
    void fit();
    void actualSize();
    void zoomBy(double factor);
signals:
    void targetChanged();
    void zoomChanged(double percent);
    void detailRequired(bool enabled);
    void cropConfirmed(QRectF originalNormalized);
    void cropModeChanged(bool active);
    void editsRendered();
    void fullscreenRequested();
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool event(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    void zoomAt(double factor, QPointF anchor);
    void updateFit();
    void constrainPan();
    QRectF imageRect() const;
    void paintImage(QPainter&) const;
    void scheduleRender();
    void startRender();
    QPointF imagePoint(QPointF point) const;
    QRectF cropScreenRect() const;
    QRectF cropFromPoints(QPointF anchor,QPointF end) const;
    QImage preview_, detail_,rendered_;
    QSize source_, previewSource_,baseSource_;
    double scale_ = 1.0;
    QPointF pan_, dragPosition_;
    bool fit_ = true, actual_ = false, dragging_ = false, loading_ = false;
    QString message_;
    QString toast_;
    QTimer toastTimer_;
    WatermarkSnapshot watermark_;
    PhotoEdits edits_;
    struct RenderJob {QImage image;PhotoEdits edits;quint64 generation;bool fullQuality=false;};
    QThreadPool renderPool_;
    QTimer refineTimer_;
    std::optional<RenderJob> pendingRender_;
    CancelToken renderCancel_;
    quint64 renderGeneration_=0;
    bool renderActive_=false,closing_=false,cropping_=false,cropPending_=false;
    QRectF cropSelection_,cropStart_;
    QPointF cropAnchor_,cropPress_;
    double cropAspect_=0;
    int cropDrag_=0; // 1 draw/resize, 2 move.
};
}
