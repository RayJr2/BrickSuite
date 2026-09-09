#include "BrickSuiteAuthentication.h"

#include <openssl/crypto.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <memory>

namespace BrickSuiteAuthentication {

QByteArray secureRandom(int byteCount, QString* error)
{
    if (byteCount <= 0 || byteCount > 4096) {
        if (error) *error = QStringLiteral("Invalid secure-random byte count.");
        return {};
    }
    QByteArray bytes(byteCount, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), byteCount) != 1) {
        if (error) *error = QStringLiteral("The cryptographic random generator failed.");
        return {};
    }
    return bytes;
}

QString generateAccessToken(QString* error)
{
    return QString::fromLatin1(secureRandom(32, error).toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray authenticationInput(const QByteArray& challenge,
                               const QByteArray& clientNonce,
                               const QByteArray& sessionId,
                               int protocolMajor, int protocolMinor)
{
    return QByteArrayLiteral("BrickSuite-HMAC-v1\0") + sessionId.toBase64()
        + '\0' + challenge.toBase64() + '\0' + clientNonce.toBase64()
        + '\0' + QByteArray::number(protocolMajor) + '.' + QByteArray::number(protocolMinor);
}

QByteArray hmacSha256(const QByteArray& secret, const QByteArray& message, QString* error)
{
    using MacPtr = std::unique_ptr<EVP_MAC, decltype(&EVP_MAC_free)>;
    using CtxPtr = std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)>;
    MacPtr mac(EVP_MAC_fetch(nullptr, "HMAC", nullptr), EVP_MAC_free);
    CtxPtr context(mac ? EVP_MAC_CTX_new(mac.get()) : nullptr, EVP_MAC_CTX_free);
    char digestName[] = "SHA256";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digestName, 0),
        OSSL_PARAM_construct_end()
    };
    if (!context || EVP_MAC_init(context.get(),
                                 reinterpret_cast<const unsigned char*>(secret.constData()),
                                 static_cast<size_t>(secret.size()), params) != 1
        || EVP_MAC_update(context.get(),
                          reinterpret_cast<const unsigned char*>(message.constData()),
                          static_cast<size_t>(message.size())) != 1) {
        if (error) *error = QStringLiteral("HMAC-SHA-256 initialization failed.");
        return {};
    }
    QByteArray output(EVP_MAX_MD_SIZE, Qt::Uninitialized);
    size_t length = 0;
    if (EVP_MAC_final(context.get(), reinterpret_cast<unsigned char*>(output.data()),
                      &length, static_cast<size_t>(output.size())) != 1) {
        if (error) *error = QStringLiteral("HMAC-SHA-256 calculation failed.");
        return {};
    }
    output.resize(static_cast<qsizetype>(length));
    return output;
}

bool constantTimeEquals(const QByteArray& left, const QByteArray& right)
{
    if (left.size() != right.size())
        return false;
    return left.isEmpty()
        || CRYPTO_memcmp(left.constData(), right.constData(),
                         static_cast<size_t>(left.size())) == 0;
}

} // namespace BrickSuiteAuthentication
