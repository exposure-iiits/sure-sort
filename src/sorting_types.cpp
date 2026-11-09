#include "sorting_types.h"
#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QJsonDocument>

namespace photocull {
QString assignmentKey(const QString& photoId,const QString& categoryId){return QString::fromUtf8(QJsonDocument(QJsonArray{photoId,categoryId}).toJson(QJsonDocument::Compact));}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString validateFolderName(const QString& name) {
    if (name.isEmpty() || name != name.trimmed() || name.endsWith('.') || name == "." || name == "..")
        return "Use a non-empty folder name without trailing dots or spaces.";
    static const QRegularExpression invalid("[\\x00-\\x1f<>:\"/\\\\|?*]");
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)", QRegularExpression::CaseInsensitiveOption);
    if (name.size() > 120 || name.contains(invalid) || reserved.match(name).hasMatch())
        return "This name is not a valid Windows subfolder name.";
    return {};
}
QString validateDestinationConfig(const DestinationConfig& config) {
    if (config.outputRoot.isEmpty() || !QDir::isAbsolutePath(config.outputRoot)) return "Select an absolute output directory.";
    QSet<QString> names, ids, keys;
    for (const auto& folder : config.folders) {
        if(folder.action!="copy"&&folder.action!="move")return "Choose Copy or Move for each folder.";
        auto error = validateFolderName(folder.name);
        if (!error.isEmpty()) return error;
        if (folder.id.isEmpty() || ids.contains(folder.id)) return "Each folder needs a unique ID.";
        ids.insert(folder.id);
        const auto name = folder.name.normalized(QString::NormalizationForm_C).toCaseFolded();
        if (names.contains(name)) return "Folder names must be unique.";
        names.insert(name);
        for(const auto& shortcut:QStringList{folder.shortcut,folder.alternateShortcut}){
        if (shortcut.isEmpty()) continue;
        const QKeySequence sequence(shortcut, QKeySequence::PortableText);
        if (sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown || sequence[0].key() == 0)
            return "Use a single key or a modifier plus key for each shortcut.";
        const auto key = sequence.toString(QKeySequence::PortableText);
        if (keys.contains(key)) return "Two folders cannot use the same shortcut.";
        keys.insert(key);
        }
    }
    return {};
}
}
