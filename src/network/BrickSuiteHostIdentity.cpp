#include "BrickSuiteHostIdentity.h"

#include "BrickSuiteAuthentication.h"
#include "../services/CredentialStore.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>

namespace {

QString identityCredentialName()
{
#if defined(BRICKSUITE_TESTING)
    return QStringLiteral("BrickSuiteHostTlsIdentity.Test.%1")
        .arg(QCoreApplication::applicationName());
#else
    return QStringLiteral("BrickSuiteHostTlsIdentity");
#endif
}

template<typename T, void (*Free)(T*)>
using OpenSslPtr = std::unique_ptr<T, decltype(Free)>;

QByteArray bioBytes(BIO* bio)
{
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return size > 0 ? QByteArray(data, static_cast<qsizetype>(size)) : QByteArray();
}

bool addExtension(X509* certificate, int nid, const char* value)
{
    X509V3_CTX context{};
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);
    OpenSslPtr<X509_EXTENSION, X509_EXTENSION_free> extension(
        X509V3_EXT_conf_nid(nullptr, &context, nid, const_cast<char*>(value)),
        X509_EXTENSION_free);
    return extension && X509_add_ext(certificate, extension.get(), -1) == 1;
}

BrickSuiteHostIdentity::Result load(const QByteArray& certificatePem,
                                    const QByteArray& privateKeyPem)
{
    BrickSuiteHostIdentity::Result result;
    const auto certificates = QSslCertificate::fromData(certificatePem, QSsl::Pem);
    if (certificates.size() != 1) {
        result.error = QStringLiteral("The stored Host certificate is invalid.");
        return result;
    }
    const QSslKey key(privateKeyPem, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    if (key.isNull()) {
        result.error = QStringLiteral("The stored Host private key is invalid.");
        return result;
    }
    result.success = true;
    result.certificate = certificates.first();
    result.privateKey = key;
    result.fingerprint = BrickSuiteHostIdentity::fingerprint(result.certificate);
    return result;
}

} // namespace

QString BrickSuiteHostIdentity::certificatePath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(root).filePath(QStringLiteral("server/host-certificate.pem"));
}

BrickSuiteHostIdentity::Result BrickSuiteHostIdentity::loadOrCreate()
{
    const auto identity = CredentialStore::read(identityCredentialName());
    if (identity.success && identity.found) {
        const QByteArray pem = identity.value.toLatin1();
        return load(pem, pem);
    }
    if (!identity.success) {
        Result result;
        result.error = QStringLiteral("Unable to load the Host TLS identity securely: %1")
                           .arg(identity.error);
        return result;
    }
    return createAndPersist();
}

BrickSuiteHostIdentity::Result BrickSuiteHostIdentity::regenerate()
{
    return createAndPersist();
}

BrickSuiteHostIdentity::Result BrickSuiteHostIdentity::generateEphemeral()
{
    return generate(false);
}

BrickSuiteHostIdentity::Result BrickSuiteHostIdentity::createAndPersist()
{
    return generate(true);
}

BrickSuiteHostIdentity::Result BrickSuiteHostIdentity::generate(bool persist)
{
    Result result;
    using KeyCtxPtr = OpenSslPtr<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;
    using KeyPtr = OpenSslPtr<EVP_PKEY, EVP_PKEY_free>;
    using CertPtr = OpenSslPtr<X509, X509_free>;
    using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

    KeyCtxPtr context(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* generated = nullptr;
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1
        || EVP_PKEY_CTX_set_group_name(context.get(), "prime256v1") != 1
        || EVP_PKEY_generate(context.get(), &generated) != 1) {
        result.error = QStringLiteral("Unable to generate the Host TLS private key.");
        return result;
    }
    KeyPtr key(generated, EVP_PKEY_free);
    CertPtr certificate(X509_new(), X509_free);
    if (!certificate || X509_set_version(certificate.get(), 2) != 1) {
        result.error = QStringLiteral("Unable to create the Host TLS certificate.");
        return result;
    }
    QString randomError;
    const QByteArray serial = BrickSuiteAuthentication::secureRandom(16, &randomError);
    OpenSslPtr<BIGNUM, BN_free> serialNumber(BN_bin2bn(
        reinterpret_cast<const unsigned char*>(serial.constData()), serial.size(), nullptr), BN_free);
    OpenSslPtr<ASN1_INTEGER, ASN1_INTEGER_free> asnSerial(
        serialNumber ? BN_to_ASN1_INTEGER(serialNumber.get(), nullptr) : nullptr, ASN1_INTEGER_free);
    if (!asnSerial || X509_set_serialNumber(certificate.get(), asnSerial.get()) != 1
        || !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -300)
        || !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 10L * 365L * 24L * 60L * 60L)
        || X509_set_pubkey(certificate.get(), key.get()) != 1) {
        result.error = QStringLiteral("Unable to initialize the Host TLS certificate.");
        return result;
    }
    X509_NAME* name = X509_get_subject_name(certificate.get());
    const QByteArray commonName = QByteArrayLiteral("BrickSuite Personal Host");
    const QByteArray organization = QByteArrayLiteral("RF StateSide, LLC");
    if (!name
        || X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(commonName.constData()),
                                      commonName.size(), -1, 0) != 1
        || X509_NAME_add_entry_by_NID(name, NID_organizationName, MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(organization.constData()),
                                      organization.size(), -1, 0) != 1
        || X509_set_issuer_name(certificate.get(), name) != 1
        || !addExtension(certificate.get(), NID_basic_constraints, "critical,CA:FALSE")
        || !addExtension(certificate.get(), NID_key_usage, "critical,digitalSignature,keyAgreement")
        || !addExtension(certificate.get(), NID_ext_key_usage, "serverAuth")
        || !addExtension(certificate.get(), NID_subject_key_identifier, "hash")
        || X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
        result.error = QStringLiteral("Unable to sign the Host TLS certificate.");
        return result;
    }
    BioPtr keyBio(BIO_new(BIO_s_mem()), BIO_free);
    BioPtr certBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!keyBio || !certBio
        || PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1
        || PEM_write_bio_X509(certBio.get(), certificate.get()) != 1) {
        result.error = QStringLiteral("Unable to encode the Host TLS identity.");
        return result;
    }
    const QByteArray keyPem = bioBytes(keyBio.get());
    const QByteArray certificatePem = bioBytes(certBio.get());
    if (persist) {
        QString credentialError;
        const QByteArray identityPem = certificatePem + keyPem;
        if (!CredentialStore::write(identityCredentialName(),
                                    QString::fromLatin1(identityPem), &credentialError)) {
            result.error = QStringLiteral("Unable to store the Host TLS identity securely: %1")
                               .arg(credentialError);
            return result;
        }
        QDir directory;
        if (!directory.mkpath(QFileInfo(certificatePath()).absolutePath())) {
            qWarning() << "Unable to create the public Host certificate directory.";
            return load(identityPem, identityPem);
        }
        QSaveFile output(certificatePath());
        if (!output.open(QIODevice::WriteOnly) || output.write(certificatePem) != certificatePem.size()
            || !output.commit()) {
            qWarning() << "Unable to store the public Host certificate copy.";
        }
        return load(identityPem, identityPem);
    }
    return load(certificatePem, keyPem);
}

QString BrickSuiteHostIdentity::fingerprint(const QSslCertificate& certificate)
{
    const QByteArray digest = QCryptographicHash::hash(certificate.toDer(),
                                                       QCryptographicHash::Sha256).toHex().toUpper();
    QStringList groups;
    for (qsizetype index = 0; index < digest.size(); index += 2)
        groups.append(QString::fromLatin1(digest.mid(index, 2)));
    return groups.join(QLatin1Char(':'));
}

QString BrickSuiteHostIdentity::normalizedFingerprint(const QString& value)
{
    QString normalized;
    for (const QChar character : value) {
        if (character.isDigit() || (character.toUpper() >= QLatin1Char('A')
                                    && character.toUpper() <= QLatin1Char('F')))
            normalized.append(character.toUpper());
    }
    return normalized.size() == 64 ? normalized : QString();
}
