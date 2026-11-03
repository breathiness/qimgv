#pragma once

#include <QMap>
#include <QString>
#include <QVersionNumber>

class Actions
{
public:
    Actions();
    static Actions *getInstance();
    const QMap<QString, QVersionNumber> &getMap();
    QList<QString> getList();
    // readable, translated name of an action id; the id itself when there is none
    QString displayName(const QString &action) const;

private:
    void init();
    QMap<QString, QVersionNumber> mActions;

};

extern Actions *appActions;
