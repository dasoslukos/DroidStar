#include "wavrecorder.h"

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QtEndian>

#include <limits>

WavRecorder::WavRecorder() :
    m_sampleRate(8000),
    m_channelCount(1),
    m_bitsPerSample(16),
    m_dataBytes(0),
    m_recording(false)
{
}

WavRecorder::~WavRecorder()
{
    stop();
}

bool WavRecorder::start(const QString &filePath,
                        quint32 sampleRate,
                        quint16 channelCount,
                        quint16 bitsPerSample)
{
    QMutexLocker locker(&m_mutex);

    closeLocked();
    m_error.clear();

    if (filePath.trimmed().isEmpty()) {
        m_error = QStringLiteral("Recording file path is empty");
        return false;
    }

    if (sampleRate == 0) {
        m_error = QStringLiteral("WAV sample rate must be greater than zero");
        return false;
    }

    if (channelCount == 0) {
        m_error = QStringLiteral("WAV channel count must be greater than zero");
        return false;
    }

    // appendPcm() currently accepts signed 16-bit PCM samples.
    if (bitsPerSample != 16) {
        m_error = QStringLiteral(
            "WavRecorder currently supports only 16-bit PCM"
        );
        return false;
    }

    const quint64 blockAlign =
        static_cast<quint64>(channelCount) *
        static_cast<quint64>(bitsPerSample / 8);

    const quint64 byteRate =
        static_cast<quint64>(sampleRate) * blockAlign;

    if (blockAlign > std::numeric_limits<quint16>::max() ||
        byteRate > std::numeric_limits<quint32>::max()) {
        m_error = QStringLiteral("Requested WAV format is too large");
        return false;
    }

    const QFileInfo fileInfo(filePath);
    QDir parentDirectory = fileInfo.dir();

    if (!parentDirectory.exists() &&
        !parentDirectory.mkpath(QStringLiteral("."))) {
        m_error = QStringLiteral(
            "Could not create recording directory: %1"
        ).arg(parentDirectory.absolutePath());
        return false;
    }

    m_file.setFileName(filePath);

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral(
            "Could not open recording file: %1"
        ).arg(m_file.errorString());
        return false;
    }

    m_sampleRate = sampleRate;
    m_channelCount = channelCount;
    m_bitsPerSample = bitsPerSample;
    m_dataBytes = 0;

    if (!writeHeaderLocked()) {
        m_file.close();
        return false;
    }

    m_recording = true;
    return true;
}

bool WavRecorder::start(int fileDescriptor,
                        quint32 sampleRate,
                        quint16 channelCount,
                        quint16 bitsPerSample)
{
    QMutexLocker locker(&m_mutex);

    closeLocked();
    m_error.clear();

    if (fileDescriptor < 0) {
        m_error = QStringLiteral("Recording file descriptor is invalid");
        return false;
    }

    if (sampleRate == 0) {
        m_error = QStringLiteral(
            "WAV sample rate must be greater than zero"
        );
        return false;
    }

    if (channelCount == 0) {
        m_error = QStringLiteral(
            "WAV channel count must be greater than zero"
        );
        return false;
    }

    if (bitsPerSample != 16) {
        m_error = QStringLiteral(
            "WavRecorder currently supports only 16-bit PCM"
        );
        return false;
    }

    const quint64 blockAlign =
        static_cast<quint64>(channelCount) *
        static_cast<quint64>(bitsPerSample / 8);

    const quint64 byteRate =
        static_cast<quint64>(sampleRate) * blockAlign;

    if (blockAlign > std::numeric_limits<quint16>::max() ||
        byteRate > std::numeric_limits<quint32>::max()) {
        m_error = QStringLiteral("Requested WAV format is too large");
        return false;
    }

    if (!m_file.open(
            fileDescriptor,
            QIODevice::ReadWrite,
            QFileDevice::AutoCloseHandle
        )) {
        m_error = QStringLiteral(
            "Could not adopt recording file descriptor: %1"
        ).arg(m_file.errorString());

        return false;
    }

    if (!m_file.resize(0) || !m_file.seek(0)) {
        m_error = QStringLiteral(
            "Could not prepare MediaStore recording file: %1"
        ).arg(m_file.errorString());

        m_file.close();
        return false;
    }

    m_sampleRate = sampleRate;
    m_channelCount = channelCount;
    m_bitsPerSample = bitsPerSample;
    m_dataBytes = 0;

    if (!writeHeaderLocked()) {
        m_file.close();
        return false;
    }

    m_recording = true;
    return true;
}

bool WavRecorder::appendPcm(const int16_t *samples,
                            std::size_t sampleCount)
{
    QMutexLocker locker(&m_mutex);

    if (!m_recording || !m_file.isOpen()) {
        m_error = QStringLiteral("No WAV recording is active");
        return false;
    }

    if (sampleCount == 0) {
        return true;
    }

    if (samples == nullptr) {
        m_error = QStringLiteral("PCM sample pointer is null");
        return false;
    }

    constexpr quint64 bytesPerSample = sizeof(int16_t);

    if (sampleCount >
        std::numeric_limits<quint64>::max() / bytesPerSample) {
        m_error = QStringLiteral("PCM sample block is too large");
        return false;
    }

    const quint64 requestedBytes =
        static_cast<quint64>(sampleCount) * bytesPerSample;

    // Standard RIFF/WAV stores the data size in a 32-bit field.
    if (requestedBytes >
        std::numeric_limits<quint32>::max() - m_dataBytes) {
        m_error = QStringLiteral(
            "WAV recording has reached the 4 GiB RIFF size limit"
        );
        return false;
    }

    if (requestedBytes >
        static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        m_error = QStringLiteral("PCM write is too large");
        return false;
    }

    const qint64 byteCount = static_cast<qint64>(requestedBytes);
    qint64 written = -1;

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    written = m_file.write(
        reinterpret_cast<const char *>(samples),
        byteCount
    );
#else
    QByteArray littleEndianData(byteCount, Qt::Uninitialized);

    for (std::size_t i = 0; i < sampleCount; ++i) {
        qToLittleEndian<qint16>(
            static_cast<qint16>(samples[i]),
            reinterpret_cast<uchar *>(
                littleEndianData.data() + (i * sizeof(int16_t))
            )
        );
    }

    written = m_file.write(littleEndianData);
#endif

    if (written > 0) {
        m_dataBytes += static_cast<quint64>(written);
    }

    if (written != byteCount) {
        m_error = QStringLiteral(
            "Incomplete WAV write: requested %1 bytes, wrote %2"
        ).arg(byteCount).arg(written);

        if (written < 0) {
            m_error += QStringLiteral(" (%1)").arg(m_file.errorString());
        }

        return false;
    }

    return true;
}

void WavRecorder::stop()
{
    QMutexLocker locker(&m_mutex);
    closeLocked();
}

bool WavRecorder::isRecording() const
{
    QMutexLocker locker(&m_mutex);
    return m_recording;
}

quint64 WavRecorder::dataBytesWritten() const
{
    QMutexLocker locker(&m_mutex);
    return m_dataBytes;
}

QString WavRecorder::errorString() const
{
    QMutexLocker locker(&m_mutex);
    return m_error;
}

bool WavRecorder::writeHeaderLocked()
{
    const quint16 blockAlign = static_cast<quint16>(
        m_channelCount * (m_bitsPerSample / 8)
    );

    const quint32 byteRate =
        m_sampleRate * static_cast<quint32>(blockAlign);

    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream.writeRawData("RIFF", 4);
    stream << quint32(0); // RIFF size, finalized when recording stops.
    stream.writeRawData("WAVE", 4);

    stream.writeRawData("fmt ", 4);
    stream << quint32(16);            // PCM format chunk size.
    stream << quint16(1);             // Audio format 1 = PCM.
    stream << m_channelCount;
    stream << m_sampleRate;
    stream << byteRate;
    stream << blockAlign;
    stream << m_bitsPerSample;

    stream.writeRawData("data", 4);
    stream << quint32(0); // PCM data size, finalized when recording stops.

    if (stream.status() != QDataStream::Ok || header.size() != 44) {
        m_error = QStringLiteral("Could not construct WAV header");
        return false;
    }

    const qint64 written = m_file.write(header);

    if (written != header.size()) {
        m_error = QStringLiteral(
            "Could not write WAV header: %1"
        ).arg(m_file.errorString());
        return false;
    }

    return true;
}

bool WavRecorder::finalizeHeaderLocked()
{
    if (!m_file.isOpen()) {
        return true;
    }

    if (m_dataBytes > std::numeric_limits<quint32>::max()) {
        m_error = QStringLiteral(
            "WAV data exceeds the standard RIFF size limit"
        );
        return false;
    }

    const quint32 dataSize = static_cast<quint32>(m_dataBytes);
    const quint32 riffSize = 36U + dataSize;

    if (!m_file.seek(4)) {
        m_error = QStringLiteral(
            "Could not seek to WAV RIFF-size field"
        );
        return false;
    }

    QDataStream stream(&m_file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << riffSize;

    if (!m_file.seek(40)) {
        m_error = QStringLiteral(
            "Could not seek to WAV data-size field"
        );
        return false;
    }

    stream << dataSize;

    if (stream.status() != QDataStream::Ok) {
        m_error = QStringLiteral("Could not finalize WAV header");
        return false;
    }

    return true;
}

void WavRecorder::closeLocked()
{
    if (!m_file.isOpen()) {
        m_recording = false;
        return;
    }

    if (m_recording) {
        finalizeHeaderLocked();
    }

    m_file.flush();
    m_file.close();
    m_recording = false;
}
