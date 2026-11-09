#include "processing_pipeline.h"
#include "exif_metadata.h"
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QImageReader>
#include <libraw/libraw.h>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

namespace photocull {
namespace {
bool cancelled(const CancelToken& c) { return c && c->load(); }
bool renamedIdentityMatches(const QByteArray& actual,const QByteArray& expected){
    if(actual==expected)return true;
#ifdef Q_OS_WIN
    // NTFS filename tunneling can restore a deleted filename's creation time
    // during rename. File ID, last-write, length AND SHA-256 must still match.
    const auto a=actual.split(':'),b=expected.split(':');if(a.size()!=9||b.size()!=9)return false;
    for(int i:{0,1,2,5,6,7,8})if(a[i]!=b[i])return false;
    return true;
#else
    return false;
#endif
}
int progress(void* p, LibRaw_progress, int, int) { return static_cast<std::atomic_bool*>(p)->load() ? 1 : 0; }
struct RawDelete { void operator()(LibRaw* p) const { p->recycle(); delete p; } };
using RawOwner = std::unique_ptr<LibRaw,RawDelete>;
using MemoryOwner = std::unique_ptr<libraw_processed_image_t,decltype(&LibRaw::dcraw_clear_mem)>;
QImage developOriginal(const QString& path, const CancelToken& cancel, QString& error,ExifMetadata* metadata) {
    RawOwner raw(new LibRaw);
    raw->imgdata.rawparams.max_raw_memory_mb = 1024;
    raw->imgdata.params.half_size = 0;
    raw->imgdata.params.output_bps = 8;
    raw->imgdata.params.output_color = 1;
    raw->imgdata.params.gamm[0] = 1.0/2.4; // Actual sRGB transfer curve, not LibRaw's BT.709 default.
    raw->imgdata.params.gamm[1] = 12.92;
    raw->imgdata.params.auto_bright_thr = 0.001f; // Reduce auto-bright highlight clipping, no extra processing pass.
    raw->imgdata.params.use_camera_matrix = 1;
    raw->imgdata.params.use_camera_wb = 1; // LibRaw falls back when camera WB is unavailable.
    raw->imgdata.params.user_qual = 3; // AHD; LibRaw selects its appropriate X-Trans path.
    if (cancel) raw->set_progress_handler(progress,cancel.get());
#ifdef Q_OS_WIN
    int rc = raw->open_file(reinterpret_cast<const wchar_t*>(path.utf16()));
#else
    const auto encoded = QFile::encodeName(path);
    int rc = raw->open_file(encoded.constData());
#endif
    if(rc==LIBRAW_SUCCESS&&metadata){const auto& data=raw->imgdata;
        metadata->camera(data.idata.make,data.idata.model,data.other.iso_speed,data.other.shutter,data.other.aperture,data.other.focal_len,data.lens.Lens);
    }
    if (rc == LIBRAW_SUCCESS && !cancelled(cancel)) rc = raw->unpack();
    if (rc == LIBRAW_SUCCESS && !cancelled(cancel)) rc = raw->dcraw_process();
    if (cancelled(cancel)) return {};
    if (rc != LIBRAW_SUCCESS) { error = "RAW development failed: " + QString::fromLatin1(libraw_strerror(rc)); return {}; }
    MemoryOwner memory(raw->dcraw_make_mem_image(&rc), &LibRaw::dcraw_clear_mem);
    if (!memory || rc != LIBRAW_SUCCESS || memory->type != LIBRAW_IMAGE_BITMAP || memory->bits != 8 ||
        (memory->colors != 3 && memory->colors != 1) || !memory->width || !memory->height ||
        quint64(memory->width)*memory->height*memory->colors > memory->data_size) {
        error = "RAW development returned an unsupported image layout."; return {};
    }
    const auto format = memory->colors == 3 ? QImage::Format_RGB888 : QImage::Format_Grayscale8;
    // dcraw_make_mem_image owns a separate allocation. Transfer it to QImage
    // and release sensor/demosaicing buffers before editing, avoiding a full copy.
    auto* pixels=memory.get();
    raw.reset();
    QImage image(pixels->data,pixels->width,pixels->height,qsizetype(pixels->width)*pixels->colors,format,
                 [](void* p){LibRaw::dcraw_clear_mem(static_cast<libraw_processed_image_t*>(p));},pixels);
    if (image.isNull()) { error = "Not enough memory to develop this RAW."; return {}; }
    memory.release(); // QImage accepted ownership; allocation failure above still has RAII cleanup.
    image.setColorSpace(QColorSpace::SRgb); // dcraw_make_mem_image has already applied orientation.
    return image;
}
QString chooseOutput(const QString& directory, const QString& filename) {
    const QFileInfo info(filename);
    for (int suffix=1;suffix<1000000;++suffix) {
        const auto name = suffix==1 ? filename : info.completeBaseName()+"_"+QString::number(suffix)+'.'+info.suffix();
        const auto path = QDir(directory).filePath(name);
        if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) return path;
    }
    return {};
}
#ifdef Q_OS_WIN
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
bool inspect(HANDLE h, QByteArray& digest, QByteArray& identity, QString& error) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        error = "File identity cannot be verified."; return false;
    }
    identity = QByteArray::number(info.dwVolumeSerialNumber)+':'+QByteArray::number(info.nFileIndexHigh)+':'+
        QByteArray::number(info.nFileIndexLow)+':'+QByteArray::number(info.ftCreationTime.dwHighDateTime)+':'+
        QByteArray::number(info.ftCreationTime.dwLowDateTime)+':'+QByteArray::number(info.ftLastWriteTime.dwHighDateTime)+':'+
        QByteArray::number(info.ftLastWriteTime.dwLowDateTime)+':'+QByteArray::number(info.nFileSizeHigh)+':'+QByteArray::number(info.nFileSizeLow);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray block(1024*1024,Qt::Uninitialized);
    DWORD read = 0;
    do {
        if (!ReadFile(h,block.data(),DWORD(block.size()),&read,nullptr)) { error = "Cannot verify output contents."; return false; }
        if (read) hash.addData(QByteArrayView(block.constData(),read));
    } while (read);
    digest = hash.result(); return true;
}
#endif
bool noReplaceRename(const QString& from, const QString& to, QString& error) {
#ifdef Q_OS_WIN
    if (MoveFileExW(reinterpret_cast<const wchar_t*>(from.utf16()),reinterpret_cast<const wchar_t*>(to.utf16()),MOVEFILE_WRITE_THROUGH)) return true;
#else
    const auto source = QFile::encodeName(from), destination = QFile::encodeName(to);
    if (::link(source.constData(),destination.constData())==0) { ::unlink(source.constData()); return true; }
#endif
    error = "Could not finalize output. The name may be occupied or the drive is unavailable; retry to choose a new name.";
    return false;
}
}
bool ProcessingPipeline::fingerprint(const QString& path, QByteArray& digest, QByteArray& identity, QString& error) {
#ifdef Q_OS_WIN
    Handle h{CreateFileW(reinterpret_cast<const wchar_t*>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};
    if (h.value==INVALID_HANDLE_VALUE) { error = "Cannot lock output for verification; it may be missing or in use."; return false; }
    return inspect(h.value,digest,identity,error);
#else
    const auto encoded = QFile::encodeName(path);
    int fd = ::open(encoded.constData(),O_RDONLY | O_NOFOLLOW);
    if (fd<0) { error = "Cannot read output identity."; return false; }
    struct stat st{};
    if (::fstat(fd,&st)!=0 || !S_ISREG(st.st_mode)) { ::close(fd); error="Not a regular output file."; return false; }
    QFile f;
    if (!f.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) { ::close(fd); error="Cannot read output."; return false; }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) { error="Cannot hash output."; return false; }
    digest=hash.result(); identity=QByteArray::number(qulonglong(st.st_dev))+':'+QByteArray::number(qulonglong(st.st_ino))+':'+QByteArray::number(qlonglong(st.st_size))+':'+QByteArray::number(qlonglong(st.st_mtime)); return true;
#endif
}
bool ProcessingPipeline::removeOwned(const QString& path, const QByteArray& digest, const QByteArray& identity, QString& error) {
    if (path.isEmpty() || !QFileInfo::exists(path)) return true;
    if (digest.isEmpty() || identity.isEmpty()) { error = "Unverified file preserved: " + path; return false; }
#ifdef Q_OS_WIN
    // Deny concurrent writers and deletion, hash and delete the SAME handle (no path race).
    Handle h{CreateFileW(reinterpret_cast<const wchar_t*>(path.utf16()),GENERIC_READ|DELETE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};
    if (h.value==INVALID_HANDLE_VALUE) { error="Output is in use; preserved: "+path; return false; }
    QByteArray actualHash,actualIdentity;
    if (!inspect(h.value,actualHash,actualIdentity,error)) return false;
    if (actualHash!=digest || actualIdentity!=identity) { error="User-modified or replaced output preserved: "+path; return false; }
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(h.value,FileDispositionInfo,&disposition,sizeof(disposition))) { error="Could not remove owned output; preserved: "+path; return false; }
    return true;
#else
    // POSIX advisory locks cannot guarantee protection against another writer.
    // Preserve rather than risk deleting a modified file on this unverified platform.
    error="Automatic output removal requires Windows file locking; file preserved: "+path;
    return false;
#endif
}
bool ProcessingPipeline::removeWithBackup(const QString& source,const QByteArray& sourceDigest,const QByteArray& sourceIdentity,
                                         const QString& backup,const QByteArray& backupDigest,const QByteArray& backupIdentity,QString& error){
#ifdef Q_OS_WIN
    Handle guard{CreateFileW(reinterpret_cast<const wchar_t*>(backup.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)};
    if(guard.value==INVALID_HANDLE_VALUE){error="Cannot lock the backup; original preserved: "+source;return false;}
    QByteArray hash,identity;if(!inspect(guard.value,hash,identity,error)||hash!=backupDigest||identity!=backupIdentity){error="Backup changed; original preserved: "+source;return false;}
    return removeOwned(source,sourceDigest,sourceIdentity,error);
#else
    Q_UNUSED(sourceDigest);Q_UNUSED(sourceIdentity);Q_UNUSED(backup);Q_UNUSED(backupDigest);Q_UNUSED(backupIdentity);
    error="Verified original relocation requires Windows file locking: "+source;return false;
#endif
}
ProcessingTask ProcessingPipeline::prepare(ProcessingTask task, const CancelToken& cancel) {
    task.status="failed"; task.error.clear();
    try {
        if (cancelled(cancel)) { task.status="cancelled"; return task; }
        const auto editError=validateEdits(task.edits);if(!editError.isEmpty()){task.error=editError;return task;}
        const auto outputError=validateOutput(task.output);if(!outputError.isEmpty()){task.error=outputError;return task;}
        const auto watermarkError=validateWatermark(task.watermark);
        if(!watermarkError.isEmpty()){task.error=watermarkError;return task;}
        const QFileInfo directory(task.directory);
        if (directory.isSymLink()
#ifdef Q_OS_WIN
            || directory.isJunction()
#endif
            || !QDir().mkpath(task.directory)) { task.error="Cannot create destination folder (linked subfolders are not allowed)."; return task; }
        const auto sources=task.readSources.isEmpty()?task.photo.sources:task.readSources;
        QString rawSource,logicalSource;
        for(int i=0;i<sources.size();++i)if(isRawExtension(QFileInfo(sources[i]).suffix())){rawSource=sources[i];logicalSource=task.photo.sources.value(i,sources[i]);break;}
        const auto source=rawSource.isEmpty()?Photo{task.photo.id,sources}.displayPath():rawSource;
        if(logicalSource.isEmpty()){const int i=sources.indexOf(source);logicalSource=task.photo.sources.value(i,source);}
        const QString filename=rawSource.isEmpty()?QFileInfo(logicalSource).fileName():QFileInfo(logicalSource).completeBaseName()+".jpg";
        task.outputPath=chooseOutput(task.directory,filename);
        if (task.outputPath.isEmpty()) { task.error="No free output filename could be chosen."; return task; }
        QFile output(task.temporaryPath);
        if (!output.open(QIODevice::WriteOnly|QIODevice::NewOnly)) { task.error="Cannot create temporary output: "+output.errorString(); return task; }
        bool success=false;
        const bool png=rawSource.isEmpty()&&QFileInfo(source).suffix().compare("png",Qt::CaseInsensitive)==0;
        if (rawSource.isEmpty() && !task.watermark.settings.enabled && task.edits.isIdentity() &&
            task.output.resolution==100 && task.output.preserveMetadata && (png||task.output.jpegQuality==95)) {
            QFile input(source);
            if (!input.open(QIODevice::ReadOnly)) task.error="Cannot read original: "+input.errorString();
            else {
                success=true;
                const qint64 expected=input.size(); qint64 copied=0;
                while (!input.atEnd() && !cancelled(cancel)) {
                    const auto block=input.read(1024*1024);
                    if ((block.isEmpty() && input.error()!=QFileDevice::NoError) || output.write(block)!=block.size()) {
                        success=false; task.error="Could not copy original photograph."; break;
                    }
                    copied+=block.size();
                }
                if (!cancelled(cancel) && (copied!=expected || input.size()!=expected || input.error()!=QFileDevice::NoError)) {
                    success=false; task.error="Original changed or could not be read completely; no final output saved.";
                }
            }
        } else {
            auto metadata=task.output.preserveMetadata?ExifMetadata::read(source):ExifMetadata();
            QImage image;
            if(!rawSource.isEmpty())image=developOriginal(rawSource,cancel,task.error,task.output.preserveMetadata?&metadata:nullptr);
            else {
                QImageReader reader(source);reader.setAutoTransform(true);image=reader.read();
                if(image.isNull())task.error="Cannot decode original for processing: "+reader.errorString();
            }
            if (!image.isNull() && !cancelled(cancel)) {
                if(image.colorSpace().isValid()&&image.colorSpace()!=QColorSpace(QColorSpace::SRgb))image.convertToColorSpace(QColorSpace::SRgb);
                else if(!image.colorSpace().isValid())image.setColorSpace(QColorSpace::SRgb);
                image.setDevicePixelRatio(1);
                if(!task.output.preserveMetadata)for(const auto& key:image.textKeys())image.setText(key,QString());
                image=renderEdits(std::move(image),task.edits,cancel);
                if(image.isNull()){task.error="Editing failed or processing was cancelled.";}
                else if(compositeWatermark(image,task.watermark,task.error)){
                    if(task.output.resolution!=100)image=image.scaled(outputSize(image.size(),task.output),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
                    const QByteArray format=png?QByteArray("png"):QByteArray("jpeg");
                    MetadataDevice device(output,format,task.output.preserveMetadata?metadata.serialize(image.size()):QByteArray());
                    QImageWriter writer(&device,format);if(format=="jpeg"){writer.setQuality(task.output.jpegQuality);writer.setOptimizedWrite(true);}
                    success=writer.write(image);
                    if (!success) task.error="Image encoding failed: "+writer.errorString();
                }
            }
        }
        if (success && !output.flush()) { success=false; task.error="Could not flush temporary output."; }
        if (success) {
#ifdef Q_OS_WIN
            if (!FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(output.handle())))) { success=false; task.error="Could not sync temporary output to disk."; }
#else
            if (::fsync(output.handle())!=0) { success=false; task.error="Could not sync temporary output to disk."; }
#endif
        }
        output.close();
        QString fingerprintError;
        const bool verified=fingerprint(task.temporaryPath,task.digest,task.identity,fingerprintError);
        if (cancelled(cancel)) task.status="cancelled";
        else if (success && verified) {
            task.status="prepared";
#ifdef Q_OS_WIN
            if(!task.predecessorPath.isEmpty()){
                QByteArray digest,identity;QString error;
                if(fingerprint(task.predecessorPath,digest,identity,error)&&digest==task.predecessorDigest&&identity==task.predecessorIdentity)task.outputPath=task.predecessorPath;
                else task.error="Previous output changed or became unavailable; preserved. Saving a new output safely.";
            }
#endif
        }
        else if (task.error.isEmpty()) task.error=fingerprintError.isEmpty()?"Processing did not produce an image.":fingerprintError;
        if (task.status!="prepared" && verified) { QString ignored; removeOwned(task.temporaryPath,task.digest,task.identity,ignored); }
        return task;
    } catch (const std::bad_alloc&) { task.error="Not enough memory to process this photograph."; }
      catch (...) { task.error="Unexpected processing failure."; }
    return task;
}
ProcessingTask ProcessingPipeline::publish(ProcessingTask task, const CancelToken& cancel) {
    if (cancelled(cancel)) { task.status="cancelled"; return task; }
    const auto warning=!task.predecessorPath.isEmpty()&&task.outputPath!=task.predecessorPath?task.error:QString();
    QByteArray digest,identity;
    if (!fingerprint(task.temporaryPath,digest,identity,task.error) || digest!=task.digest || identity!=task.identity) {
        task.status="failed"; task.error="Temporary output changed or became unavailable; preserved."; return task;
    }
    if(task.outputPath==task.predecessorPath&&!task.predecessorPath.isEmpty()&&
       !removeOwned(task.predecessorPath,task.predecessorDigest,task.predecessorIdentity,task.error)){task.status="failed";return task;}
    if (!noReplaceRename(task.temporaryPath,task.outputPath,task.error)) { task.status="failed"; return task; }
    QByteArray publishedDigest,publishedIdentity;
    if(!fingerprint(task.outputPath,publishedDigest,publishedIdentity,task.error)||publishedDigest!=task.digest||!renamedIdentityMatches(publishedIdentity,task.identity)){
        task.status="failed";task.error="Published output changed or could not be verified; preserved.";return task;
    }
    task.identity=publishedIdentity;
    task.status="completed"; task.error=warning; return task;
}
ProcessingTask ProcessingPipeline::recover(ProcessingTask task, const CancelToken& cancel) {
    if (QFileInfo::exists(task.outputPath)) {
        QByteArray digest,identity;
        if (fingerprint(task.outputPath,digest,identity,task.error) && digest==task.digest && renamedIdentityMatches(identity,task.identity)) {
            task.identity=identity;task.status="completed";
            if(task.predecessorPath.isEmpty()||task.outputPath==task.predecessorPath)task.error.clear();
            return task;
        }
        if(task.outputPath==task.predecessorPath&&!task.predecessorPath.isEmpty())return publish(std::move(task),cancel);
        task.status="failed"; task.error="Recovery found an occupied or modified output; preserved. Retry to use another filename."; return task;
    }
    return publish(std::move(task),cancel);
}
ProcessingTask ProcessingPipeline::cleanup(ProcessingTask task) {
    QStringList warnings;
    for (const auto& path : {task.outputPath,task.temporaryPath}) {
        QString error;
        if (!removeOwned(path,task.digest,task.identity,error)) warnings.append(error);
    }
    if(!task.predecessorPath.isEmpty()){
        QString error;if(!removeOwned(task.predecessorPath,task.predecessorDigest,task.predecessorIdentity,error))warnings.append(error);
    }
    task.status=warnings.isEmpty()?"retired-clean":"preserved"; task.error=warnings.join('\n'); return task;
}
}
