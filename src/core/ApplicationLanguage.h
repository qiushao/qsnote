#ifndef QSNOTE_APPLICATIONLANGUAGE_H
#define QSNOTE_APPLICATIONLANGUAGE_H

#include <QLocale>
#include <QTranslator>

// Keep the translators alive for the lifetime of the application.
class ApplicationLanguage {
public:
    ApplicationLanguage();
    static QString resolve(const QString &preference, const QLocale &systemLocale = QLocale::system());

private:
    QTranslator application_;
    QTranslator editor_;
    QTranslator qt_;
};

#endif // QSNOTE_APPLICATIONLANGUAGE_H
