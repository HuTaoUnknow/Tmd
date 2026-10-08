#pragma once
#include <QByteArray>
#include <QString>
namespace LocalCredentials {
bool protect(const QByteArray &input, QByteArray &output, QString *error);
bool unprotect(const QByteArray &input, QByteArray &output, QString *error);
}
