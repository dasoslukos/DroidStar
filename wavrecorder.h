/*
    Simple PCM WAV recorder for DroidStar.

    Copyright (C) 2026 Dasos Lukos

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef WAVRECORDER_H
#define WAVRECORDER_H

#include <QFile>
#include <QMutex>
#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>

class WavRecorder
{
public:
    WavRecorder();
    ~WavRecorder();

    WavRecorder(const WavRecorder &) = delete;
    WavRecorder &operator=(const WavRecorder &) = delete;

    bool start(const QString &filePath,
               quint32 sampleRate = 8000,
               quint16 channelCount = 1,
               quint16 bitsPerSample = 16);

    bool appendPcm(const int16_t *samples, std::size_t sampleCount);
    void stop();

    bool isRecording() const;
    quint64 dataBytesWritten() const;
    QString errorString() const;

private:
    bool writeHeaderLocked();
    bool finalizeHeaderLocked();
    void closeLocked();

    mutable QMutex m_mutex;
    QFile m_file;

    quint32 m_sampleRate;
    quint16 m_channelCount;
    quint16 m_bitsPerSample;
    quint64 m_dataBytes;

    bool m_recording;
    QString m_error;
};

#endif // WAVRECORDER_H
