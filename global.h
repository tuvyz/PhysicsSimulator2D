#ifndef GLOBAL_H
#define GLOBAL_H

#include <chrono>
#include <QDebug>
#include <QtGui>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

#pragma pack(push, 1)


using namespace std;



#define DT_PHIS_SIM 0.016




qint64 random(qint64 a, qint64 b);



#define CHEKING_PROGRESS static int count = 0; qDebug() << count++;





class WaitWindow : public QObject
{
    Q_OBJECT

public:
    explicit WaitWindow(QObject *parent = nullptr);
    ~WaitWindow() override {
        close();
    }

    void open(QString title, QString text, QWidget *parent);
    void setText(QString text);
    void close();
    bool isOpened() {
        return isOpen;
    }

private:
    QMessageBox *mesBox;
    bool isOpen = 0;
    std::recursive_mutex mainMtx;

private slots:
    void closedByUserSlot();

signals:
    void closedByUser();
};





struct MySize
{
    MySize(int width = 0, int height = 0) {
        this->width = width;
        this->height = height;
    }
    int width = 0;
    int height = 0;
    bool isValid() {
        return bool(width * height);
    }
    operator QString() const {
        return "width = " + QString::number(width) + ", height = " + QString::number(height);
    }
};


#pragma pack(pop)



class MyTime {
private:

    chrono::time_point<chrono::steady_clock> begin = chrono::steady_clock::now();

    bool pause = 0;
    long timeBeforePause = 0;
    long deltaTime = 0;

    std::recursive_mutex timerMtx;

public:
    MyTime () {}
    MyTime (uint8_t measurementNumber1or2) {
        operator ()(measurementNumber1or2);
    }

    // 1 - ставит начальную метку времени, 2 - выдаёт разницу между начальной меткой и текущей
    long operator ()(uint8_t measurementNumber1or2);

    void setPauseTimer(bool);
    void setTimerValue(long);

    operator QString const();

    // Сон
    static void sleep(long delayInMicroseconds, bool processEvents = 0);

};






struct MyPoint
{
    MyPoint(){};
    MyPoint(double x, double y) {
        this->x = x, this->y = y;
    }
    MyPoint operator - (const MyPoint &point) {
        return MyPoint(x - point.x, y - point.y);
    }
    MyPoint operator + (const MyPoint &point) {
        return MyPoint(x + point.x, y + point.y);
    }
    bool operator == (const MyPoint &point) {
        return x == point.x && y == point.y;
    }
    bool operator != (const MyPoint &point) {
        return x != point.x || y != point.y;
    }

    MyPoint operator + (double numeric) {
        return MyPoint(x + numeric, y + numeric);
    }
    MyPoint operator - (double numeric) {
        return MyPoint(x - numeric, y - numeric);
    }
    MyPoint operator * (double numeric) {
        return MyPoint(x * numeric, y * numeric);
    }
    MyPoint operator / (double numeric) {
        return MyPoint(x / numeric, y / numeric);
    }

    operator QString() const {
        return "x = " + QString::number(x) + ", y = " + QString::number(y);
    }


    double x = 0;
    double y = 0;
};

namespace std {
MyPoint floor(MyPoint point);
MyPoint ceil(MyPoint point);
MyPoint round(MyPoint point);
MyPoint abs(MyPoint point);
}





// Представление азимута и угла места
struct MyAngle {
    MyAngle(){};
    MyAngle(double az, double el) {
        this->azimuth = az, this->elevation = el;
    }

    double az() {
        return azimuth;
    }
    double el() {
        return elevation;
    }
    void set(MyAngle ang) {
        *this = toBorder(ang);
    }
    void setAz(double az) {
        azimuth = az;
        *this = toBorder(*this);
    }
    void setEl(double el) {
        elevation = el;
        *this = toBorder(*this);
    }

    MyAngle operator - (const MyAngle &ang) {
        return toBorder(MyAngle(azimuth - ang.azimuth, elevation - ang.elevation));
    }
    MyAngle operator + (const MyAngle &ang) {
        return toBorder(MyAngle(azimuth + ang.azimuth, elevation + ang.elevation));
    }
    bool operator == (const MyAngle &ang) {
        return azimuth == ang.azimuth && elevation == ang.elevation;
    }
    bool operator != (const MyAngle &ang) {
        return azimuth != ang.azimuth || elevation != ang.elevation;
    }

    MyAngle operator + (double numeric) {
        return toBorder(MyAngle(azimuth + numeric, elevation + numeric));
    }
    MyAngle operator - (double numeric) {
        return toBorder(MyAngle(azimuth - numeric, elevation - numeric));
    }
    MyAngle operator * (double numeric) {
        return toBorder(MyAngle(azimuth * numeric, elevation * numeric));
    }
    MyAngle operator / (double numeric) {
        return toBorder(MyAngle(azimuth / numeric, elevation / numeric));
    }

    operator QString() const {
        return "Азимут = " + QString::number(azimuth) + ", Угол места = " + QString::number(elevation);
    }

private:

    double azimuth = 0; // Азимут
    double elevation = 0; // Угол места

    // Вписать в границы от 0 до 360
    MyAngle toBorder(MyAngle ang) {
        while (ang.azimuth < 0)
            ang.azimuth += 360;
        while (ang.azimuth > 360)
            ang.azimuth -= 360;

        while (ang.elevation < 0)
            ang.elevation += 360;
        while (ang.elevation > 360)
            ang.elevation -= 360;

        return ang;
    }
};






struct PairPoint : public std::pair<MyPoint, MyPoint> {
    PairPoint(){}
    PairPoint(std::pair<MyPoint, MyPoint> coor) {
        *this = coor;
    }
    PairPoint(MyPoint fP, MyPoint sP) {
        first = fP;
        second = sP;
    }
    PairPoint(double x1, double y1, double x2, double y2) {
        first.x = x1;
        first.y = y1;
        second.x = x2;
        second.y = y2;
    }
    double x1() {
        return first.x;
    }
    double y1() {
        return first.y;
    }
    double x2() {
        return second.x;
    }
    double y2() {
        return second.y;
    }
};

// Проверка находится ли точка в области, ограниченной двумя другими точками
bool pointInAreaBetweenPoints(const MyPoint &point,
                              PairPoint twoPoints,
                              bool pointCanBeOnBorder = 1);















#endif // GLOBAL_H
