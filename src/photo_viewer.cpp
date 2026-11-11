#include "photo_viewer.h"
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QPainterPath>
#include <QRunnable>
#include <cmath>

namespace photocull {
PhotoViewer::PhotoViewer(QWidget* parent) : QWidget(parent) {
    setMinimumSize(160, 120);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    toastTimer_.setSingleShot(true);
    connect(&toastTimer_,&QTimer::timeout,this,[this]{toast_.clear();update();});
    message_ = "Open a folder to start browsing";
    renderPool_.setMaxThreadCount(1);renderPool_.setThreadPriority(QThread::LowPriority);
    refineTimer_.setSingleShot(true);refineTimer_.setInterval(220);
    connect(&refineTimer_,&QTimer::timeout,this,[this]{
        const auto image=detail_.isNull()?preview_:detail_;auto tone=edits_;tone.crop={0,0,1,1};tone.rotation=0;tone.rotationDegrees=0;tone.cropInRotatedSpace=false;
        if(image.isNull()||!tone.hasTone())return;
        pendingRender_=RenderJob{image,tone,renderGeneration_,true};startRender();
    });
}
PhotoViewer::~PhotoViewer(){closing_=true;++renderGeneration_;if(renderCancel_)renderCancel_->store(true);pendingRender_.reset();renderPool_.waitForDone();}
QSize PhotoViewer::displayTarget() const {
    return {qMax(1, int(std::ceil(width() * devicePixelRatioF()))),
            qMax(1, int(std::ceil(height() * devicePixelRatioF())))};
}
void PhotoViewer::resetPhoto() {
    ++renderGeneration_;if(renderCancel_)renderCancel_->store(true);pendingRender_.reset();
    refineTimer_.stop();
    preview_ = {}; detail_ = {};rendered_={}; source_ = {}; previewSource_ = {};baseSource_={}; pan_ = {};edits_={};
    if(cropping_){cropping_=false;emit cropModeChanged(false);}cropPending_=false;cropDrag_=0;cropSelection_={};
    fit_ = true; actual_ = false; message_.clear(); loading_ = true;
    toastTimer_.stop();toast_.clear();
    update();
}
void PhotoViewer::setPreview(const DecodedImage& result) {
    loading_ = false;
    if (result.image.isNull()) { message_ = result.error; update(); return; }
    preview_ = result.image;
    previewSource_ = result.sourceSize.isValid() ? result.sourceSize : preview_.size();
    if (detail_.isNull())baseSource_=previewSource_;
    message_.clear();
    scheduleRender();
    if(cropPending_){cropPending_=false;beginCrop();}
    update();
}
void PhotoViewer::setDetail(const DecodedImage& result) {
    if (fit_) return;
    if (result.image.isNull()) { message_ = result.error; update(); return; }
    // Preserve the same visual field when the RAW preview resolution changes.
    if (!actual_ && baseSource_.width() > 0) scale_ *= double(baseSource_.width()) / result.image.width();
    detail_ = result.image;baseSource_=detail_.size();scheduleRender();
    if (actual_) scale_ = 1.0 / devicePixelRatioF();
    message_.clear(); constrainPan(); emit zoomChanged(zoomPercent()); update();
}
void PhotoViewer::setMessage(QString text) { message_ = std::move(text); update(); }
void PhotoViewer::setLoading(bool loading) { loading_ = loading; update(); }
void PhotoViewer::showToast(const QString& text) { toast_=text;toastTimer_.start(1400);update(); }
QImage PhotoViewer::histogramImage() const {
    const auto& image=rendered_.isNull()?(detail_.isNull()?preview_:detail_):rendered_;
    if(image.isNull())return {};
    auto edits=edits_;if(!rendered_.isNull()){edits.exposure=edits.brightness=edits.contrast=edits.highlights=edits.shadows=edits.saturation=0;}
    return renderEdits(image.scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation),edits);
}
void PhotoViewer::setWatermark(const WatermarkSnapshot& snapshot) {watermark_=snapshot;update();}
QRect PhotoViewer::sortingBounds() const {return hasPhoto()?imageRect().toAlignedRect().intersected(rect()):QRect();}
QImage PhotoViewer::sortingSnapshot() const {
    const auto bounds=sortingBounds();if(bounds.isEmpty())return {};
    const auto pixels=bounds.size()*devicePixelRatioF();
    const auto size=pixels.width()>1280||pixels.height()>960?pixels.scaled(1280,960,Qt::KeepAspectRatio):pixels;
    QImage snapshot(size,QImage::Format_ARGB32_Premultiplied);snapshot.fill(QColor("#161617"));
    QPainter painter(&snapshot);painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.scale(qreal(size.width())/bounds.width(),qreal(size.height())/bounds.height());painter.translate(-bounds.topLeft());paintImage(painter);
    return snapshot;
}
void PhotoViewer::updateFit() {
    if (!source_.isValid()) return;
    scale_ = qMin(double(width()) / source_.width(), double(height()) / source_.height());
    pan_ = {}; emit zoomChanged(zoomPercent());
}
void PhotoViewer::fit() {
    fit_ = true; actual_ = false; detail_ = {};
    baseSource_ = previewSource_;scheduleRender();
    // Source dimensions of a standard preview can differ from its decoded size.
    emit detailRequired(false);
    updateFit(); message_.clear(); update();
}
void PhotoViewer::actualSize() {
    if (preview_.isNull()) return;
    const bool wasFit = fit_;
    fit_ = false; actual_ = true; scale_ = 1.0 / devicePixelRatioF(); pan_ = {};
    emit zoomChanged(zoomPercent()); update();
    if (wasFit) emit detailRequired(true);
}
void PhotoViewer::zoomBy(double factor) { zoomAt(factor, QPointF(width() / 2.0, height() / 2.0)); }
void PhotoViewer::zoomAt(double factor, QPointF anchor) {
    if (preview_.isNull()) return;
    const bool wasFit = fit_;
    fit_ = false; actual_ = false;
    const QPointF center(width() / 2.0, height() / 2.0);
    const double newScale = qBound(0.005 / devicePixelRatioF(), scale_ * factor, 32.0 / devicePixelRatioF());
    const double ratio = newScale / scale_;
    pan_ = anchor - center - (anchor - center - pan_) * ratio;
    scale_ = newScale; constrainPan(); emit zoomChanged(zoomPercent()); update();
    if (wasFit) emit detailRequired(true);
}
QRectF PhotoViewer::imageRect() const {
    const QSizeF drawn(source_.width() * scale_, source_.height() * scale_);
    return {QPointF((width() - drawn.width()) / 2.0, (height() - drawn.height()) / 2.0) + pan_, drawn};
}
void PhotoViewer::constrainPan() {
    const double x = qMax(0.0, (source_.width() * scale_ - width()) / 2.0);
    const double y = qMax(0.0, (source_.height() * scale_ - height()) / 2.0);
    pan_.setX(qBound(-x, pan_.x(), x)); pan_.setY(qBound(-y, pan_.y(), y));
}
void PhotoViewer::paintImage(QPainter& p) const {
    const QImage& image=rendered_.isNull()?(detail_.isNull()?preview_:detail_):rendered_;
    if(image.isNull())return;
        p.setRenderHint(QPainter::SmoothPixmapTransform, zoomPercent() < 100.0);
        auto geometry=edits_;if(cropping_)geometry.crop={0,0,1,1};
        const auto pixels=editCropPixels(image.size(),geometry);const auto target=imageRect();
        p.save();p.setClipRect(target,Qt::IntersectClip);p.translate(target.topLeft());
        p.scale(target.width()/pixels.width(),target.height()/pixels.height());p.translate(-pixels.left(),-pixels.top());
        p.setRenderHint(QPainter::SmoothPixmapTransform,editRotationDegrees(geometry)%90||zoomPercent()<100.0);
        p.setWorldTransform(editRotationTransform(image.size(),geometry),true);
        p.drawImage(QRectF(QPointF(),image.size()),image);p.restore();
        paintWatermark(p,cropping_&&!cropSelection_.isEmpty()?cropScreenRect():imageRect(),watermark_);
}
void PhotoViewer::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#161617"));
    const QImage& image = rendered_.isNull()?(detail_.isNull()?preview_:detail_):rendered_;
    if (!image.isNull()) {
        paintImage(p);
        if(cropping_){
            QPainterPath outside;outside.setFillRule(Qt::OddEvenFill);outside.addRect(imageRect());
            if(!cropSelection_.isEmpty())outside.addRect(cropScreenRect());
            p.fillPath(outside,QColor(0,0,0,145));
            if(!cropSelection_.isEmpty()){
                const auto box=cropScreenRect();p.setPen(QPen(Qt::white,1));p.drawRect(box);
                for(auto point:{box.topLeft(),box.topRight(),box.bottomLeft(),box.bottomRight()})p.fillRect(QRectF(point-QPointF(4,4),QSizeF(8,8)),Qt::white);
            }
        }
    }
    p.setPen(QColor("#a9adb6"));
    if (image.isNull()) {
        auto f = font(); f.setPointSize(14); p.setFont(f);
        p.drawText(rect().adjusted(28, 20, -28, -20), Qt::AlignCenter | Qt::TextWordWrap,
                   loading_ ? "Loading preview…" : message_);
    } else if (loading_ || !message_.isEmpty()) {
        const QString text = loading_ ? "Loading preview…" : message_;
        const QRect box = QRect(16, 16, qMin(width() - 32, p.fontMetrics().horizontalAdvance(text) + 24), 32);
        p.fillRect(box, QColor(20, 22, 26, 220));
        p.drawText(box.adjusted(12, 0, -8, 0), Qt::AlignVCenter, text);
    }
    if(!toast_.isEmpty()){
        p.save();auto font=p.font();
        if(font.pixelSize()>0)font.setPixelSize(qRound(font.pixelSize()*1.4));
        else font.setPointSizeF(qMax(9.0,font.pointSizeF())*1.4);
        font.setWeight(QFont::DemiBold);p.setFont(font);
        const auto text=p.fontMetrics().elidedText(toast_,Qt::ElideRight,qMax(20,width()-80));
        auto box=p.fontMetrics().boundingRect(text).adjusted(-22,-10,22,10);box.moveCenter(QPoint(width()/2,height()-58));
        p.setPen(Qt::NoPen);p.setBrush(QColor(12,12,14,220));p.drawRoundedRect(box,box.height()/2.0,box.height()/2.0);p.setPen(QColor("#f2f2f5"));p.drawText(box,Qt::AlignCenter,text);p.restore();
    }
}
void PhotoViewer::resizeEvent(QResizeEvent*) {
    if (fit_) updateFit(); else constrainPan();
    emit targetChanged();
}
bool PhotoViewer::event(QEvent* event) {
    if (event->type() == QEvent::DevicePixelRatioChange) {
        if (fit_) updateFit();
        else if (actual_) { scale_ = 1.0 / devicePixelRatioF(); emit zoomChanged(zoomPercent()); }
        emit targetChanged(); update();
    }
    return QWidget::event(event);
}
void PhotoViewer::wheelEvent(QWheelEvent* e) {
    const double delta = e->pixelDelta().isNull() ? e->angleDelta().y() / 120.0 : e->pixelDelta().y() / 80.0;
    zoomAt(std::pow(1.2, delta), e->position()); e->accept();
}
void PhotoViewer::mouseDoubleClickEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton&&!cropping_){dragging_=false;emit fullscreenRequested();e->accept();return;}
    QWidget::mouseDoubleClickEvent(e);
}
void PhotoViewer::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if(cropping_&&e->button()==Qt::LeftButton&&!(e->modifiers()&Qt::AltModifier)){
        cropPress_=imagePoint(e->position());cropStart_=cropSelection_;const auto box=cropScreenRect();
        if(!cropSelection_.isEmpty()){
            const QPointF corners[]={box.topLeft(),box.topRight(),box.bottomLeft(),box.bottomRight()};
            const QPointF anchors[]={cropSelection_.bottomRight(),cropSelection_.bottomLeft(),cropSelection_.topRight(),cropSelection_.topLeft()};
            for(int i=0;i<4;++i)if(QLineF(e->position(),corners[i]).length()<=10){cropAnchor_=anchors[i];cropDrag_=1;e->accept();return;}
            if(box.contains(e->position())){cropDrag_=2;e->accept();return;}
        }
        cropAnchor_=cropPress_;cropSelection_={};cropDrag_=1;e->accept();update();return;
    }
    if ((e->button() == Qt::LeftButton||e->button()==Qt::MiddleButton) && !fit_) {
        dragging_ = true; dragPosition_ = e->position(); setCursor(Qt::ClosedHandCursor); e->accept();
    }
}
void PhotoViewer::mouseMoveEvent(QMouseEvent* e) {
    if(cropDrag_){
        const auto point=imagePoint(e->position());
        if(cropDrag_==1)cropSelection_=cropFromPoints(cropAnchor_,point);
        else{const auto delta=point-cropPress_;const double x=qBound(0.0,cropStart_.left()+delta.x(),1-cropStart_.width()),y=qBound(0.0,cropStart_.top()+delta.y(),1-cropStart_.height());cropSelection_=QRectF(QPointF(x,y),cropStart_.size());}
        update();e->accept();return;
    }
    if (dragging_) { pan_ += e->position() - dragPosition_; dragPosition_ = e->position(); constrainPan(); update(); }
}
void PhotoViewer::mouseReleaseEvent(QMouseEvent*) { dragging_ = false;cropDrag_=0; unsetCursor(); }
void PhotoViewer::setEdits(const PhotoEdits& edits){if(!validateEdits(edits).isEmpty())return;edits_=edits;scheduleRender();}
void PhotoViewer::scheduleRender(){
    refineTimer_.stop();
    ++renderGeneration_;if(renderCancel_)renderCancel_->store(true);
    auto renderParameters=edits_;if(cropping_)renderParameters.crop={0,0,1,1};
    source_=editedSize(baseSource_,renderParameters);
    // Crop and rotation are painter transforms, applied immediately.
    // Only tone processing needs an asynchronous pixel buffer.
    renderParameters.rotation=0;renderParameters.rotationDegrees=0;renderParameters.cropInRotatedSpace=false;renderParameters.crop={0,0,1,1};
    if(fit_)updateFit();else constrainPan();
    const auto image=detail_.isNull()?preview_:detail_;
    if(image.isNull()||renderParameters.isIdentity()){
        pendingRender_.reset();rendered_={};update();emit editsRendered();return;
    }
    pendingRender_=RenderJob{image,renderParameters,renderGeneration_};startRender();update();
    if(qint64(image.width())*image.height()>2000000)refineTimer_.start();
}
void PhotoViewer::startRender(){
    if(renderActive_||closing_||!pendingRender_)return;
    const auto job=*pendingRender_;pendingRender_.reset();renderActive_=true;renderCancel_=std::make_shared<std::atomic_bool>(false);const auto cancel=renderCancel_;
    renderPool_.start(QRunnable::create([this,job,cancel]{
        QImage result;try{
            auto image=job.image;
            const qint64 pixels=qint64(image.width())*image.height();
            if(!job.fullQuality&&pixels>2000000){const double factor=std::sqrt(2000000.0/pixels);image=image.scaled(qMax(1,int(image.width()*factor)),qMax(1,int(image.height()*factor)),Qt::KeepAspectRatio,Qt::SmoothTransformation);}
            result=renderEdits(std::move(image),job.edits,cancel);
        }catch(...){ }
        QMetaObject::invokeMethod(this,[this,job,result=std::move(result)]()mutable{
            renderActive_=false;if(closing_)return;
            if(job.generation==renderGeneration_){if(!result.isNull())rendered_=std::move(result);else message_="Could not render photo edits.";update();emit editsRendered();}
            startRender();
        },Qt::QueuedConnection);
    }));
}
QPointF PhotoViewer::imagePoint(QPointF point) const {
    const auto r=imageRect();if(r.isEmpty())return {};
    return {qBound(0.0,(point.x()-r.left())/r.width(),1.0),qBound(0.0,(point.y()-r.top())/r.height(),1.0)};
}
QRectF PhotoViewer::cropScreenRect() const {const auto r=imageRect();return {r.topLeft()+QPointF(cropSelection_.x()*r.width(),cropSelection_.y()*r.height()),QSizeF(cropSelection_.width()*r.width(),cropSelection_.height()*r.height())};}
QRectF PhotoViewer::cropFromPoints(QPointF anchor,QPointF end) const {
    double w=qAbs(end.x()-anchor.x()),h=qAbs(end.y()-anchor.y());
    const bool left=end.x()<anchor.x(),top=end.y()<anchor.y();
    if(cropAspect_!=0&&source_.isValid()){
        const double ratio=cropAspect_<0?double(source_.width())/source_.height():cropAspect_;
        const double normalizedRatio=ratio*source_.height()/source_.width();
        if(w>h*normalizedRatio)h=w/normalizedRatio;else w=h*normalizedRatio;
        const double maxW=left?anchor.x():1-anchor.x(),maxH=top?anchor.y():1-anchor.y();
        const double scale=qMin(1.0,qMin(w>0?maxW/w:1.0,h>0?maxH/h:1.0));w*=scale;h*=scale;
    }
    return {QPointF(left?anchor.x()-w:anchor.x(),top?anchor.y()-h:anchor.y()),QSizeF(w,h)};
}
void PhotoViewer::beginCrop(){
    if(preview_.isNull()){cropPending_=!cropPending_;return;}
    cropping_=true;cropDrag_=0;
    cropSelection_=edits_.crop==QRectF(0,0,1,1)?QRectF():editRotatedCrop(baseSource_,edits_);
    scheduleRender();setCropAspect(cropAspect_);emit cropModeChanged(true);
}
void PhotoViewer::cancelCrop(){cropPending_=false;if(!cropping_)return;cropping_=false;cropDrag_=0;scheduleRender();emit cropModeChanged(false);}
void PhotoViewer::confirmCrop(){
    if(!cropping_)return;
    if(cropSelection_.width()<0.001||cropSelection_.height()<0.001){cancelCrop();return;}
    const int angle=editRotationDegrees(edits_);
    edits_.cropInRotatedSpace=angle%90!=0||edits_.cropInRotatedSpace;
    const auto crop=edits_.cropInRotatedSpace?cropSelection_:rotatedNormalizedRect(cropSelection_,-angle/90);
    cropping_=false;cropDrag_=0;edits_.crop=crop;scheduleRender();emit cropModeChanged(false);emit cropConfirmed(crop);
}
void PhotoViewer::setCropAspect(double ratio){
    cropAspect_=ratio<0?-1:ratio;
    if(cropping_&&!cropSelection_.isEmpty()&&cropAspect_!=0){const auto center=cropSelection_.center();auto r=cropFromPoints(cropSelection_.topLeft(),cropSelection_.bottomRight());r.moveCenter(center);r.moveLeft(qBound(0.0,r.left(),1-r.width()));r.moveTop(qBound(0.0,r.top(),1-r.height()));cropSelection_=r;update();}
}
void PhotoViewer::keyPressEvent(QKeyEvent* event){
    if(cropping_&&(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter)){confirmCrop();event->accept();return;}
    if(cropping_&&event->key()==Qt::Key_Escape){cancelCrop();event->accept();return;}
    QWidget::keyPressEvent(event);
}
}
