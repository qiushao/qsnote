/*
 * kspell_hunspellclient.cpp
 *
 * SPDX-FileCopyrightText: 2009 Montel Laurent <montel@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "hunspellclient.h"
#include "hunspelldebug.h"
#include "hunspelldict.h"

#include <QDir>
#include <QStandardPaths>
#include <QString>

using namespace Sonnet;

QStringList HunspellClient::s_dictionarySearchPaths;

HunspellClient::HunspellClient(QObject *parent)
    : Client(parent)
{
    qCDebug(SONNET_HUNSPELL) << " HunspellClient::HunspellClient";

    QStringList dirList;
    // search QStandardPaths
    dirList.append(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("hunspell"), QStandardPaths::LocateDirectory));

    auto maybeAddPath = [&dirList](const QString &path) {
        if (QFileInfo::exists(path)) {
            dirList.append(path);

            QDir dir(path);
            for (const QString &subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                dirList.append(dir.absoluteFilePath(subDir));
            }
        }
    };

    // Search custom paths.
    for (const auto &path : s_dictionarySearchPaths) {
        maybeAddPath(path);
    }

#ifndef Q_OS_WIN
    maybeAddPath(QStringLiteral("/System/Library/Spelling"));
    maybeAddPath(QStringLiteral("/usr/share/hunspell/"));
    maybeAddPath(QStringLiteral("/usr/share/myspell/"));
#endif

    for (const QString &dirString : dirList) {
        QDir dir(dirString);
        const auto dicts = dir.entryInfoList({QStringLiteral("*.aff")}, QDir::Files);
        for (const QFileInfo &dict : dicts) {
            m_languagePaths.insert(dict.baseName(), dict.canonicalPath());
        }
    }
}

HunspellClient::~HunspellClient()
{
}

SpellerPlugin *HunspellClient::createSpeller(const QString &language)
{
    qCDebug(SONNET_HUNSPELL) << " SpellerPlugin *HunspellClient::createSpeller(const QString &language) ;" << language;
    HunspellDict *ad = new HunspellDict(language, m_languagePaths.value(language));
    return ad;
}

QStringList HunspellClient::languages() const
{
    return m_languagePaths.keys();
}

void HunspellClient::addDictionaryCustomSearchPaths(const QStringList &p_dirs)
{
    s_dictionarySearchPaths.append(p_dirs);
    s_dictionarySearchPaths.removeDuplicates();
}
