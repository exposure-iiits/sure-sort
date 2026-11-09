#include "folder_scanner.h"
#include <QCollator>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRunnable>
#include <algorithm>

namespace photocull {
bool isRawExtension(const QString& extension) {
    static const QStringList raw = {"arw", "cr2", "cr3", "nef", "dng", "raf", "rw2", "orf"};
    return raw.contains(extension.toLower());
}
bool isSupportedExtension(const QString& extension) {
    const auto ext = extension.toLower();
    return ext == "jpg" || ext == "jpeg" || ext == "png" || isRawExtension(ext);
}
QString Photo::displayPath() const {
    for (const auto& path : sources) {
        const auto ext = QFileInfo(path).suffix().toLower();
        if (ext == "jpg" || ext == "jpeg") return path;
    }
    return sources.value(0);
}
QString Photo::name() const { return QFileInfo(displayPath()).fileName(); }
QString Photo::typeLabel() const {
    QStringList types;
    for (const auto& path : sources) {
        const auto type = QFileInfo(path).suffix().toUpper();
        if (!types.contains(type)) types.append(type);
    }
    std::stable_sort(types.begin(), types.end(), [](const auto& a, const auto& b) {
        const bool ar = isRawExtension(a), br = isRawExtension(b);
        return ar != br ? ar : a < b;
    });
    return types.join('+');
}
ScanResult scanFolder(const QString& folder, const CancelToken& cancel) {
    ScanResult result;
    const QFileInfo info(folder);
    if (!info.exists() || !info.isDir() || !info.isReadable()) {
        result.error = "Cannot read this folder. Check that the drive is connected and access is allowed.";
        return result;
    }
    QHash<QString, int> groups;
    QDirIterator it(info.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (cancel && cancel->load()) return {};
        it.next();
        const auto file = it.fileInfo();
        const auto ext = file.suffix().toLower();
        if (!isSupportedExtension(ext)) continue;
        // Pair companions only within their own directory; PNGs remain separate.
        const QString group = (ext == "png" ? "png:" : "photo:") + file.completeBaseName().toCaseFolded();
        // Use the containing directory so IDs stay stable when opening a parent folder.
        QString identity = file.absolutePath() + '/' + group;
#ifdef Q_OS_WIN
        identity = identity.toCaseFolded();
#endif
        const auto found = groups.constFind(identity);
        if (found == groups.cend()) {
            const auto id = QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
            groups.insert(identity, int(result.photos.size()));
            result.photos.append({id, {file.absoluteFilePath()}});
        } else result.photos[*found].sources.append(file.absoluteFilePath());
    }
    if (cancel && cancel->load()) return {};
    for (auto& photo : result.photos) std::sort(photo.sources.begin(), photo.sources.end());
    // C locale numeric collation is not implemented by some Qt backends.
    QCollator collator(QLocale(QLocale::English, QLocale::UnitedStates));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    struct Entry { Photo photo; QString name, path; };
    QVector<Entry> entries; entries.reserve(result.photos.size());
    for(auto& photo:result.photos){const auto path=photo.displayPath();entries.append({std::move(photo),QFileInfo(path).fileName(),path});}
    std::stable_sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
        const int order = collator.compare(a.name, b.name);
        return order != 0 ? order < 0 : a.path < b.path;
    });
    for(int i=0;i<entries.size();++i)result.photos[i]=std::move(entries[i].photo);
    return result;
}
FolderScanner::FolderScanner(QObject* parent) : QObject(parent) { pool_.setMaxThreadCount(1); }
FolderScanner::~FolderScanner() { shutdown(); }
void FolderScanner::cancel() {
    if (cancel_) cancel_->store(true);
    pool_.clear();
}
void FolderScanner::shutdown() { cancel(); pool_.waitForDone(); }
void FolderScanner::start(const QString& folder, quint64 epoch) {
    cancel();
    cancel_ = std::make_shared<std::atomic_bool>(false);
    auto token = cancel_;
    pool_.start(QRunnable::create([this, folder, epoch, token] {
        auto result = scanFolder(folder, token);
        if (token->load()) return;
        QMetaObject::invokeMethod(this, [this, epoch, token, result = std::move(result)]() mutable {
            if (!token->load()) emit finished(epoch, std::move(result.photos), std::move(result.error));
        }, Qt::QueuedConnection);
    }));
}
}
