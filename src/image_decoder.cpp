#include "image_decoder.h"
#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QTransform>
#include <libraw/libraw.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

static_assert(LIBRAW_COMPILE_CHECK_VERSION_NOTLESS(0, 21), "LibRaw >= 0.21 required");
namespace photocull {
namespace {
bool cancelled(const CancelToken& token) { return token && token->load(); }
QSize oriented(QSize size, int flip) { return (flip & 4) ? size.transposed() : size; }
QImage finishImage(QImage image, QSize target, bool full) {
    if (!full && (image.width() > target.width() || image.height() > target.height()))
        image = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace(QColorSpace::SRgb))
        image.convertToColorSpace(QColorSpace::SRgb);
    return image;
}
DecodedImage readImage(QImageReader& reader, const DecodeRequest& request, bool autoTransform = true,
                       int extraFlip = 0) {
    reader.setAutoTransform(autoTransform);
    QSize original = reader.size();
    const bool swap = autoTransform ? (int(reader.transformation()) & int(QImageIOHandler::TransformationRotate90)) != 0
                                    : (extraFlip & 4) != 0;
    const QSize source = swap ? original.transposed() : original;
    if (!request.fullResolution && original.isValid()) {
        const QSize rawTarget = swap ? request.target.transposed() : request.target;
        reader.setScaledSize(original.scaled(rawTarget, Qt::KeepAspectRatio).boundedTo(original));
    }
    QImage image = reader.read();
    if (image.isNull()) return {{}, {}, "Cannot read image: " + reader.errorString(), {}};
    if (!autoTransform && extraFlip) image = applyRawFlip(std::move(image), extraFlip);
    return {finishImage(std::move(image), request.target, request.fullResolution), source, {}, {}};
}
struct RawDeleter { void operator()(LibRaw* raw) const { raw->recycle(); delete raw; } };
using RawPtr = std::unique_ptr<LibRaw, RawDeleter>;
using MemPtr = std::unique_ptr<libraw_processed_image_t, decltype(&LibRaw::dcraw_clear_mem)>;
int progress(void* context, LibRaw_progress, int, int) {
    return static_cast<std::atomic_bool*>(context)->load() ? 1 : 0;
}
QImage bitmap(const libraw_processed_image_t& data) {
    if (data.type != LIBRAW_IMAGE_BITMAP || data.bits != 8 || !data.width || !data.height) return {};
    if (data.colors != 1 && data.colors != 3 && data.colors != 4) return {};
    const quint64 stride = quint64(data.width) * data.colors;
    if (stride * data.height > data.data_size || stride > quint64(std::numeric_limits<int>::max())) return {};
    const auto format = data.colors == 1 ? QImage::Format_Grayscale8 :
                        data.colors == 3 ? QImage::Format_RGB888 : QImage::Format_RGBA8888;
    return QImage(data.data, data.width, data.height, qsizetype(stride), format).copy();
}
}

QVector<int> rankThumbnails(QVector<ThumbnailCandidate> candidates, QSize target, bool largest) {
    auto area = [](QSize s) { return qint64(s.width()) * s.height(); };
    auto adequate = [&](const ThumbnailCandidate& c) {
        const auto size = oriented(c.size, c.flip);
        const auto needed = size.scaled(target, Qt::KeepAspectRatio);
        return size.width() >= needed.width() && size.height() >= needed.height();
    };
    std::stable_sort(candidates.begin(), candidates.end(), [&](const auto& a, const auto& b) {
        if (largest) return area(a.size) > area(b.size);
        const bool aa = a.jpeg && adequate(a), ba = b.jpeg && adequate(b);
        if (aa != ba) return aa;
        // If no adequate JPEG exists, use the largest available preview of any type.
        return aa ? area(a.size) < area(b.size) : area(a.size) > area(b.size);
    });
    QVector<int> result;
    for (const auto& c : candidates) result.append(c.index);
    return result;
}

std::optional<int> jpegExifOrientation(const QByteArray& jpeg) {
    const auto* p = reinterpret_cast<const uchar*>(jpeg.constData());
    const qsizetype n = jpeg.size();
    if (n < 4 || p[0] != 0xff || p[1] != 0xd8) return {};
    qsizetype offset = 2;
    while (offset + 4 <= n) {
        if (p[offset++] != 0xff) break;
        while (offset < n && p[offset] == 0xff) ++offset;
        if (offset + 3 > n) break;
        const int marker = p[offset++];
        if (marker == 0xda || marker == 0xd9) break;
        const qsizetype len = (p[offset] << 8) | p[offset + 1];
        if (len < 2 || len > n - offset) break;
        if (marker == 0xe1 && len >= 16 && QByteArray::fromRawData(jpeg.constData() + offset + 2, 6) == QByteArray("Exif\0\0", 6)) {
            const qsizetype base = offset + 8, end = offset + len;
            const bool little = p[base] == 'I' && p[base + 1] == 'I';
            if (!little && !(p[base] == 'M' && p[base + 1] == 'M')) return {};
            auto u16 = [&](qsizetype i) -> quint16 {
                return little ? quint16(p[i] | p[i + 1] << 8) : quint16(p[i] << 8 | p[i + 1]);
            };
            auto u32 = [&](qsizetype i) -> quint32 {
                return little ? quint32(u16(i)) | quint32(u16(i + 2)) << 16
                              : quint32(u16(i)) << 16 | u16(i + 2);
            };
            if (u16(base + 2) != 42) return {};
            const quint32 ifdOffset = u32(base + 4);
            if (quint64(ifdOffset) + 2 > quint64(end - base)) return {};
            const qsizetype ifd = base + ifdOffset;
            const int count = u16(ifd);
            for (int j = 0; j < count; ++j) {
                const qsizetype tag = ifd + 2 + j * 12;
                if (tag + 12 > end) break;
                if (u16(tag) == 0x112 && u16(tag + 2) == 3 && u32(tag + 4) == 1) {
                    const int value = u16(tag + 8);
                    return value >= 1 && value <= 8 ? std::optional<int>(value) : std::nullopt;
                }
            }
        }
        offset += len;
    }
    return {};
}
QImage applyRawFlip(QImage image, int flip) {
    // LibRaw maps output coordinates back to source; invert that mapping here.
    if (flip & 3) image = image.mirrored((flip & 1) != 0, (flip & 2) != 0);
    if (flip & 4) image = image.transformed(QTransform(0, 1, 1, 0, 0, 0));
    return image;
}

DecodedImage ImageDecoder::decode(const DecodeRequest& request, const CancelToken& token) {
    if (cancelled(token)) return {};
    if (!request.target.isValid() && !request.fullResolution) return {{}, {}, "Viewer is not ready.", {}};
    try {
        // A broken JPEG companion should not hide an otherwise usable RAW preview.
        QStringList paths = request.photo.sources;
        paths.removeAll(request.photo.displayPath());
        paths.prepend(request.photo.displayPath());
        DecodedImage result;
        for (const auto& path : paths) {
            if (cancelled(token)) return {};
            if (!QFileInfo(path).isReadable()) { result.error = "Photo is missing or cannot be read."; continue; }
            if (isRawExtension(QFileInfo(path).suffix())) result = decodeRaw(path, request, token);
            else { QImageReader reader(path); result = readImage(reader, request); }
            if (!result.image.isNull()) return cancelled(token) ? DecodedImage{} : result;
        }
        return result;
    } catch (const std::bad_alloc&) {
        return {{}, {}, "Not enough memory to preview this photo.", {}};
    } catch (...) {
        return {{}, {}, "This photo could not be decoded.", {}};
    }
}

DecodedImage ImageDecoder::decodeRaw(const QString& path, const DecodeRequest& request, const CancelToken& token) {
    RawPtr raw(new LibRaw);
    raw->imgdata.rawparams.max_raw_memory_mb = 512;
    if (token) raw->set_progress_handler(progress, token.get());
#ifdef Q_OS_WIN
    const int opened = raw->open_file(reinterpret_cast<const wchar_t*>(path.utf16()));
#else
    const auto encoded = QFile::encodeName(path);
    const int opened = raw->open_file(encoded.constData());
#endif
    if (opened != LIBRAW_SUCCESS)
        return {{}, {}, "RAW preview unavailable: " + QString::fromLatin1(libraw_strerror(opened)), {}};
    const int flip = raw->imgdata.sizes.flip;
    QVector<ThumbnailCandidate> candidates;
    const auto& list = raw->imgdata.thumbs_list;
    for (int i = 0; i < qMin(list.thumbcount, LIBRAW_THUMBNAIL_MAXCOUNT); ++i) {
        const auto& t = list.thumblist[i];
        if (t.twidth && t.theight)
            candidates.append({i, QSize(t.twidth, t.theight), t.tformat == LIBRAW_INTERNAL_THUMBNAIL_JPEG, flip});
    }
    auto indices = rankThumbnails(candidates, request.target, request.fullResolution);
    if (indices.isEmpty()) indices.append(-1); // Legacy single-thumbnail metadata.
    for (int index : indices) {
        if (cancelled(token)) return {};
        int error = index < 0 ? raw->unpack_thumb() : raw->unpack_thumb_ex(index);
        if (LIBRAW_FATAL_ERROR(error)) break;
        if (error != LIBRAW_SUCCESS) continue;
        const auto& thumb = raw->imgdata.thumbnail;
        std::optional<int> ownOrientation;
        if (thumb.tformat == LIBRAW_THUMBNAIL_JPEG && thumb.thumb)
            ownOrientation = jpegExifOrientation(QByteArray::fromRawData(thumb.thumb, thumb.tlength));
        MemPtr mem(raw->dcraw_make_mem_thumb(&error), &LibRaw::dcraw_clear_mem);
        if (!mem || error != LIBRAW_SUCCESS) continue;
        DecodedImage result;
        if (mem->type == LIBRAW_IMAGE_JPEG) {
            // LibRaw may prepend synthesized EXIF if the original APP1 was not first.
            // Use original JPEG bytes when they carry an explicit orientation.
            auto bytes = ownOrientation ? QByteArray::fromRawData(thumb.thumb, thumb.tlength)
                                        : QByteArray::fromRawData(reinterpret_cast<const char*>(mem->data), mem->data_size);
            QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "jpeg");
            // Ignore any EXIF synthesized by LibRaw if the original thumbnail lacked orientation.
            result = readImage(reader, request, ownOrientation.has_value(), ownOrientation ? 0 : flip);
        } else {
            auto image = applyRawFlip(bitmap(*mem), flip);
            const auto size = image.size();
            result = {finishImage(std::move(image), request.target, request.fullResolution), size, {}, {}};
        }
        if (!result.image.isNull()) { result.note = "Embedded RAW preview"; return result; }
    }
    if (request.fullResolution || !request.allowRawFallback)
        return {{}, {}, "No usable embedded RAW preview.", {}};
    if (cancelled(token)) return {};
    // Reopen after thumbnail failure: fatal errors may have recycled LibRaw state.
    raw->recycle();
#ifdef Q_OS_WIN
    int error = raw->open_file(reinterpret_cast<const wchar_t*>(path.utf16()));
#else
    int error = raw->open_file(encoded.constData());
#endif
    if (error != LIBRAW_SUCCESS) return {{}, {}, "Cannot reopen RAW photo.", {}};
    raw->imgdata.rawparams.max_raw_memory_mb = 512;
    raw->imgdata.params.half_size = 1;
    raw->imgdata.params.output_bps = 8;
    raw->imgdata.params.output_color = 1;
    raw->imgdata.params.gamm[0] = 1.0/2.4;
    raw->imgdata.params.gamm[1] = 12.92;
    raw->imgdata.params.auto_bright_thr = 0.001f;
    raw->imgdata.params.use_camera_wb = 1;
    raw->imgdata.params.user_qual = 0;
    if (token) raw->set_progress_handler(progress, token.get());
    error = raw->unpack();
    if (!cancelled(token) && error == LIBRAW_SUCCESS) error = raw->dcraw_process();
    if (cancelled(token)) return {};
    if (error != LIBRAW_SUCCESS)
        return {{}, {}, "Cannot preview RAW: " + QString::fromLatin1(libraw_strerror(error)), {}};
    MemPtr mem(raw->dcraw_make_mem_image(&error), &LibRaw::dcraw_clear_mem);
    if (!mem || error != LIBRAW_SUCCESS) return {{}, {}, "Cannot create RAW preview.", {}};
    auto image = bitmap(*mem); // dcraw_make_mem_image already applies orientation.
    if (image.isNull()) return {{}, {}, "Unsupported RAW preview layout.", {}};
    image.setColorSpace(QColorSpace::SRgb);
    const auto source = oriented(QSize(raw->imgdata.sizes.width, raw->imgdata.sizes.height), raw->imgdata.sizes.flip);
    return {finishImage(std::move(image), request.target, false), source, {}, "Half-size RAW fallback"};
}
}
