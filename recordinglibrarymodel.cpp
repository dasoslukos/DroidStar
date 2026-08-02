#include "recordinglibrarymodel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#endif

#include <algorithm>
#include <utility>

namespace {

quint32 readLe32(const QByteArray &data, int offset)
{
    if (offset < 0 || data.size() < offset + 4) {
        return 0;
    }

    return
        static_cast<quint32>(
            static_cast<unsigned char>(data.at(offset))
        ) |
        (static_cast<quint32>(
            static_cast<unsigned char>(data.at(offset + 1))
        ) << 8) |
        (static_cast<quint32>(
            static_cast<unsigned char>(data.at(offset + 2))
        ) << 16) |
        (static_cast<quint32>(
            static_cast<unsigned char>(data.at(offset + 3))
        ) << 24);
}

} // namespace

RecordingLibraryModel::RecordingLibraryModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int RecordingLibraryModel::count() const
{
    return m_entries.size();
}

int RecordingLibraryModel::rowCount(
    const QModelIndex &parent
) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant RecordingLibraryModel::data(
    const QModelIndex &index,
    int role
) const
{
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= m_entries.size()) {
        return {};
    }

    const RecordingEntry &entry = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case DisplayTitleRole:
        return entry.displayTitle;
    case FileNameRole:
        return entry.fileName;
    case ModeRole:
        return entry.mode;
    case DirectionRole:
        return entry.direction;
    case TimestampRole:
        return entry.timestamp;
    case DateTextRole:
        return entry.timestamp.isValid()
            ? entry.timestamp.toString(
                  QStringLiteral("yyyy-MM-dd")
              )
            : QString();
    case TimeTextRole:
        return entry.timestamp.isValid()
            ? entry.timestamp.toString(
                  QStringLiteral("hh:mm:ss")
              )
            : QString();
    case DurationMsRole:
        return entry.durationMs;
    case DurationTextRole:
        return formatDuration(entry.durationMs);
    case SizeBytesRole:
        return entry.sizeBytes;
    case SizeTextRole:
        return formatSize(entry.sizeBytes);
    case TalkgroupRole:
        return entry.talkgroup;
    case SourceIdRole:
        return entry.sourceId;
    case CallsignRole:
        return entry.callsign;
    case ReflectorRole:
        return entry.reflector;
    case MetadataRole:
        return entry.metadata;
    case LocationRole:
        return entry.location;
    case PlaybackUrlRole:
        return entry.playbackUrl;
    default:
        return {};
    }
}

QHash<int, QByteArray>
RecordingLibraryModel::roleNames() const
{
    return {
        {FileNameRole, "fileName"},
        {DisplayTitleRole, "displayTitle"},
        {ModeRole, "mode"},
        {DirectionRole, "direction"},
        {TimestampRole, "timestamp"},
        {DateTextRole, "dateText"},
        {TimeTextRole, "timeText"},
        {DurationMsRole, "durationMs"},
        {DurationTextRole, "durationText"},
        {SizeBytesRole, "sizeBytes"},
        {SizeTextRole, "sizeText"},
        {TalkgroupRole, "talkgroup"},
        {SourceIdRole, "sourceId"},
        {CallsignRole, "callsign"},
        {ReflectorRole, "reflector"},
        {MetadataRole, "metadata"},
        {LocationRole, "location"},
        {PlaybackUrlRole, "playbackUrl"}
    };
}

void RecordingLibraryModel::refresh()
{
    QVector<RecordingEntry> entries;

    loadSharedRecordings(entries);
    loadPrivateRecordings(entries);

    std::sort(
        entries.begin(),
        entries.end(),
        [](const RecordingEntry &left,
           const RecordingEntry &right) {
            if (left.timestamp != right.timestamp) {
                return left.timestamp > right.timestamp;
            }

            return left.fileName > right.fileName;
        }
    );

    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();

    emit countChanged();

    qDebug().noquote()
        << QStringLiteral(
               "Recording library refreshed: %1 item(s)"
           ).arg(m_entries.size());

    const int previewCount = std::min(
        10,
        static_cast<int>(m_entries.size())
    );

    for (int index = 0; index < previewCount; ++index) {
        const RecordingEntry &entry = m_entries.at(index);

        qDebug().noquote()
            << QStringLiteral(
                   "Recording[%1]: %2 | %3 %4 | "
                   "%5 | %6 | %7"
               )
                   .arg(index)
                   .arg(entry.fileName)
                   .arg(entry.mode)
                   .arg(entry.direction)
                   .arg(formatDuration(entry.durationMs))
                   .arg(formatSize(entry.sizeBytes))
                   .arg(entry.location);
    }
}

QVariantMap RecordingLibraryModel::get(int row) const
{
    QVariantMap result;

    if (row < 0 || row >= m_entries.size()) {
        return result;
    }

    const QModelIndex modelIndex = index(row, 0);
    const QHash<int, QByteArray> names = roleNames();

    for (
        auto iterator = names.constBegin();
        iterator != names.constEnd();
        ++iterator
    ) {
        result.insert(
            QString::fromUtf8(iterator.value()),
            data(modelIndex, iterator.key())
        );
    }

    return result;
}

QString RecordingLibraryModel::diagnosticSummary() const
{
    QStringList lines;

    lines.append(
        QStringLiteral(
            "Recording library contains %1 item(s)"
        ).arg(m_entries.size())
    );

    const int previewCount = std::min(
        10,
        static_cast<int>(m_entries.size())
    );

    for (int index = 0; index < previewCount; ++index) {
        const RecordingEntry &entry = m_entries.at(index);

        lines.append(
            QStringLiteral(
                "%1. %2 | %3 %4 | %5 | %6 | %7"
            )
                .arg(index + 1)
                .arg(entry.fileName)
                .arg(entry.mode)
                .arg(entry.direction)
                .arg(formatDuration(entry.durationMs))
                .arg(formatSize(entry.sizeBytes))
                .arg(entry.location)
        );
    }

    return lines.join(QLatin1Char('\n'));
}

QString RecordingLibraryModel::playbackSource(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return {};
    }

    const RecordingEntry &entry = m_entries.at(row);

    if (!entry.playbackUrl.startsWith(
            QStringLiteral("content://"),
            Qt::CaseInsensitive
        )) {
        return entry.playbackUrl;
    }

#if defined(Q_OS_ANDROID)
    const QJniObject context =
        QNativeInterface::QAndroidApplication::context();

    if (!context.isValid()) {
        qWarning()
            << "Could not obtain Android context for playback";
        return {};
    }

    const QJniObject javaUri =
        QJniObject::fromString(entry.playbackUrl);

    const QJniObject javaName =
        QJniObject::fromString(entry.fileName);

    const QJniObject result =
        QJniObject::callStaticObjectMethod(
            "org/dudetronics/droidstar/RecordingStorage",
            "prepareSharedRecordingForPlayback",
            "(Landroid/content/Context;"
            "Ljava/lang/String;"
            "Ljava/lang/String;)"
            "Ljava/lang/String;",
            context.object<jobject>(),
            javaUri.object<jstring>(),
            javaName.object<jstring>()
        );

    if (!result.isValid()) {
        qWarning()
            << "Recording playback cache call returned no result";
        return {};
    }

    const QString localPath = result.toString();

    if (localPath.isEmpty()) {
        qWarning()
            << "Could not prepare shared recording for playback:"
            << entry.fileName;
        return {};
    }

    qDebug().noquote()
        << QStringLiteral(
               "Recording playback source prepared: %1"
           ).arg(localPath);

    return QUrl::fromLocalFile(localPath).toString();
#else
    return {};
#endif
}

void RecordingLibraryModel::loadSharedRecordings(
    QVector<RecordingEntry> &entries
) const
{
#if defined(Q_OS_ANDROID)
    const QJniObject context =
        QNativeInterface::QAndroidApplication::context();

    if (!context.isValid()) {
        qWarning()
            << "Recording library could not obtain Android context";
        return;
    }

    const QJniObject result =
        QJniObject::callStaticObjectMethod(
            "org/dudetronics/droidstar/RecordingStorage",
            "listSharedMusicRecordings",
            "(Landroid/content/Context;)Ljava/lang/String;",
            context.object<jobject>()
        );

    if (!result.isValid()) {
        qWarning()
            << "Recording library MediaStore query returned no result";
        return;
    }

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            result.toString().toUtf8(),
            &parseError
        );

    if (parseError.error != QJsonParseError::NoError ||
        !document.isArray()) {
        qWarning()
            << "Recording library could not parse MediaStore result:"
            << parseError.errorString();
        return;
    }

    const QJsonArray array = document.array();

    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const QString fileName =
            object.value(QStringLiteral("fileName")).toString();

        const QString uri =
            object.value(QStringLiteral("uri")).toString();

        if (fileName.isEmpty() || uri.isEmpty()) {
            continue;
        }

        const qint64 sizeBytes = static_cast<qint64>(
            object.value(
                QStringLiteral("sizeBytes")
            ).toDouble()
        );

        qint64 durationMs = static_cast<qint64>(
            object.value(
                QStringLiteral("durationMs")
            ).toDouble()
        );

        if (durationMs <= 0) {
            durationMs = durationFromSize(sizeBytes);
        }

        const qint64 modifiedMs = static_cast<qint64>(
            object.value(
                QStringLiteral("modifiedSeconds")
            ).toDouble() * 1000.0
        );

        entries.append(
            makeEntry(
                fileName,
                uri,
                QStringLiteral("Shared"),
                sizeBytes,
                durationMs,
                modifiedMs
            )
        );
    }
#elif defined(Q_OS_IOS)
    // iOS recordings are local sandbox documents and
    // are loaded by loadPrivateRecordings().
    Q_UNUSED(entries)
#else
    const QString musicDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::MusicLocation
        );

    if (!musicDirectory.isEmpty()) {
        loadDirectory(
            entries,
            QDir(musicDirectory).filePath(
                QStringLiteral("DroidStar/Recordings")
            ),
            QStringLiteral("Shared")
        );
    }
#endif
}

void RecordingLibraryModel::loadPrivateRecordings(
    QVector<RecordingEntry> &entries
) const
{
    QString basePath;
    QString locationLabel = QStringLiteral("App");

#if defined(Q_OS_ANDROID)
    const QJniObject context =
        QNativeInterface::QAndroidApplication::context();

    if (context.isValid()) {
        const QJniObject externalDirectory =
            context.callObjectMethod(
                "getExternalFilesDir",
                "(Ljava/lang/String;)Ljava/io/File;",
                static_cast<jobject>(nullptr)
            );

        if (externalDirectory.isValid()) {
            const QJniObject absolutePath =
                externalDirectory.callObjectMethod(
                    "getAbsolutePath",
                    "()Ljava/lang/String;"
                );

            if (absolutePath.isValid()) {
                basePath = absolutePath.toString();
            }
        }
    }
#elif defined(Q_OS_IOS)
    // Files exposes this Documents folder locally, but
    // DroidStar does not enable an iCloud container.
    basePath =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
        );
    locationLabel = QStringLiteral("Local");
#endif

    if (basePath.isEmpty()) {
        basePath =
            QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation
            );
    }

    if (basePath.isEmpty()) {
        return;
    }

    loadDirectory(
        entries,
        QDir(basePath).filePath(
            QStringLiteral("Recordings")
        ),
        locationLabel
    );
}

void RecordingLibraryModel::loadDirectory(
    QVector<RecordingEntry> &entries,
    const QString &directoryPath,
    const QString &location
) const
{
    QDir directory(directoryPath);

    if (!directory.exists()) {
        return;
    }

    const QFileInfoList files =
        directory.entryInfoList(
            {
                QStringLiteral("*.wav"),
                QStringLiteral("*.WAV")
            },
            QDir::Files | QDir::Readable,
            QDir::Time
        );

    for (const QFileInfo &fileInfo : files) {
        const qint64 sizeBytes = fileInfo.size();

        entries.append(
            makeEntry(
                fileInfo.fileName(),
                QUrl::fromLocalFile(
                    fileInfo.absoluteFilePath()
                ).toString(),
                location,
                sizeBytes,
                wavDurationMs(
                    fileInfo.absoluteFilePath(),
                    sizeBytes
                ),
                fileInfo.lastModified().toMSecsSinceEpoch()
            )
        );
    }
}

RecordingLibraryModel::RecordingEntry
RecordingLibraryModel::makeEntry(
    const QString &fileName,
    const QString &playbackUrl,
    const QString &location,
    qint64 sizeBytes,
    qint64 durationMs,
    qint64 modifiedMs
) const
{
    RecordingEntry entry;

    entry.fileName = fileName;
    entry.playbackUrl = playbackUrl;
    entry.location = location;
    entry.sizeBytes = sizeBytes;
    entry.durationMs = durationMs;
    entry.timestamp = timestampFromFileName(fileName);

    if (!entry.timestamp.isValid() && modifiedMs > 0) {
        entry.timestamp =
            QDateTime::fromMSecsSinceEpoch(modifiedMs);
    }

    static const QRegularExpression modernPattern(
        QStringLiteral(
            R"(^(\d{8}-\d{6}-\d{3})_([A-Za-z0-9]+)_(RX|TX)(?:_(.+))?\.wav$)"
        ),
        QRegularExpression::CaseInsensitiveOption
    );

    static const QRegularExpression legacyPattern(
        QStringLiteral(
            R"(^(\d{8}-\d{6}-\d{3})_(RX|TX)\.wav$)"
        ),
        QRegularExpression::CaseInsensitiveOption
    );

    const QRegularExpressionMatch modernMatch =
        modernPattern.match(fileName);

    if (modernMatch.hasMatch()) {
        entry.mode =
            modernMatch.captured(2).toUpper();

        entry.direction =
            modernMatch.captured(3).toUpper();

        entry.metadata =
            modernMatch.captured(4).toUpper();
    }
    else {
        const QRegularExpressionMatch legacyMatch =
            legacyPattern.match(fileName);

        if (legacyMatch.hasMatch()) {
            entry.mode = QStringLiteral("Legacy");
            entry.direction =
                legacyMatch.captured(2).toUpper();
        }
        else {
            entry.mode = QStringLiteral("Unknown");
            entry.metadata =
                QFileInfo(fileName).completeBaseName();
        }
    }

    const QStringList tokens =
        entry.metadata.split(
            QLatin1Char('_'),
            Qt::SkipEmptyParts
        );

    int callsignIndex = -1;

    for (int index = 0; index < tokens.size(); ++index) {
        const QString token = tokens.at(index);

        static const QRegularExpression talkgroupPattern(
            QStringLiteral(R"(^TG(\d+)$)")
        );

        static const QRegularExpression idPattern(
            QStringLiteral(R"(^ID(\d+)$)")
        );

        const QRegularExpressionMatch talkgroupMatch =
            talkgroupPattern.match(token);

        if (talkgroupMatch.hasMatch()) {
            entry.talkgroup =
                talkgroupMatch.captured(1);
            continue;
        }

        const QRegularExpressionMatch idMatch =
            idPattern.match(token);

        if (idMatch.hasMatch()) {
            entry.sourceId = idMatch.captured(1);
            continue;
        }

        if (callsignIndex < 0 &&
            looksLikeCallsign(token)) {
            callsignIndex = index;
            entry.callsign = token;
        }
    }

    if (callsignIndex >= 0 &&
        (entry.mode == QStringLiteral("REF") ||
         entry.mode == QStringLiteral("XRF") ||
         entry.mode == QStringLiteral("DCS")) &&
        callsignIndex + 1 < tokens.size()) {
        const QString suffix =
            tokens.at(callsignIndex + 1);

        if (suffix.size() <= 2 &&
            !suffix.startsWith(QStringLiteral("TG")) &&
            !suffix.startsWith(QStringLiteral("ID"))) {
            entry.callsign +=
                QStringLiteral("_") + suffix;
        }
    }

    if (!tokens.isEmpty()) {
        if (entry.mode == QStringLiteral("REF") ||
            entry.mode == QStringLiteral("XRF") ||
            entry.mode == QStringLiteral("DCS")) {
            entry.reflector = tokens.first();
        }
        else if (
            (entry.mode == QStringLiteral("YSF") ||
             entry.mode == QStringLiteral("FCS") ||
             entry.mode == QStringLiteral("M17")) &&
            callsignIndex > 0
        ) {
            entry.reflector =
                tokens.mid(0, callsignIndex).join(
                    QLatin1Char('_')
                );
        }
    }

    if (!entry.callsign.isEmpty()) {
        entry.displayTitle = entry.callsign;
        entry.displayTitle.replace(
            QLatin1Char('_'),
            QLatin1Char(' ')
        );
    }
    else if (!entry.sourceId.isEmpty()) {
        entry.displayTitle =
            QStringLiteral("ID %1").arg(entry.sourceId);
    }
    else if (!entry.reflector.isEmpty()) {
        entry.displayTitle = entry.reflector;
        entry.displayTitle.replace(
            QLatin1Char('_'),
            QLatin1Char(' ')
        );
    }
    else if (!entry.talkgroup.isEmpty()) {
        entry.displayTitle =
            QStringLiteral("TG %1").arg(entry.talkgroup);
    }
    else if (!entry.metadata.isEmpty()) {
        entry.displayTitle = entry.metadata;
        entry.displayTitle.replace(
            QLatin1Char('_'),
            QLatin1Char(' ')
        );
    }
    else {
        entry.displayTitle =
            QStringLiteral("%1 %2")
                .arg(entry.mode, entry.direction)
                .trimmed();
    }

    return entry;
}

QDateTime RecordingLibraryModel::timestampFromFileName(
    const QString &fileName
)
{
    static const QRegularExpression timestampPattern(
        QStringLiteral(
            R"(^(\d{8}-\d{6}-\d{3})_)"
        )
    );

    const QRegularExpressionMatch match =
        timestampPattern.match(fileName);

    if (!match.hasMatch()) {
        return {};
    }

    return QDateTime::fromString(
        match.captured(1),
        QStringLiteral("yyyyMMdd-HHmmss-zzz")
    );
}

qint64 RecordingLibraryModel::wavDurationMs(
    const QString &filePath,
    qint64 sizeBytes
)
{
    QFile file(filePath);

    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray header = file.read(44);

        if (header.size() >= 44 &&
            header.left(4) == QByteArrayLiteral("RIFF") &&
            header.mid(8, 4) == QByteArrayLiteral("WAVE")) {
            const quint32 byteRate = readLe32(header, 28);
            const quint32 dataBytes = readLe32(header, 40);

            if (byteRate > 0 && dataBytes > 0) {
                return static_cast<qint64>(
                    (static_cast<quint64>(dataBytes) *
                     1000ULL) /
                    byteRate
                );
            }
        }
    }

    return durationFromSize(sizeBytes);
}

qint64 RecordingLibraryModel::durationFromSize(
    qint64 sizeBytes
)
{
    if (sizeBytes <= 44) {
        return 0;
    }

    // DroidStar recordings are 8 kHz, mono, signed 16-bit PCM.
    return ((sizeBytes - 44) * 1000LL) / 16000LL;
}

QString RecordingLibraryModel::formatDuration(
    qint64 durationMs
)
{
    if (durationMs < 0) {
        durationMs = 0;
    }

    const qint64 totalSeconds = durationMs / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds / 60) % 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString RecordingLibraryModel::formatSize(
    qint64 sizeBytes
)
{
    if (sizeBytes < 1024) {
        return QStringLiteral("%1 B").arg(sizeBytes);
    }

    const double kib =
        static_cast<double>(sizeBytes) / 1024.0;

    if (kib < 1024.0) {
        return QStringLiteral("%1 KiB")
            .arg(kib, 0, 'f', 1);
    }

    return QStringLiteral("%1 MiB")
        .arg(kib / 1024.0, 0, 'f', 1);
}

bool RecordingLibraryModel::looksLikeCallsign(
    const QString &token
)
{
    if (token.startsWith(QStringLiteral("TG")) ||
        token.startsWith(QStringLiteral("ID")) ||
        token.startsWith(QStringLiteral("REF")) ||
        token.startsWith(QStringLiteral("XRF")) ||
        token.startsWith(QStringLiteral("DCS")) ||
        token.startsWith(QStringLiteral("FCS"))) {
        return false;
    }

    static const QRegularExpression callsignPattern(
        QStringLiteral(
            R"(^(?=.{3,12}$)(?=.*[A-Z])(?=.*\d)[A-Z0-9]+$)"
        )
    );

    return callsignPattern.match(token).hasMatch();
}
