#include "ffclasses.h"


using namespace ff;

std::mutex VideoGraber::sharedOpenMtx;
std::mutex VideoGraber::sharedReleaseMtx;


bool VideoGraber::open(const std::string &url)
{
    std::unique_lock<std::recursive_mutex> un1(mainMtx);

    this->linkVideoDevice = url;

    if (isOpen == true) {
        qDebug() << "Already open";
        return 0;
    }

    int errorCode;
    AVCodec *pCodec;

    sharedOpenMtx.lock();
    av_register_all();
    sharedOpenMtx.unlock();

    avformat_network_init();
    formatCtx = avformat_alloc_context();
    frame = av_frame_alloc();

    AVDictionary *d = nullptr;
    av_dict_set(&d, "rtsp_transport", "tcp", 0);
    av_dict_set(&d, "stimeout", "1000000", 0);
    errorCode = avformat_open_input(&formatCtx, url.c_str(), nullptr, &d);

    if (errorCode < 0) {
        qDebug() << "Can not open this file" << QString::fromStdString(url);
        isOpen = false;
        return 0;
    }
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        qDebug() << "Unable to get stream info";
        isOpen = false;
        return 0;
    }

    streamNum = -1;
    for (uint i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            streamNum = int(i);
            stream = formatCtx->streams[i];
            break;
        }
    }
    if (streamNum == -1) {
        qDebug() << "Unable to find video stream";
        isOpen = false;
        return 0;
    }
    codecCtx = stream->codec;



    image.allocateMemory(codecCtx->height, codecCtx->width, AV_PIX_FMT_RGB24);
    if (image.getIsInvalidSize()) {
        isOpen = false;
        return 0;
    }
    pCodec = avcodec_find_decoder(codecCtx->codec_id);
    swsCtx = sws_getContext(image.width, image.height, AV_PIX_FMT_YUV420P, image.width,
            image.height, AV_PIX_FMT_RGB24,
            SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (pCodec == nullptr) {
        qDebug() << "Unsupported codec";
        isOpen = false;
        return 0;
    }
    if (avcodec_open2(codecCtx, pCodec, nullptr) < 0) {
        qDebug() << "Unable to open codec";
        isOpen = false;
        return 0;
    }

    isOpen = true;




    // Длительность одного кадра в единицах AV_TIME_BASE
    double frameDuration = AV_TIME_BASE * av_q2d(codecCtx->time_base);
    frameDuration *= codecCtx->ticks_per_frame;

    fps = AV_TIME_BASE / frameDuration; // Работает только если частота кадров постоянная

    timeBaseAVstream = AV_TIME_BASE * av_q2d(stream->time_base);




    if (url[0] == 'r' &&
            url[1] == 't' &&
            url[2] == 's' &&
            url[3] == 'p') {
        return 1;
    }




    // Получение массива номеров кадров с их меткой времени
    while (av_read_frame(formatCtx, &packet) >= 0) {
        if (packet.stream_index == streamNum) {
            frameNumbers.push_back(packet.pts * timeBaseAVstream);
            av_free_packet(&packet);
        }
    }
    rewind(0);


    // Извлекаем метаданные о метке времени начала видео
    AVDictionaryEntry *timestampAV = nullptr;
    timestampAV = av_dict_get(formatCtx->metadata, "startTime", timestampAV, 0);
    if (timestampAV != nullptr)
        timestampStart = QDateTime::fromString(timestampAV->value, "dd.MM.yyyy hh:mm:ss");
    else
        timestampStart = QDateTime(QDate(2000, 1, 1), QTime(0, 0, 0));


    return 1;
}




void VideoGraber::release() {
    if (isOpen == true) {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
//        std::unique_lock<std::mutex> un2(sharedReleaseMtx);
        packet.dts = 0;
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        sws_freeContext(swsCtx);
        av_frame_free(&frame);
        isOpen = false;
    }
}





void VideoGraber::operator>>(Image &inImage)
{
    std::unique_lock<std::recursive_mutex> uni(mainMtx);

    if (isOpen) {

        while (av_read_frame(formatCtx, &packet) >= 0) {

            if (packet.stream_index == streamNum) {

                int frameFinished = 0;
                avcodec_decode_video2(codecCtx, frame, &frameFinished, &packet);
                av_free_packet(&packet);
                if (frameFinished == 0)
                    continue;

                inImage.allocateMemory(image.height, image.width, image.format);

                // Скейл
                sws_scale(swsCtx, (const uint8_t* const *) frame->data,
                          frame->linesize, 0,
                          inImage.height, inImage.data, inImage.linesize);

                break;
            }

            av_free_packet(&packet);

        }

    }

}




void VideoGraber::rewind(long timeStampMcs, bool highAccuracy) {
    std::unique_lock<std::recursive_mutex> uni(mainMtx);

    if (isOpen) {

        if (highAccuracy == 1) {

            // Поиск предыдущего кадра за целевым
            timeStampMcs = frameNum2McSec(mcSec2FrameNum(timeStampMcs) - 1);

            // Перемотка обратно к ключевому кадру
            av_seek_frame(formatCtx, -1, timeStampMcs, /*AVSEEK_FLAG_ANY*/ AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(codecCtx);

            // Декодирование предыдущих кадров для того чтобы декодировать тот, что перед целевым
            while (1) {

                while (av_read_frame(formatCtx, &packet) >= 0) {
                    if (packet.stream_index == streamNum) {
                        int frameFinished = 0;
                        avcodec_decode_video2(codecCtx, frame, &frameFinished, &packet);
                        av_free_packet(&packet);
                        if (frameFinished == 0)
                            continue;
                        break;
                    }
                    av_free_packet(&packet);
                }

                if (getCurrentMcSec() >= timeStampMcs)
                    break;
//                if (targTS == LONG_MIN)
//                    break;
            }

        } else {

            // Перемотка до ближайшего ключевого кадра позади целевого
            av_seek_frame(formatCtx, -1, timeStampMcs, /*AVSEEK_FLAG_ANY*/ AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(codecCtx);

        }

    }
}






// Временная метка отображения текущего кадра (pts, переведённый в QTime, мкс))
long VideoGraber::getCurrentMcSec() {
    return packet.pts * timeBaseAVstream;
}
// Длительность видео в микросекундах
long VideoGraber::getDuractionMcSec() {
    return frameNumbers.back();
}
// Временная метка первого кадра
long VideoGraber::getFirstTimestamp() {
    return frameNumbers.front();
}



// Переводит микросекунды в номер кадра
int VideoGraber::mcSec2FrameNum(long m) {
    int i = frameNumbers.size() * (double(m) / frameNumbers.back());
    i = qBound<int>(0, i, frameNumbers.size());
    if (frameNumbers[i] < m) {
        while (frameNumbers[i] < m && i < frameNumbers.size() - 1)
            i++;
        if (frameNumbers[i] != m)
            i--;
    } else if (frameNumbers[i] > m) {
        while (frameNumbers[i] > m && i > 0)
            i--;
    }
    return i;
}
// Номер кадра в микросекунды
long VideoGraber::frameNum2McSec(int n) {
    long res = frameNumbers[qBound<int>(0, n, frameNumbers.size())];
    return res;
}



double VideoGraber::getFps() {
    if (isOpen == 1)
        return fps;
    else
        return -1;
}
bool VideoGraber::isOpened() {
    return isOpen;
}
int VideoGraber::frameHeight() {
    if (isOpen == 1)
        return image.height;
    else
        return -1;
}
int VideoGraber::frameWidth() {
    if (isOpen == 1)
        return image.width;
    else
        return -1;
}
int VideoGraber::streamID() {
    if (isOpen == 1)
        return streamNum;
    else
        return -1;
}






// Выдать метку времени предыдущего кадра
long VideoGraber::get2PreTs(long curTs) {
    int i;
    for (i = 0; i < frameNumbers.size(); i++) {
        if (frameNumbers[i] >= curTs)
            break;
    }

    if (i > 0)
        i--;

    return frameNumbers[i];
}

// Выдать метку времени следующего кадра
long VideoGraber::get2NextTs(long curTs) {
    int i;
    for (i = 0; i < frameNumbers.size(); i++) {
        if (frameNumbers[i] >= curTs)
            break;
    }

    if (i < frameNumbers.size() - 1)
        i++;

    return frameNumbers[i];
}












void ff::trimVideo(const std::string &linkVideoDevice, const std::string &saveDirectory, bool isOriginalTimestamp,
               long firstFrameMcS, long secondFrameMcs)
{

    AVFormatContext *formatCtx;
    AVFormatContext *outFormatCtx;
    AVCodecContext *codecCtx;
    AVPacket packet;
    SwsContext *swsCtx;
    int videoStream;
    std::string rtspURL;


    int errorCode;
    rtspURL = linkVideoDevice;
    AVCodec *pCodec;

    static std::mutex sharedMtx;
    sharedMtx.lock();
    av_register_all();
    sharedMtx.unlock();

    avformat_network_init();
    formatCtx = avformat_alloc_context();

    AVDictionary *d = nullptr;
    av_dict_set(&d, "rtsp_transport", "tcp", 0);
    errorCode = avformat_open_input(&formatCtx, rtspURL.c_str(), nullptr, &d);

    if (errorCode < 0) {
        qDebug() << "Can not open this file";
        return;
    }
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        qDebug() << "Unable to get stream info";
        return;
    }

    errorCode = avformat_alloc_output_context2(&outFormatCtx, nullptr, nullptr, saveDirectory.c_str());
    if (errorCode < 0) {
        fprintf(stderr, "Could not create output context\n");
        return;
    }

    videoStream = -1;
    for (uint i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = int(i);
            break;
        }
    }
    if (videoStream == -1) {
        qDebug() << "Unable to find video stream";
        return;
    }



    AVStream *in_stream = formatCtx->streams[videoStream];
    AVStream *out_stream = avformat_new_stream(outFormatCtx, in_stream->codec->codec);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        return;
    }

    errorCode = avcodec_copy_context(out_stream->codec, in_stream->codec);
    if (errorCode < 0) {
        fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
        return;
    }

    out_stream->codec->codec_tag = 0;
    if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


    if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        errorCode = avio_open(&outFormatCtx->pb, saveDirectory.c_str(), AVIO_FLAG_WRITE);
        if (errorCode < 0) {
            fprintf(stderr, "Could not open output file '%s'", saveDirectory.c_str());
            return;
        }
    }




    // Извлечение метаданных о метке времени
    AVDictionaryEntry *timestampAV = nullptr;
    timestampAV = av_dict_get(formatCtx->metadata, "startTime", timestampAV, 0);
    if (timestampAV != nullptr) {
        QDateTime timestampStart = QDateTime::fromString(timestampAV->value, "dd.MM.yyyy hh:mm:ss");
        // Прибавление к метке времени до момента вырезки
        timestampStart = timestampStart.addMSecs(firstFrameMcS / 1000);
        // Копирование метаданных
        auto byAr = timestampStart.toString("dd.MM.yyyy hh:mm:ss").toLocal8Bit();
        const char *timestampChar = byAr.data();
        av_dict_set(&outFormatCtx->metadata, "startTime", timestampChar, 0);
    }




    // Записываем заголовки потока
    errorCode = avformat_write_header(outFormatCtx, nullptr);
    if (errorCode < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return;
    }


    codecCtx = formatCtx->streams[videoStream]->codec;

    pCodec = avcodec_find_decoder(codecCtx->codec_id);
    swsCtx = sws_getContext(codecCtx->width, codecCtx->height, AV_PIX_FMT_YUV420P, codecCtx->width,
                             codecCtx->height, AV_PIX_FMT_RGB24,
                             SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (pCodec == nullptr) {
        qDebug() << "Unsupported codec";
        return;
    }

    if (avcodec_open2(codecCtx, pCodec, nullptr) < 0) {
        qDebug() << "Unable to open codec";
        return;
    }



    VideoGraber videoGraber(linkVideoDevice);
    double timeBaseAVstream = videoGraber.getTimeBaseAVstream();


    av_seek_frame(formatCtx, -1, firstFrameMcS, /*AVSEEK_FLAG_ANY*/ AVSEEK_FLAG_BACKWARD);
    if (secondFrameMcs < 0)
        secondFrameMcs = videoGraber.getDuractionMcSec();

    double lockFps = 25;
    long iterTimestamp = (1000000 / lockFps) / timeBaseAVstream;

    long firstDts = LONG_MIN;
    long firstPts = LONG_MIN;

    long curDts = - iterTimestamp;
    long curPts = - iterTimestamp;

    long curTimeStamp = 0;

    while (av_read_frame(formatCtx, &packet) >= 0) {

        if(packet.stream_index == videoStream) {

            curTimeStamp = packet.pts * timeBaseAVstream;

            // Правим временные метки
            if (isOriginalTimestamp == 1) {
                // Копирование старых временных меток
                if (firstDts == LONG_MIN) {
                    firstPts = packet.pts - formatCtx->streams[packet.stream_index]->start_time;
                    firstDts = packet.dts - formatCtx->streams[packet.stream_index]->first_dts;
                }
                packet.pts -= firstPts;
                packet.dts -= firstDts;
            } else {
                // Генерация новых меток времени
                curPts += iterTimestamp;
                curDts += iterTimestamp;
                packet.pts = curPts;
                packet.dts = curDts;
            }
            packet.pts = av_rescale_q(packet.pts, in_stream->time_base, out_stream->time_base);
            packet.dts = qMin(av_rescale_q(packet.dts, in_stream->time_base, out_stream->time_base), packet.pts);
            packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);

            av_interleaved_write_frame(outFormatCtx, &packet);

        }

        av_free_packet(&packet);

        if (curTimeStamp >= secondFrameMcs)
            break;

    }

    //  Записываем трейлер потока в выходной медиафайл и освобождение ресурсов
    av_write_trailer(outFormatCtx);
    sws_freeContext(swsCtx);
    avio_close(outFormatCtx->pb);
    avformat_close_input(&formatCtx);
    avformat_free_context(formatCtx);
    avformat_free_context(outFormatCtx);
}
















std::mutex FastEncoder::sharedOpenMtx;


bool FastEncoder::open(const std::string &linkVideoDevice)
{

    if (isOpenIn == true) {
        qDebug() << "Already open";
        return 0;
    }

    sharedOpenMtx.lock();
    av_register_all();
    sharedOpenMtx.unlock();

    avformat_network_init();
    formatCtx = avformat_alloc_context();
    AVDictionary *d = nullptr;
    av_dict_set(&d, "rtsp_transport", "tcp", 0);
    av_dict_set(&d, "stimeout", "1000000", 0);

    if (avformat_open_input(&formatCtx, linkVideoDevice.c_str(), nullptr, &d) < 0) {
        qDebug() << "Can not open this file";
        return 0;
    }
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        qDebug() << "Unable to get stream info";
        return 0;
    }
    videoStream = -1;
    for (uint i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = int(i);
            break;
        }
    }
    if (videoStream == -1) {
        qDebug() << "Unable to find video stream";
        return 0;
    }
    in_stream = formatCtx->streams[videoStream];



    double frameDuration = AV_TIME_BASE * av_q2d(in_stream->codec->time_base);
    frameDuration *= in_stream->codec->ticks_per_frame;
    double timeBaseAVstream = AV_TIME_BASE * av_q2d(in_stream->time_base);
    pacDuraction = frameDuration / timeBaseAVstream;


    this->linkVideoDevice = linkVideoDevice;
    isOpenIn = true;

    return 1;
}


void FastEncoder::release() {

    if (isOpenOut) {
        av_write_trailer(outFormatCtx);
        avio_close(outFormatCtx->pb);
        avformat_free_context(outFormatCtx);
        isOpenOut = 0;
    }

    releaseInput();
}



void FastEncoder::stop() {
    stopSignal = 1;
    std::unique_lock<std::recursive_mutex> un2(grabMtx);
    stopSignal = 0;
}



void FastEncoder::releaseInput() {
    if (isOpenIn) {
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        isOpenIn = 0;
    }
}







void FastEncoder::start(const std::string &inputFile, const std::string &outputFile, const QDateTime startTime) {

    this->startTime = startTime;

    std::thread th([this, inputFile, outputFile](){

        std::unique_lock<std::recursive_mutex> un(grabMtx);

        int num_frames = 0;

        while (1) {

            if (stopSignal == 1)
                break;


            if (isOpenIn == 0) {
                if (!open(inputFile)) {
                    qDebug() << "Не удаётся подключиться к камере" << QString::fromStdString(inputFile);
                    MyTime::sleep(1000000);
                    continue;
                } else {
                    qDebug() << "Подключена" << QString::fromStdString(inputFile);
                }
            }
            if (isOpenOut == 0) {
                if (!connectOutputFile(outputFile)) {
                    qDebug() << "Не подключиться к выходному файлу" << QString::fromStdString(outputFile);

                    av_write_trailer(outFormatCtx);
                    avio_close(outFormatCtx->pb);
                    avformat_free_context(outFormatCtx);
                    isOpenIn = 0;
//                    avformat_close_input(&formatCtx);
//                    avformat_free_context(formatCtx);
                    QFile file(QString::fromStdString(outputFile));
                    file.remove();

                    continue;
                }
            }
            if (av_read_frame(formatCtx, &packet) < 0) {
                qDebug() << "Отрубилась" << QString::fromStdString(inputFile);
                releaseInput();
                continue;
            }





            if(packet.stream_index == videoStream) {

                // Создание временных меток
//                packet.pts = lastTS + pacDuraction;
//                packet.dts = packet.pts;
//                lastTS = packet.pts;


                // Создание времнных меток, привязанных к системному времени
                packet.pts = (this->startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0) / av_q2d(in_stream->time_base);
                packet.dts = packet.pts;



                // Скейлим временные метки под новый файл
                packet.duration = av_rescale_q(pacDuraction, in_stream->time_base, out_stream->time_base);
                packet.dts = av_rescale_q(packet.dts, in_stream->time_base, out_stream->time_base);
                packet.pts = av_rescale_q(packet.pts, in_stream->time_base, out_stream->time_base);



                // Запись в файл (пакет освобождается автоматом)
                if (av_interleaved_write_frame(outFormatCtx, &packet) >= 0)
                    num_frames++;


                if (num_frames == 1)
                    emit fileCreated(QString::fromStdString(outputFile));

            }
        }

        release();

    });
    th.detach();

}







bool FastEncoder::connectOutputFile(const std::string &outputFile) {

    std::unique_lock<std::recursive_mutex> un1(mainMtx);
    std::unique_lock<std::mutex> un2(sharedOpenMtx);

    int errorCode = avformat_alloc_output_context2(&outFormatCtx, nullptr, nullptr, outputFile.c_str());
    if (errorCode < 0) {
        fprintf(stderr, "Could not create output context\n");
        return 0;
    }
    out_stream = avformat_new_stream(outFormatCtx, in_stream->codec->codec);
    if (!out_stream) {
        fprintf(stderr, "Failed allocating output stream\n");
        return 0;
    }
    errorCode = avcodec_copy_context(out_stream->codec, in_stream->codec);
    if (errorCode < 0) {
        fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
        return 0;
    }

    // Создание метаданных с временем начала записи
    auto byAr = startTime.toString("dd.MM.yyyy hh:mm:ss").toLocal8Bit();
    const char *timestampChar = byAr.data();
    av_dict_set(&outFormatCtx->metadata, "startTime", timestampChar, 0);

    out_stream->codec->codec_tag = 0;
    if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFormatCtx->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open output file '%s'", outputFile.c_str());
            return 0;
        }
    }
    //  Записываем заголовки потока
    if (avformat_write_header(outFormatCtx, nullptr) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return 0;
    }

    isOpenOut = 1;
    return 1;
}












// Слияние всех видео в папке
void ff::mergeVideo(const std::string directory, const std::string &outputFile) {
    std::queue<std::string> que;
    QDir dir(QString::fromStdString(directory));
    for (auto fileName : dir.entryList()) {
        if (fileName == "." || fileName == "..")
            continue;
        std::string fullPath = directory + "/" + fileName.toStdString();
        que.push(fullPath);
    }
    mergeVideo(que, outputFile);
}

// Слияние видео в одно
void ff::mergeVideo(const std::queue<std::string> &linkVideoDevices,
                        const std::string &outputFile) {

    class Merger : private FastEncoder
    {
    public:
        void go(const std::queue<std::string> linkVideoDevices,
                const std::string &outputFile) {

            std::queue<std::string> queueDevices = linkVideoDevices;

            std::pair<long, long> adjustment(0, 0);
            long lastDts = 0;
            long lastPts = 0;

            bool newVideo = 1;

            while (1) {

                // Открытие очередного источника видео
                if (newVideo == 1) {

                    if (queueDevices.size() > 0) {
                        if (isOpenIn == 1) {
                            avformat_close_input(&formatCtx);
                            avformat_free_context(formatCtx);
                            isOpenIn = 0;
                        }

                        if (open(queueDevices.front())) {
                            queueDevices.pop();
                        } else {
                            queueDevices.pop();
                            continue;
                        }

                        if (isOpenOut == 0)
                            connectOutputFile(outputFile);

                        adjustment.first = lastDts;
                        adjustment.second = lastPts;
                        newVideo = 0;

                    } else
                        break;
                }


                if (av_read_frame(formatCtx, &packet) >= 0) {
                    if(packet.stream_index == videoStream) {

                        // Расставление временных меток в нужном порядке
                        packet.dts += adjustment.first;
                        packet.pts += adjustment.second;

                        if (packet.dts <= lastDts) {
                            packet.dts = lastDts + pacDuraction;
                            adjustment.first += pacDuraction;
                        }
                        if (packet.pts < packet.dts) {
                            adjustment.second += packet.dts - packet.pts;
                            packet.pts = packet.dts;
                        }
                        lastDts = packet.dts;
                        lastPts = packet.pts;

                        // Масштабирование временных меток
                        packet.duration = av_rescale_q(pacDuraction, in_stream->time_base, out_stream->time_base);
                        packet.dts = av_rescale_q(packet.dts, in_stream->time_base, out_stream->time_base);
                        packet.pts = av_rescale_q(packet.pts, in_stream->time_base, out_stream->time_base);

                        // Запись в файл (пакет освобождается автоматом)
                        av_interleaved_write_frame(outFormatCtx, &packet);
                    }
                } else
                    newVideo = 1;

            }

            release();

            qDebug() << "Конец";
        }

    };

    Merger merger;
    merger.go(linkVideoDevices, outputFile);
}






