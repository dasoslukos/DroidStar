#ifndef RECORDINGLIBRARYMODEL_H
#define RECORDINGLIBRARYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

class RecordingLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum RecordingRole {
        FileNameRole = Qt::UserRole + 1,
        DisplayTitleRole,
        ModeRole,
        DirectionRole,
        TimestampRole,
        DateTextRole,
        TimeTextRole,
        DurationMsRole,
        DurationTextRole,
        SizeBytesRole,
        SizeTextRole,
        TalkgroupRole,
        SourceIdRole,
        CallsignRole,
        ReflectorRole,
        MetadataRole,
        LocationRole,
        PlaybackUrlRole
    };
    Q_ENUM(RecordingRole)

    explicit RecordingLibraryModel(QObject *parent = nullptr);

    int count() const;

    int rowCount(
        const QModelIndex &parent = QModelIndex()
    ) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole
    ) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QString diagnosticSummary() const;
    Q_INVOKABLE QString playbackSource(int row) const;

signals:
    void countChanged();

private:
    struct RecordingEntry {
        QString fileName;
        QString displayTitle;
        QString mode;
        QString direction;
        QDateTime timestamp;
        qint64 durationMs = 0;
        qint64 sizeBytes = 0;
        QString talkgroup;
        QString sourceId;
        QString callsign;
        QString reflector;
        QString metadata;
        QString location;
        QString playbackUrl;
    };

    QVector<RecordingEntry> m_entries;

    void loadSharedRecordings(
        QVector<RecordingEntry> &entries
    ) const;

    void loadPrivateRecordings(
        QVector<RecordingEntry> &entries
    ) const;

    void loadDirectory(
        QVector<RecordingEntry> &entries,
        const QString &directoryPath,
        const QString &location
    ) const;

    RecordingEntry makeEntry(
        const QString &fileName,
        const QString &playbackUrl,
        const QString &location,
        qint64 sizeBytes,
        qint64 durationMs,
        qint64 modifiedMs
    ) const;

    static QDateTime timestampFromFileName(
        const QString &fileName
    );

    static qint64 wavDurationMs(
        const QString &filePath,
        qint64 sizeBytes
    );

    static qint64 durationFromSize(qint64 sizeBytes);
    static QString formatDuration(qint64 durationMs);
    static QString formatSize(qint64 sizeBytes);
    static bool looksLikeCallsign(const QString &token);
};

#endif // RECORDINGLIBRARYMODEL_H
