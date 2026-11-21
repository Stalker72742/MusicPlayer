//
// Created by Stalker7274 on 19.11.2026.
//

#include "SyncCrypto.h"

#include <QFile>
#include <QMessageAuthenticationCode>

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstring>

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif

#ifndef BCRYPT_KDF_RAW_SECRET
#define BCRYPT_KDF_RAW_SECRET L"TRUNCATE"
#endif

namespace
{
    BCRYPT_ALG_HANDLE OpenAlgorithm(LPCWSTR id)
    {
        BCRYPT_ALG_HANDLE handle = nullptr;
        if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&handle, id, nullptr, 0)))
        {
            return nullptr;
        }
        return handle;
    }

    struct Providers
    {
        BCRYPT_ALG_HANDLE sha256 = nullptr;
        BCRYPT_ALG_HANDLE ecdh = nullptr;
        BCRYPT_ALG_HANDLE aesGcm = nullptr;

        Providers()
        {
            sha256 = OpenAlgorithm(BCRYPT_SHA256_ALGORITHM);
            ecdh = OpenAlgorithm(BCRYPT_ECDH_P256_ALGORITHM);
            aesGcm = OpenAlgorithm(BCRYPT_AES_ALGORITHM);
            if (aesGcm)
            {
                const wchar_t mode[] = BCRYPT_CHAIN_MODE_GCM;
                if (!NT_SUCCESS(BCryptSetProperty(aesGcm, BCRYPT_CHAINING_MODE,
                                                  reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                                                  sizeof(mode), 0)))
                {
                    BCryptCloseAlgorithmProvider(aesGcm, 0);
                    aesGcm = nullptr;
                }
            }
        }

        ~Providers()
        {
            for (BCRYPT_ALG_HANDLE handle : {sha256, ecdh, aesGcm})
            {
                if (handle)
                {
                    BCryptCloseAlgorithmProvider(handle, 0);
                }
            }
        }
    };

    const Providers& GetProviders()
    {
        static const Providers providers;
        return providers;
    }

    PUCHAR Bytes(const char* data)
    {
        return reinterpret_cast<PUCHAR>(const_cast<char*>(data));
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO MakeAuthInfo(QByteArrayView nonce, char* tag)
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        std::memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        info.dwInfoVersion = BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION;
        info.pbNonce = Bytes(nonce.data());
        info.cbNonce = ULONG(nonce.size());
        info.pbTag = reinterpret_cast<PUCHAR>(tag);
        info.cbTag = 16;
        return info;
    }
}

namespace SyncCrypto
{
    QByteArray Sha256(QByteArrayView data)
    {
        Sha256Hasher hasher;
        hasher.Add(data);
        return hasher.Finish();
    }

    QByteArray HmacSha256(QByteArrayView key, QByteArrayView data)
    {
        return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
    }

    QByteArray RandomBytes(int count)
    {
        QByteArray out(count, Qt::Uninitialized);
        if (count > 0 && !NT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(out.data()), ULONG(count),
                                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        {
            qFatal("SyncCrypto: system random generator failed");
        }
        return out;
    }

    QString RandomHex(int byteCount)
    {
        return QString::fromLatin1(RandomBytes(byteCount).toHex());
    }

    struct Sha256Hasher::Impl
    {
        BCRYPT_HASH_HANDLE hash = nullptr;

        void Create()
        {
            hash = nullptr;
            if (!GetProviders().sha256
                || !NT_SUCCESS(BCryptCreateHash(GetProviders().sha256, &hash, nullptr, 0, nullptr, 0, 0)))
            {
                hash = nullptr;
            }
        }

        void Destroy()
        {
            if (hash)
            {
                BCryptDestroyHash(hash);
                hash = nullptr;
            }
        }
    };

    Sha256Hasher::Sha256Hasher()
        : impl(std::make_unique<Impl>())
    {
        impl->Create();
    }

    Sha256Hasher::~Sha256Hasher()
    {
        impl->Destroy();
    }

    void Sha256Hasher::Add(QByteArrayView data)
    {
        if (!impl->hash)
        {
            return;
        }
        qsizetype offset = 0;
        while (offset < data.size())
        {
            const ULONG chunk = ULONG(std::min<qsizetype>(data.size() - offset, 1 << 30));
            if (!NT_SUCCESS(BCryptHashData(impl->hash, Bytes(data.data() + offset), chunk, 0)))
            {
                impl->Destroy();
                return;
            }
            offset += chunk;
        }
    }

    QByteArray Sha256Hasher::Finish()
    {
        QByteArray digest;
        if (impl->hash)
        {
            digest.resize(32);
            if (!NT_SUCCESS(BCryptFinishHash(impl->hash, reinterpret_cast<PUCHAR>(digest.data()), 32, 0)))
            {
                digest.clear();
            }
        }
        impl->Destroy();
        impl->Create();
        return digest;
    }

    QByteArray HashFile(const QString& path, const std::atomic_bool* cancelled)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return {};
        }
        Sha256Hasher hasher;
        QByteArray buffer(1 << 20, Qt::Uninitialized);
        while (true)
        {
            if (cancelled && cancelled->load(std::memory_order_relaxed))
            {
                return {};
            }
            const qint64 read = file.read(buffer.data(), buffer.size());
            if (read < 0)
            {
                return {};
            }
            if (read == 0)
            {
                break;
            }
            hasher.Add(QByteArrayView(buffer.constData(), read));
        }
        return hasher.Finish();
    }

    struct EcdhKey::Impl
    {
        BCRYPT_KEY_HANDLE key = nullptr;
        QByteArray publicKey;
    };

    EcdhKey::EcdhKey()
        : impl(std::make_unique<Impl>())
    {
        const BCRYPT_ALG_HANDLE algorithm = GetProviders().ecdh;
        if (!algorithm || !NT_SUCCESS(BCryptGenerateKeyPair(algorithm, &impl->key, 256, 0)))
        {
            impl->key = nullptr;
            return;
        }
        ULONG size = 0;
        bool ok = NT_SUCCESS(BCryptFinalizeKeyPair(impl->key, 0))
                  && NT_SUCCESS(BCryptExportKey(impl->key, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &size, 0));
        QByteArray blob;
        if (ok)
        {
            blob.resize(size);
            ok = NT_SUCCESS(BCryptExportKey(impl->key, nullptr, BCRYPT_ECCPUBLIC_BLOB,
                                            reinterpret_cast<PUCHAR>(blob.data()), size, &size, 0))
                 && size >= sizeof(BCRYPT_ECCKEY_BLOB) + 64;
        }
        if (ok)
        {
            BCRYPT_ECCKEY_BLOB header;
            std::memcpy(&header, blob.constData(), sizeof(header));
            ok = header.cbKey == 32;
        }
        if (!ok)
        {
            BCryptDestroyKey(impl->key);
            impl->key = nullptr;
            return;
        }
        impl->publicKey = QByteArray(1, '\x04') + blob.mid(sizeof(BCRYPT_ECCKEY_BLOB), 64);
    }

    EcdhKey::~EcdhKey()
    {
        if (impl->key)
        {
            BCryptDestroyKey(impl->key);
        }
    }

    bool EcdhKey::IsValid() const
    {
        return impl->key != nullptr;
    }

    QByteArray EcdhKey::PublicKey() const
    {
        return impl->publicKey;
    }

    QByteArray EcdhKey::Agree(QByteArrayView peerPublicKey) const
    {
        if (!impl->key || peerPublicKey.size() != 65 || peerPublicKey[0] != '\x04')
        {
            return {};
        }
        QByteArray blob(qsizetype(sizeof(BCRYPT_ECCKEY_BLOB)) + 64, '\0');
        BCRYPT_ECCKEY_BLOB header;
        header.dwMagic = BCRYPT_ECDH_PUBLIC_P256_MAGIC;
        header.cbKey = 32;
        std::memcpy(blob.data(), &header, sizeof(header));
        std::memcpy(blob.data() + sizeof(header), peerPublicKey.data() + 1, 64);

        BCRYPT_KEY_HANDLE peerKey = nullptr;
        if (!NT_SUCCESS(BCryptImportKeyPair(GetProviders().ecdh, nullptr, BCRYPT_ECCPUBLIC_BLOB, &peerKey,
                                            reinterpret_cast<PUCHAR>(blob.data()), ULONG(blob.size()), 0)))
        {
            return {};
        }
        BCRYPT_SECRET_HANDLE secret = nullptr;
        const NTSTATUS agreed = BCryptSecretAgreement(impl->key, peerKey, &secret, 0);
        BCryptDestroyKey(peerKey);
        if (!NT_SUCCESS(agreed))
        {
            return {};
        }
        QByteArray shared(32, '\0');
        ULONG size = 0;
        if (!NT_SUCCESS(BCryptDeriveKey(secret, BCRYPT_KDF_RAW_SECRET, nullptr,
                                        reinterpret_cast<PUCHAR>(shared.data()), 32, &size, 0))
            || size != 32)
        {
            shared.clear();
        }
        else
        {
            std::reverse(shared.begin(), shared.end());
        }
        BCryptDestroySecret(secret);
        return shared;
    }

    struct AesGcm::Impl
    {
        BCRYPT_KEY_HANDLE key = nullptr;
    };

    AesGcm::AesGcm(QByteArrayView key)
        : impl(std::make_unique<Impl>())
    {
        const BCRYPT_ALG_HANDLE algorithm = GetProviders().aesGcm;
        if (!algorithm || key.size() != 32
            || !NT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &impl->key, nullptr, 0, Bytes(key.data()), 32, 0)))
        {
            impl->key = nullptr;
        }
    }

    AesGcm::~AesGcm()
    {
        if (impl->key)
        {
            BCryptDestroyKey(impl->key);
        }
    }

    bool AesGcm::IsValid() const
    {
        return impl->key != nullptr;
    }

    QByteArray AesGcm::Seal(QByteArrayView nonce, QByteArrayView plain) const
    {
        if (!impl->key || nonce.size() != 12)
        {
            return {};
        }
        QByteArray out(plain.size() + 16, Qt::Uninitialized);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = MakeAuthInfo(nonce, out.data() + plain.size());
        ULONG written = 0;
        if (!NT_SUCCESS(BCryptEncrypt(impl->key, Bytes(plain.data()), ULONG(plain.size()), &info, nullptr, 0,
                                      reinterpret_cast<PUCHAR>(out.data()), ULONG(plain.size()), &written, 0))
            || written != ULONG(plain.size()))
        {
            return {};
        }
        return out;
    }

    std::optional<QByteArray> AesGcm::Open(QByteArrayView nonce, QByteArrayView sealed) const
    {
        if (!impl->key || nonce.size() != 12 || sealed.size() < 16)
        {
            return std::nullopt;
        }
        const qsizetype length = sealed.size() - 16;
        QByteArray tag(sealed.data() + length, 16);
        QByteArray out(length, Qt::Uninitialized);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = MakeAuthInfo(nonce, tag.data());
        ULONG written = 0;
        if (!NT_SUCCESS(BCryptDecrypt(impl->key, Bytes(sealed.data()), ULONG(length), &info, nullptr, 0,
                                      reinterpret_cast<PUCHAR>(out.data()), ULONG(length), &written, 0))
            || written != ULONG(length))
        {
            return std::nullopt;
        }
        return out;
    }
}
