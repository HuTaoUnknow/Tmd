#include "core/platform/LocalCredentials.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif
namespace {
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
bool transformToken(const QByteArray &input, QByteArray &output, bool encrypt, QString *error) {
    if (input.isEmpty()) { output.clear(); return true; }
#ifdef Q_OS_WIN
    DATA_BLOB source{DWORD(input.size()), reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()))}, result{};
    const BOOL success = encrypt ? CryptProtectData(&source, L"Tmd image upload", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result)
        : CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result);
    if (!success) return fail(error, QStringLiteral("无法读取或保护本机图床凭据，请重新填写。"));
    output = QByteArray(reinterpret_cast<const char *>(result.pbData), result.cbData); LocalFree(result.pbData); return true;
#else
    Q_UNUSED(output); Q_UNUSED(encrypt);
    return fail(error, QStringLiteral("当前平台不支持保存受保护的图床凭据。"));
#endif
}
}
bool LocalCredentials::protect(const QByteArray &input, QByteArray &output, QString *error) { return transformToken(input, output, true, error); }
bool LocalCredentials::unprotect(const QByteArray &input, QByteArray &output, QString *error) { return transformToken(input, output, false, error); }
