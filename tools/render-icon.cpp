#include <QGuiApplication>
#include <QSvgRenderer>
#include <QPainter>
#include <QBuffer>
#include <QDataStream>
#include <QFile>

// Rebuild the Windows multi-resolution icon from the editable SVG master.
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    if (argc != 2) return 1;
    const QString base = QString::fromLocal8Bit(argv[1]);
    QSvgRenderer svg(base + ".svg");
    if (!svg.isValid()) return 2;
    QList<QByteArray> images;
    const QList<int> sizes = {16, 24, 32, 48, 64, 128, 256};
    for (int size : sizes) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        { QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing); svg.render(&painter); }
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) return 3;
        images.append(bytes);
        if (size == 256 && !image.save(base + ".png")) return 4;
    }
    QFile file(base + ".ico"); if (!file.open(QIODevice::WriteOnly)) return 5;
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    out << quint16(0) << quint16(1) << quint16(sizes.size());
    quint32 offset = 6 + 16 * sizes.size();
    for (int i = 0; i < sizes.size(); ++i) {
        out << quint8(sizes[i] % 256) << quint8(sizes[i] % 256) << quint8(0) << quint8(0)
            << quint16(1) << quint16(32) << quint32(images[i].size()) << offset;
        offset += images[i].size();
    }
    for (const auto& bytes : images) out.writeRawData(bytes.constData(), bytes.size());
    return out.status() == QDataStream::Ok ? 0 : 6;
}
