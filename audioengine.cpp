/*
	Copyright (C) 2019-2021 Doug McLain

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "audioengine.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#endif

#include <cmath>

#if defined (Q_OS_MACOS) || defined(Q_OS_IOS)
#define MACHAK 1
#else
#define MACHAK 0
#endif

AudioEngine::AudioEngine(
	QString in,
	QString out,
	QString mode,
	std::function<QString(bool)> metadataProvider
) :
	m_mode(mode),
	m_metadataProvider(metadataProvider),
	m_outputdevice(out),
	m_inputdevice(in),
	m_out(nullptr),
	m_in(nullptr),
	m_outdev(nullptr),
	m_indev(nullptr),
	m_rxrecordingpending(false),
	m_srm(1)
{
	m_audio_out_temp_buf_p = m_audio_out_temp_buf;
	memset(m_aout_max_buf, 0, sizeof(float) * 200);
	m_aout_max_buf_p = m_aout_max_buf;
	m_aout_max_buf_idx = 0;
	m_aout_gain = 100;
	m_volume = 1.0f;
}

AudioEngine::~AudioEngine()
{
	// Finalize any recording still active during disconnect or shutdown.
	stop_capture();
	stop_playback();
}

void AudioEngine::log_recording(const QString &message)
{
	qDebug().noquote() << message;
	emit recording_log(message);
}

QStringList AudioEngine::discover_audio_devices(uint8_t d)
{
	QStringList list;
	QList<QAudioDevice> devices;

	if(d){
		devices = QMediaDevices::audioOutputs();
	}
	else{
		devices = QMediaDevices::audioInputs();
	}

	for (QList<QAudioDevice>::ConstIterator it = devices.constBegin(); it != devices.constEnd(); ++it ) {
		//fprintf(stderr, "Playback device name = %s\n", (*it).deviceName().toStdString().c_str());fflush(stderr);
		list.append((*it).description());
	}

	return list;
}

void AudioEngine::init()
{
	QAudioFormat format;
	format.setSampleRate(8000);
	format.setChannelCount(1);
	format.setSampleFormat(QAudioFormat::Int16);

	m_agc = true;

	QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
	if(devices.size() == 0){
        qDebug() << "No audio playback hardware found";
	}
	else{
		QAudioDevice device(QMediaDevices::defaultAudioOutput());
		for (QList<QAudioDevice>::ConstIterator it = devices.constBegin(); it != devices.constEnd(); ++it ) {

            //qDebug() << "Playback device name = " << (*it).description();
            //qDebug() << (*it).supportedSampleFormats();
            //qDebug() << (*it).preferredFormat();
            //qDebug() << (*it).minimumSampleRate();
            //qDebug() << (*it).maximumSampleRate();


			if((*it).description() == m_outputdevice){
				device = *it;
			}
		}
		if (!device.isFormatSupported(format)) {
            qWarning() << "Current audio format not supported by playback device";
        }

        qDebug() << "Playback device: " << device.description() << "SR: " << format.sampleRate();

        try{
            m_out = new QAudioSink(device, format, this);
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in constructor:" << e.what();
        }

		m_out->setBufferSize(1280);
		connect(m_out, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
	}

	devices = QMediaDevices::audioInputs();

    if(devices.size() == 0){
        qDebug() <<  "No audio capture hardware found";
	}
	else{
		QAudioDevice device(QMediaDevices::defaultAudioInput());
		for (QList<QAudioDevice>::ConstIterator it = devices.constBegin(); it != devices.constEnd(); ++it ) {
            //qDebug() << "Capture device name = " << (*it).description();
            //qDebug() << (*it).supportedSampleFormats();
            //qDebug() << (*it).preferredFormat();
            //qDebug() << (*it).minimumSampleRate();
            //qDebug() << (*it).maximumSampleRate();

			if((*it).description() == m_inputdevice){
				device = *it;
			}
		}
		if (!device.isFormatSupported(format)) {
            qWarning() << "Current audio format not supported by capture device";
        }

		int sr = 8000;
		if(MACHAK){
			sr = device.preferredFormat().sampleRate();
			m_srm = (float)sr / 8000.0;
		}
		format.setSampleRate(sr);
        m_in = new QAudioSource(device, format, this);
        qDebug() << "Capture device: " <<  device.description() << " SR: " << sr << " resample factor: " << m_srm;
	}
}

void AudioEngine::start_capture()
{
	m_audioinq.clear();

	if(m_in != nullptr){
		m_indev = m_in->start();

		if(m_indev == nullptr){
			qWarning() << "Could not start microphone capture";
			return;
		}

		if(MACHAK) {
			m_srm =
			    static_cast<float>(m_in->format().sampleRate()) /
			    8000.0f;
		}

		if (recording_enabled() &&
		    !m_txrecorder.isRecording()) {
			if (start_recording(
			        m_txrecorder,
			        m_txrecordingpath,
			        m_txrecordinguri,
			        QStringLiteral("TX"),
				        m_metadataProvider
				            ? m_metadataProvider(true)
				            : QString()
			    )) {
				log_recording(
					QStringLiteral("%1 TX recording started: %2")
						.arg(m_mode)
						.arg(m_txrecordingpath)
				);
			}
			else {
				log_recording(
					QStringLiteral("%1 TX recording failed: %2")
						.arg(m_mode)
						.arg(m_txrecorder.errorString())
				);
				m_txrecordingpath.clear();
				m_txrecordinguri.clear();
			}
		}

		connect(
		    m_indev,
		    SIGNAL(readyRead()),
		    SLOT(input_data_received())
		);
	}
}

void AudioEngine::stop_capture()
{
	if(m_in != nullptr){
		if(m_indev != nullptr){
			m_indev->disconnect();
		}

		m_in->stop();
	}

	if (m_txrecorder.isRecording()) {
		m_txrecorder.stop();
		publish_recording(m_txrecordinguri);

		if (!m_txrecorder.errorString().isEmpty()) {
			log_recording(
				QStringLiteral(
					"%1 TX recording failed while saving %2: %3"
				)
					.arg(m_mode)
					.arg(m_txrecordingpath)
					.arg(m_txrecorder.errorString())
			);
		}
		else {
			const double pcmBytes = static_cast<double>(
				m_txrecorder.dataBytesWritten()
			);

			log_recording(
				QStringLiteral(
					"%1 TX recording saved: %2 (%3 s, %4 KiB)"
				)
					.arg(m_mode)
					.arg(m_txrecordingpath)
					.arg(pcmBytes / 16000.0, 0, 'f', 1)
					.arg(pcmBytes / 1024.0, 0, 'f', 1)
			);
		}

		m_txrecordingpath.clear();
		m_txrecordinguri.clear();
	}
}

void AudioEngine::start_playback()
{
	// Delay opening the RX WAV until the first decoded PCM frame.
	// Header metadata is normally available by that point.
	if (recording_enabled() &&
	    !m_rxrecorder.isRecording()) {
		m_rxrecordingpending = true;
	}

	if (m_out) {
		// Only start if stopped or suspended - IdleState and ActiveState mean already started
		if (m_out->state() == QAudio::StoppedState || m_out->state() == QAudio::SuspendedState) {
			m_outdev = m_out->start();
		}
	}
}

void AudioEngine::start_rx_recording_if_needed()
{
	if (!m_rxrecordingpending) {
		return;
	}

	m_rxrecordingpending = false;

	if (!recording_enabled() ||
	    m_rxrecorder.isRecording()) {
		return;
	}

	const QString metadata =
	    m_metadataProvider
	        ? m_metadataProvider(false)
	        : QString();

	if (start_recording(
	        m_rxrecorder,
	        m_rxrecordingpath,
	        m_rxrecordinguri,
	        QStringLiteral("RX"),
	        metadata
	    )) {
		log_recording(
			QStringLiteral("%1 RX recording started: %2")
				.arg(m_mode)
				.arg(m_rxrecordingpath)
		);
	}
	else {
		log_recording(
			QStringLiteral("%1 RX recording failed: %2")
				.arg(m_mode)
				.arg(m_rxrecorder.errorString())
		);
		m_rxrecordingpath.clear();
		m_rxrecordinguri.clear();
	}
}

void AudioEngine::stop_playback()
{
	m_rxrecordingpending = false;
	if (m_rxrecorder.isRecording()) {
		m_rxrecorder.stop();
		publish_recording(m_rxrecordinguri);

		if (!m_rxrecorder.errorString().isEmpty()) {
			log_recording(
				QStringLiteral(
					"%1 RX recording failed while saving %2: %3"
				)
					.arg(m_mode)
					.arg(m_rxrecordingpath)
					.arg(m_rxrecorder.errorString())
			);
		}
		else {
			const double pcmBytes = static_cast<double>(
				m_rxrecorder.dataBytesWritten()
			);

			log_recording(
				QStringLiteral(
					"%1 RX recording saved: %2 (%3 s, %4 KiB)"
				)
					.arg(m_mode)
					.arg(m_rxrecordingpath)
					.arg(pcmBytes / 16000.0, 0, 'f', 1)
					.arg(pcmBytes / 1024.0, 0, 'f', 1)
			);
		}

		m_rxrecordingpath.clear();
		m_rxrecordinguri.clear();
	}

	if (m_out) {
		//m_outdev->reset();
		m_out->reset();
		m_out->stop();
	}
}

void AudioEngine::input_data_received()
{
	QByteArray data = m_indev->readAll();

	if (data.size() > 0){
/*
		fprintf(stderr, "AUDIOIN: ");
		for(int i = 0; i < len; ++i){
			fprintf(stderr, "%02x ", (uint8_t)data.data()[i]);
		}
		fprintf(stderr, "\n");
		fflush(stderr);
*/
		if(MACHAK){
			std::vector<int16_t> samples;
			for(int i = 0; i < data.size(); i += 2){
				samples.push_back(((data.data()[i+1] << 8) & 0xff00) | (data.data()[i] & 0xff));
			}
			for(float i = 0; i < (float)data.size()/2; i += m_srm){
				m_audioinq.enqueue(samples[i]);
			}
		}
		else{
			for(int i = 0; i < data.size(); i += (2 * m_srm)){
				m_audioinq.enqueue(((data.data()[i+1] << 8) & 0xff00) | (data.data()[i] & 0xff));
			}
		}
	}
}

void AudioEngine::write(int16_t *pcm, size_t s)
{
	m_maxlevel = 0;
/*
	fprintf(stderr, "AUDIOOUT: ");
	for(int i = 0; i < s; ++i){
		fprintf(stderr, "%04x ", (uint16_t)pcm[i]);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
*/
	if(m_agc){
		process_audio(pcm, s);
	}

	start_rx_recording_if_needed();

	if (m_rxrecorder.isRecording() &&
	    !m_rxrecorder.appendPcm(pcm, s)) {
		qWarning() << "RX recording write failed:"
		           << m_rxrecorder.errorString();
		m_rxrecorder.stop();
		publish_recording(m_rxrecordinguri);
	}

	size_t l = m_outdev->write((const char *) pcm, sizeof(int16_t) * s);

	if (l*2 < s){
		qDebug() << "AudioEngine::write() " << s << ":" << l << ":" << (int)m_out->bytesFree() << ":" << m_out->bufferSize() << ":" << m_out->error();
	}

	for(uint32_t i = 0; i < s; ++i){
		if(pcm[i] > m_maxlevel){
			m_maxlevel = pcm[i];
		}
	}
}

uint16_t AudioEngine::read(int16_t *pcm, int s)
{
	m_maxlevel = 0;

	if(m_audioinq.size() >= s){
		for(int i = 0; i < s; ++i){
			pcm[i] = m_audioinq.dequeue();
			if(pcm[i] > m_maxlevel){
				m_maxlevel = pcm[i];
			}
		}

		if (m_txrecorder.isRecording() &&
		    !m_txrecorder.appendPcm(
		        pcm,
		        static_cast<std::size_t>(s)
		    )) {
			qWarning() << "TX recording write failed:"
			           << m_txrecorder.errorString();
			m_txrecorder.stop();
		publish_recording(m_txrecordinguri);
		}

		return 1;
	}
	else if(m_in == nullptr){
		memset(pcm, 0, sizeof(int16_t) * s);
		return 1;
	}
	else{
		return 0;
	}
}

uint16_t AudioEngine::read(int16_t *pcm)
{
	int s;
	m_maxlevel = 0;

	if(m_audioinq.size() >= 160){
		s = 160;
	}
	else{
		s = m_audioinq.size();
	}

	for(int i = 0; i < s; ++i){
		pcm[i] = m_audioinq.dequeue();
		if(pcm[i] > m_maxlevel){
			m_maxlevel = pcm[i];
		}
	}

	if (s > 0 &&
	    m_txrecorder.isRecording() &&
	    !m_txrecorder.appendPcm(
	        pcm,
	        static_cast<std::size_t>(s)
	    )) {
		qWarning() << "TX recording write failed:"
		           << m_txrecorder.errorString();
		m_txrecorder.stop();
		publish_recording(m_txrecordinguri);
	}

	return s;
}

// process_audio() based on code from DSD https://github.com/szechyjs/dsd
void AudioEngine::process_audio(int16_t *pcm, size_t s)
{
	float aout_abs, max, gainfactor, gaindelta, maxbuf;

	for(size_t i = 0; i < s; ++i){
		m_audio_out_temp_buf[i] = static_cast<float>(pcm[i]);
	}

	// detect max level
	max = 0;
	m_audio_out_temp_buf_p = m_audio_out_temp_buf;

	for (size_t i = 0; i < s; i++){
		aout_abs = fabsf(*m_audio_out_temp_buf_p);

		if (aout_abs > max){
			max = aout_abs;
		}

		m_audio_out_temp_buf_p++;
	}

	*m_aout_max_buf_p = max;
	m_aout_max_buf_p++;
	m_aout_max_buf_idx++;

	if (m_aout_max_buf_idx > 24){
		m_aout_max_buf_idx = 0;
		m_aout_max_buf_p = m_aout_max_buf;
	}

	// lookup max history
	for (size_t i = 0; i < 25; i++){
		maxbuf = m_aout_max_buf[i];

		if (maxbuf > max){
			max = maxbuf;
		}
	}

	// determine optimal gain level
	if (max > static_cast<float>(0)){
		gainfactor = (static_cast<float>(30000) / max);
	}
	else{
		gainfactor = static_cast<float>(50);
	}

	if (gainfactor < m_aout_gain){
		m_aout_gain = gainfactor;
		gaindelta = static_cast<float>(0);
	}
	else{
		if (gainfactor > static_cast<float>(50)){
			gainfactor = static_cast<float>(50);
		}

		gaindelta = gainfactor - m_aout_gain;

		if (gaindelta > (static_cast<float>(0.05) * m_aout_gain)){
			gaindelta = (static_cast<float>(0.05) * m_aout_gain);
		}
	}

	gaindelta /= static_cast<float>(s); //160

	// adjust output gain
	m_audio_out_temp_buf_p = m_audio_out_temp_buf;

	for (size_t i = 0; i < s; i++){
		*m_audio_out_temp_buf_p = (m_aout_gain + (static_cast<float>(i) * gaindelta)) * (*m_audio_out_temp_buf_p);
		m_audio_out_temp_buf_p++;
	}

	m_aout_gain += (static_cast<float>(s) * gaindelta);
	m_audio_out_temp_buf_p = m_audio_out_temp_buf;

	for (size_t i = 0; i < s; i++){
		*m_audio_out_temp_buf_p *= m_volume;
		if (*m_audio_out_temp_buf_p > static_cast<float>(32760)){
			*m_audio_out_temp_buf_p = static_cast<float>(32760);
		}
		else if (*m_audio_out_temp_buf_p < static_cast<float>(-32760)){
			*m_audio_out_temp_buf_p = static_cast<float>(-32760);
		}
		pcm[i] = static_cast<int16_t>(*m_audio_out_temp_buf_p);
		m_audio_out_temp_buf_p++;
	}
}

bool AudioEngine::recording_enabled() const
{
	QSettings settings(
	    QSettings::IniFormat,
	    QSettings::UserScope,
	    QStringLiteral("dudetronics"),
	    QStringLiteral("droidstar")
	);

	return settings.value(
	    QStringLiteral("RECORDING_ENABLED"),
	    false
	).toBool();
}

QString AudioEngine::sanitize_recording_label(
    const QString &value
) const
{
	QString result;
	bool separatorPending = false;
	const QString upper = value.trimmed().toUpper();

	for (const QChar character : upper) {
		const ushort code = character.unicode();
		const bool asciiLetter = code >= static_cast<ushort>('A') && code <= static_cast<ushort>('Z');
		const bool asciiDigit = code >= static_cast<ushort>('0') && code <= static_cast<ushort>('9');

		if (asciiLetter || asciiDigit) {
			if (separatorPending && !result.isEmpty()) {
				result.append(QLatin1Char('_'));
			}
			result.append(character);
			separatorPending = false;
		}
		else if (!result.isEmpty()) {
			separatorPending = true;
		}
	}

	if (result.size() > 78) {
		result.truncate(78);
	}

	while (result.endsWith(QLatin1Char('_'))) {
		result.chop(1);
	}

	return result;
}

bool AudioEngine::start_recording(
    WavRecorder &recorder,
    QString &displayPath,
    QString &contentUri,
    const QString &direction,
    const QString &metadata
) const
{
	QString safeDirection = direction.trimmed().toUpper();

	if (safeDirection != QStringLiteral("RX") &&
	    safeDirection != QStringLiteral("TX")) {
		safeDirection = QStringLiteral("AUDIO");
	}

	QString labelSource =
	    m_mode +
	    QStringLiteral("_") +
	    safeDirection;

	if (!metadata.trimmed().isEmpty()) {
		labelSource += QStringLiteral("_") + metadata;
	}

	QString recordingLabel = sanitize_recording_label(labelSource);

	if (recordingLabel.isEmpty()) {
		recordingLabel = safeDirection;
	}

	const QString fileName =
	    QDateTime::currentDateTime().toString(
	        QStringLiteral("yyyyMMdd-HHmmss-zzz")
	    ) +
	    QStringLiteral("_") +
	    recordingLabel +
	    QStringLiteral(".wav");

	QSettings settings(
	    QSettings::IniFormat,
	    QSettings::UserScope,
	    QStringLiteral("dudetronics"),
	    QStringLiteral("droidstar")
	);

	const QString location = settings.value(
	    QStringLiteral("RECORDING_LOCATION"),
	    QStringLiteral("shared")
	).toString().simplified().toLower();

	displayPath.clear();
	contentUri.clear();

#if defined(Q_OS_ANDROID)
	if (location == QStringLiteral("shared")) {
		const QJniObject context =
		    QNativeInterface::QAndroidApplication::context();

		const QJniObject javaFileName =
		    QJniObject::fromString(fileName);

		if (context.isValid() && javaFileName.isValid()) {
			const QJniObject result =
			    QJniObject::callStaticObjectMethod(
			        "org/dudetronics/droidstar/RecordingStorage",
			        "createSharedMusicRecording",
			        "(Landroid/content/Context;"
			        "Ljava/lang/String;)Ljava/lang/String;",
			        context.object<jobject>(),
			        javaFileName.object<jstring>()
			    );

			if (result.isValid()) {
				const QString resultText = result.toString();
				const qsizetype separator =
				    resultText.indexOf(QLatin1Char('\n'));

				if (separator > 0) {
					bool descriptorOkay = false;

					const int fileDescriptor =
					    resultText.left(separator).toInt(
					        &descriptorOkay
					    );

					const QString uri =
					    resultText.mid(separator + 1);

					if (descriptorOkay &&
					    fileDescriptor >= 0 &&
					    !uri.isEmpty()) {
						if (recorder.start(
						        fileDescriptor,
						        8000,
						        1,
						        16
						    )) {
							displayPath =
							    QStringLiteral(
							        "Music/DroidStar/"
							        "Recordings/%1"
							    ).arg(fileName);

							contentUri = uri;
							return true;
						}

						const QJniObject javaUri =
						    QJniObject::fromString(uri);

						QJniObject::callStaticMethod<jboolean>(
						    "org/dudetronics/droidstar/"
						    "RecordingStorage",
						    "discardSharedMusicRecording",
						    "(Landroid/content/Context;"
						    "Ljava/lang/String;)Z",
						    context.object<jobject>(),
						    javaUri.object<jstring>()
						);

						return false;
					}
				}
			}
		}

		qWarning()
		    << "Could not create shared Music recording;"
		    << "falling back to app storage";
	}
#endif

	displayPath = make_recording_path(fileName);
	return recorder.start(displayPath, 8000, 1, 16);
}

void AudioEngine::publish_recording(QString &contentUri) const
{
	if (contentUri.isEmpty()) {
		return;
	}

#if defined(Q_OS_ANDROID)
	const QJniObject context =
	    QNativeInterface::QAndroidApplication::context();

	const QJniObject javaUri =
	    QJniObject::fromString(contentUri);

	if (context.isValid() && javaUri.isValid()) {
		const jboolean published =
		    QJniObject::callStaticMethod<jboolean>(
		        "org/dudetronics/droidstar/RecordingStorage",
		        "publishSharedMusicRecording",
		        "(Landroid/content/Context;"
		        "Ljava/lang/String;)Z",
		        context.object<jobject>(),
		        javaUri.object<jstring>()
		    );

		if (!published) {
			qWarning() << "Could not publish MediaStore recording:"
			           << contentUri;
		}
	}
#endif

	contentUri.clear();
}

QString AudioEngine::make_recording_path(
    const QString &fileName
) const
{
    QSettings settings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QStringLiteral("dudetronics"),
        QStringLiteral("droidstar")
    );

    const QString location = settings.value(
        QStringLiteral("RECORDING_LOCATION"),
        QStringLiteral("shared")
    ).toString().simplified().toLower();

    QString basePath;
    QString relativeDirectory = QStringLiteral("Recordings");

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
    // Keep recordings local to DroidStar's app sandbox.
    // Files visibility does not enable iCloud synchronization.
    basePath = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation
    );
    relativeDirectory =
        QStringLiteral("Recordings");
#elif defined(Q_OS_MACOS)
    if (location == QStringLiteral("shared")) {
        basePath = QStandardPaths::writableLocation(
            QStandardPaths::MusicLocation
        );
        relativeDirectory = QStringLiteral("DroidStar/Recordings");
    }
#endif

    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation
        );
        relativeDirectory = QStringLiteral("Recordings");
    }

    const QString recordingDirectory =
        QDir(basePath).filePath(relativeDirectory);

    if (!QDir().mkpath(recordingDirectory)) {
        qWarning() << "Could not create recording directory:"
                   << recordingDirectory;
    }

    return QDir(recordingDirectory).filePath(fileName);
}

void AudioEngine::handleStateChanged(QAudio::State newState)
{
	switch (newState) {
	case QAudio::ActiveState:
        //qDebug() << "AudioOut state active";
		break;
	case QAudio::SuspendedState:
        //qDebug() << "AudioOut state suspended";
		break;
	case QAudio::IdleState:
        //qDebug() << "AudioOut state idle";
		break;
	case QAudio::StoppedState:
        //qDebug() << "AudioOut state stopped";
		break;
	default:
		break;
	}
}
