#include "folder_scanner.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace photocull;
namespace {
bool writeFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("fixture") == 7;
}
}

class FolderScannerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<Catalog>(); }

    void recursiveScanIncludesNestedAndHiddenPhotos() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto unicode = QString::fromUtf8("परीक्षण");
        for (const auto& folder : QStringList{"shoot/deep", ".hidden", unicode})
            QVERIFY(QDir(dir.path()).mkpath(folder));
        const QStringList files = {"IMG_10.JPG", "shoot/IMG_2.jPeG", "shoot/deep/IMG_1.PNG",
            unicode + "/IMG_3.NEF", ".hidden/IMG_4.ARW"};
        for (const auto& file : files) QVERIFY(writeFile(dir.filePath(file)));
        QVERIFY(writeFile(dir.filePath("shoot/deep/notes.txt")));
        QVERIFY(writeFile(dir.filePath("shoot/unsupported.gif")));

        const auto result = scanFolder(dir.path());
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.photos.size(), files.size());
        const QStringList expected = {"IMG_1.PNG", "IMG_2.jPeG", "IMG_3.NEF", "IMG_4.ARW", "IMG_10.JPG"};
        QSet<QString> paths;
        for (int i = 0; i < result.photos.size(); ++i) {
            QCOMPARE(result.photos[i].name(), expected[i]);
            paths.insert(result.photos[i].displayPath());
        }
        for (const auto& file : files) QVERIFY(paths.contains(dir.filePath(file)));
    }

    void pairsStayWithinDirectoriesAndIdsStayStable() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QStringList folders = {".", "first", "second", "first/deep", QString::fromUtf8("परीक्षण")};
        for (const auto& folder : folders) {
            const QDir containing(dir.filePath(folder));
            QVERIFY(QDir().mkpath(containing.path()));
            for (const QString name : {"IMG_2.JPG", "IMG_2.NEF", "IMG_2.png"})
                QVERIFY(writeFile(containing.filePath(name)));
        }
        const auto result = scanFolder(dir.path());
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.photos.size(), folders.size() * 2);
        QSet<QString> ids;
        for (const auto& photo : result.photos) {
            QVERIFY(!ids.contains(photo.id)); ids.insert(photo.id);
            const QFileInfo displayed(photo.displayPath());
            const bool png = displayed.suffix() == "png";
            QCOMPARE(photo.sources.size(), png ? 1 : 2);
            if (!png) QCOMPARE(photo.typeLabel(), "NEF+JPG");
            for (const auto& source : photo.sources)
                QCOMPARE(QFileInfo(source).absolutePath(), displayed.absolutePath());

            // Retain the existing ID formula for files opened directly in their folder.
            QString identity = displayed.absolutePath() + (png ? "/png:img_2" : "/photo:img_2");
#ifdef Q_OS_WIN
            identity = identity.toCaseFolded();
#endif
            QCOMPARE(photo.id, QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex()));
            const auto direct = scanFolder(displayed.absolutePath());
            bool found = false;
            for (const auto& candidate : direct.photos) {
                if (candidate.displayPath() != photo.displayPath()) continue;
                QCOMPARE(candidate.id, photo.id); QCOMPARE(candidate.sources, photo.sources); found = true;
            }
            QVERIFY(found);
        }
        const auto repeated = scanFolder(dir.path());
        QCOMPARE(repeated.photos.size(), result.photos.size());
        for (int i = 0; i < result.photos.size(); ++i) {
            QCOMPARE(repeated.photos[i].id, result.photos[i].id);
            QCOMPARE(repeated.photos[i].sources, result.photos[i].sources);
        }
    }

    void emptyParentFindsDeepPhotosWithoutPairingAcrossFolders() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath("raw/deep"));
        QVERIFY(QDir(dir.path()).mkpath("jpeg/deep"));
        QVERIFY(writeFile(dir.filePath("raw/deep/IMG_1.CR3")));
        QVERIFY(writeFile(dir.filePath("jpeg/deep/IMG_1.JPG")));
        const auto result = scanFolder(dir.path());
        QVERIFY(result.error.isEmpty()); QCOMPARE(result.photos.size(), 2);
        for (const auto& photo : result.photos) QCOMPARE(photo.sources.size(), 1);
        QVERIFY(result.photos[0].id != result.photos[1].id);
        auto cancel = std::make_shared<std::atomic_bool>(true);
        const auto cancelled = scanFolder(dir.path(), cancel);
        QVERIFY(cancelled.photos.isEmpty()); QVERIFY(cancelled.error.isEmpty());
    }

    void asynchronousNestedScanCanBeReplaced() {
        QTemporaryDir a, b; QVERIFY(a.isValid()); QVERIFY(b.isValid());
        QVERIFY(QDir(a.path()).mkpath("nested/deep"));
        QVERIFY(QDir(b.path()).mkpath("nested/deep"));
        for (int i = 0; i < 100; ++i)
            QVERIFY(writeFile(a.filePath(QString("nested/deep/%1.jpg").arg(i))));
        const auto only = b.filePath("nested/deep/only.png"); QVERIFY(writeFile(only));
        FolderScanner scanner; QSignalSpy spy(&scanner, &FolderScanner::finished);
        scanner.start(a.path(), 1); scanner.start(b.path(), 2);
        QTRY_COMPARE(spy.size(), 1); QCOMPARE(spy[0][0].toULongLong(), 2);
        const auto photos = qvariant_cast<Catalog>(spy[0][1]);
        QCOMPARE(photos.size(), 1); QCOMPARE(photos[0].displayPath(), only);
        QVERIFY(spy[0][2].toString().isEmpty());
        scanner.start(a.path(), 3); scanner.shutdown();
        QCoreApplication::processEvents(); QCOMPARE(spy.size(), 1);
    }
};
QTEST_GUILESS_MAIN(FolderScannerTest)
#include "test_folder_scanner.moc"
