#pragma once
#include <QObject>
#include <QKeySequence>
#include <QSet>
#include <QVector>
#include <QJsonArray>

namespace photocull {
struct DestinationConfig;
class ShortcutManager : public QObject {
    Q_OBJECT
public:
    explicit ShortcutManager(QWidget* window);
    void bind(QString action, QKeySequence sequence, bool autoRepeat = true);
    void bindChord(QString action, int heldKey, QKeySequence trigger);
    bool validateDestinations(const QVector<QPair<QString,QKeySequence>>& bindings,QString& error) const;
    bool setDestinations(const QVector<QPair<QString,QKeySequence>>& bindings,QString& error);
    QString nextDestinationKey(const DestinationConfig& config) const;
    static QJsonArray defaultEditBindings();
    static QVector<QPair<QString,QString>> editActions();
    bool validateEditBindings(const QJsonArray& bindings,QString& error) const;
    bool setEditBindings(const QJsonArray& bindings,QString& error);
    QJsonArray editBindings() const {return editBindings_;}
    QJsonArray appBindings() const {return appBindings_;}
    bool validateAppBindings(const QJsonArray& bindings,QString& error) const;
    bool setAppBindings(const QJsonArray& bindings,QString& error);
    QVector<QPair<QString,QString>> allShortcuts() const;
signals:
    void triggered(QString action);
protected:
    bool eventFilter(QObject*, QEvent*) override;
private:
    struct Binding { QString action; QKeySequence trigger; int heldKey = 0; bool autoRepeat = true; };
    QWidget* window_;
    QVector<Binding> bindings_;
    QSet<int> held_;
    QJsonArray editBindings_;
    QJsonArray appBindings_;
};
}
