/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#ifndef QMLJSLOCATORDATA_H
#define QMLJSLOCATORDATA_H

#include <qmljs/qmljsdocument.h>

#include <QObject>
#include <QHash>
#include <QMutex>

namespace QmlJSTools {
namespace Internal {

class LocatorData : public QObject
{
    Q_OBJECT
public:
    explicit LocatorData(QObject *parent = 0);
    ~LocatorData();

    enum EntryType
    {
        Function
    };

    class Entry
    {
    public:
        EntryType type;
        QString symbolName;
        QString displayName;
        QString extraInfo;
        QString fileName;
        int line;
        int column;
    };

    QHash<QString, QList<Entry> > entries() const;

private slots:
    void onDocumentUpdated(const QmlJS::Document::Ptr &doc);
    void onAboutToRemoveFiles(const QStringList &files);

private:
    mutable QMutex m_mutex;
    QHash<QString, QList<Entry> > m_entries;
};

} // namespace Internal
} // namespace QmlJSTools

#endif // QMLJSLOCATORDATA_H