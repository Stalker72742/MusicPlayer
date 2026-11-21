//
// Created by Stalker7274 on 19.11.2026.
//

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <atomic>
#include <memory>
#include <optional>

/// Cryptography of the sync protocol (see sync.proto), on Windows CNG.
namespace SyncCrypto
{
    /// SHA-256 of data.
    QByteArray Sha256(QByteArrayView data);
    /// HMAC-SHA256 of data under key.
    QByteArray HmacSha256(QByteArrayView key, QByteArrayView data);
    /// Bytes from the system CSPRNG.
    QByteArray RandomBytes(int count);
    /// Random bytes as lowercase hex, e.g. RandomHex(16) for a 32-char id.
    QString RandomHex(int byteCount);

    /// Incremental SHA-256.
    class Sha256Hasher
    {
    public:
        Sha256Hasher();
        ~Sha256Hasher();
        Sha256Hasher(const Sha256Hasher&) = delete;
        Sha256Hasher& operator=(const Sha256Hasher&) = delete;

        void Add(QByteArrayView data);
        /// The digest; the hasher starts over afterwards.
        QByteArray Finish();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    /// SHA-256 of a whole file. Empty if it cannot be read or `cancelled` became true.
    QByteArray HashFile(const QString& path, const std::atomic_bool* cancelled = nullptr);

    /// An ephemeral P-256 key pair for one ECDH exchange.
    class EcdhKey
    {
    public:
        EcdhKey();
        ~EcdhKey();
        EcdhKey(const EcdhKey&) = delete;
        EcdhKey& operator=(const EcdhKey&) = delete;

        bool IsValid() const;
        /// Uncompressed public key: 0x04 || X || Y, 65 bytes.
        QByteArray PublicKey() const;
        /// The shared secret (x-coordinate, 32 bytes big-endian) with the peer's public key; empty if it is invalid.
        QByteArray Agree(QByteArrayView peerPublicKey) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    /// AES-256-GCM with a 12-byte nonce, no AAD and the 16-byte tag after the ciphertext.
    class AesGcm
    {
    public:
        explicit AesGcm(QByteArrayView key);
        ~AesGcm();
        AesGcm(const AesGcm&) = delete;
        AesGcm& operator=(const AesGcm&) = delete;

        bool IsValid() const;
        QByteArray Seal(QByteArrayView nonce, QByteArrayView plain) const;
        /// The plaintext, or nothing if the tag does not match.
        std::optional<QByteArray> Open(QByteArrayView nonce, QByteArrayView sealed) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
