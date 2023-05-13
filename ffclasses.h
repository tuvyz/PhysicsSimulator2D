#ifndef CLOSECV_H
#define CLOSECV_H

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavformat/avio.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libavutil/motion_vector.h"
#include "libavutil/frame.h"
#include <libavutil/timestamp.h>
}
#include <QString>
#include <thread>
#include <mutex>
#include <queue>
#include <QDebug>
#include <condition_variable>
#include "global.h"




namespace ff {


class Image : public AVPicture
{
public:
    Image(){}
    Image (const Image &image) {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        clone(image.data[0], image.linesize[0], image.height, image.width, image.format);
    }
    Image(unsigned char *data, int linesize, int height, int width, enum AVPixelFormat format){
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        clone(data, linesize, height, width, format);
    }
    Image(int height, int width, enum AVPixelFormat format = AV_PIX_FMT_RGB24) {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        allocateMemory(height, width, format);
    }
    ~Image() {
        freeMemory();
    }

    bool getIsInvalidSize() {
        return isInvalidSize;
    }

    void allocateMemory(int height, int width, enum AVPixelFormat format = AV_PIX_FMT_RGB24) {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        if (isAllocateMemory == true)
            freeMemory();
        this->height = height;
        this->width = width;
        if (height <= 0 || width <= 0) {
            isInvalidSize = 1;
            return;
        } else
            isInvalidSize = 0;
        avpicture_alloc(this, format, width, height);
        if (format == AV_PIX_FMT_RGB24)
            channels = 3;
        else
            channels = 4;
        this->format = format;
        isAllocateMemory = true;
    }
    void freeMemory() {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        if (isAllocateMemory == true) {
            avpicture_free(this);
            isAllocateMemory = false;
        }
    }

    void clone(unsigned char *data, int linesize, int height, int width, enum AVPixelFormat format = AV_PIX_FMT_RGB24){
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        allocateMemory(height, width, format);
        this->linesize[0] = linesize;
        for (int j = 0; j < channels * height * width; j++) {
            this->data[0][j] = data[j];
        }
    }

    void operator = (const Image &image) {
        std::unique_lock<std::recursive_mutex> un1(mainMtx);
        clone(image.data[0], image.linesize[0], image.height, image.width, image.format);
    }

    int height;
    int width;
    int channels;
    enum AVPixelFormat format;
    bool isAllocateMemory = false;
    bool isInvalidSize = true;

private:
    std::recursive_mutex mainMtx;

};



struct Size
{
    Size(int width = 0, int height = 0) {
        this->width = width;
        this->height = height;
    }
    int width = 0;
    int height = 0;
    bool isValid() {
        return bool(width * height);
    }
};

struct VecRgb {
    VecRgb(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0) {
        rgb[0] = r;
        rgb[1] = g;
        rgb[2] = b;
    }
    unsigned char r() {
        return rgb[0];
    }
    unsigned char g() {
        return rgb[1];
    }
    unsigned char b() {
        return rgb[2];
    }
    unsigned char operator[](int index){
        return rgb[index];
    }
private:
    unsigned char rgb[3];
};



class VideoGraber
{
public:

    VideoGraber() {
        std::unique_lock<std::recursive_mutex> uni(mainMtx);
        isOpen = false;
    }
    VideoGraber(const std::string &url) {
        std::unique_lock<std::recursive_mutex> uni(mainMtx);
        isOpen = false;
        open(url);
    }
    ~VideoGraber(){
        release();
    }

    bool open(const std::string &linkVideoDevice);

    void operator >> (Image &image);

    void rewind(long timeStampMcSec, bool highAccuracy = 0);

    long getCurrentMcSec();
    long getDuractionMcSec();
    long getFirstTimestamp();

    int mcSec2FrameNum(long);
    long frameNum2McSec(int);

    double getFps();
    bool isOpened();
    int frameHeight();
    int frameWidth();
    int streamID();

    double getTimeBaseAVstream() {
        return timeBaseAVstream;
    }

    QDateTime getTimestampStart() {
        return timestampStart;
    }

    long get2PreTs(long curTs);
    long get2NextTs(long curTs);

    void release();


protected:

    int num = 0;
    std::recursive_mutex mainMtx;
    static std::mutex sharedOpenMtx;
    static std::mutex sharedReleaseMtx;

    void grabStream();
    AVFormatContext *formatCtx;
    AVStream *stream;
    AVCodecContext *codecCtx;
    AVPacket packet;
    SwsContext *swsCtx;
    Image image;
    int streamNum;
    std::string linkVideoDevice;
    AVFrame *frame;
    std::atomic<bool> isOpen;
    double fps;
    long maxFrameIndex; // Порой может быть некорректен
    double timeBaseAVstream;
    QDateTime timestampStart;

    std::vector<long> frameNumbers;
};











class FastEncoder : public QObject
{
    Q_OBJECT
public:
    FastEncoder() {
        std::unique_lock<std::recursive_mutex> uni(mainMtx);
        isOpenIn = false;
        isOpenOut = false;
        stopSignal = false;
    }

    ~FastEncoder() {
        release();
    }

    void start(const std::string &inputFile, const std::string &outputFile, const QDateTime startTime);

    void release();
    void stop();

    bool isOpened() {
        std::unique_lock<std::recursive_mutex> uni(mainMtx);
        return isOpenOut * isOpenIn;
    }

protected:

    AVFormatContext *formatCtx;
    AVPacket packet;
    int videoStream;
    AVStream *in_stream;
    AVFormatContext *outFormatCtx;
    AVStream *out_stream;

    QDateTime startTime;

    std::recursive_mutex mainMtx;
    std::mutex stopMtx;
    std::recursive_mutex grabMtx;
    static std::mutex sharedOpenMtx;

    std::atomic<bool> isOpenIn;
    std::atomic<bool> isOpenOut;
    std::atomic<bool> stopSignal;

    std::string linkVideoDevice;

    int64_t pacDuraction; // Расчётная величина на случай если кодер не даёт такой информации

    bool connectOutputFile(const std::string &outputFile);

    bool open(const std::string &linkVideoDevice);

    void releaseInput();

signals:

    void fileCreated(QString);

};






void mergeVideo(const std::string directory, const std::string &outputFile);
void mergeVideo(const std::queue<std::string> &linkVideoDevices,
                    const std::string &outputFile);



void trimVideo(const std::string &linkVideoDevice, const std::string &saveDirectory, bool isOriginalTimestamp,
               long firstFrameMcS = LONG_MIN, long secondFrameMcs = LONG_MIN);



}



#endif // CLOSECV_H
