#ifndef PHYSSIMWIDGET_H
#define PHYSSIMWIDGET_H

#include "glgraphics.h"
#include "physsim2d.h"
#include <QTimer>
#include <QImage>
#include <QTimer>
#include <QSlider>
#include <QVBoxLayout>
#include <QLabel>


class PhysSimWidget : public gl::GLgraphics {
    Q_OBJECT
public:

    PhysSimWidget(QWidget *parent = nullptr);

    ~PhysSimWidget();

private slots:

    // Изменение слайдера ускорения/замедления
    void timeSldChanged(int value);

    void mousePressEvent(QMouseEvent *e);

    void mouseReleaseEvent(QMouseEvent *e);

    void mouseMoveEvent(QMouseEvent *e);

    // Пауза по пробелу
    void keyPressEvent(QKeyEvent *event);

    // Курсор над объектом
    void cursorIsOverImage(QString name);

    // Шлёт обновления для объекта в главное окно
    void updateSelectedObjSlot();

    // Открыть настройки в главном окне
    void selectObj();

signals:

    void updateSelectedObjSig(Ball *ball);
    void deselectObjSig();
    void sceneUpdateSig();

public slots:

    void sceneUpdateSlot();

    // Закрыть настройки в главном окне
    void deselectObj();

    // Удалить объект по сигналу из настроек
    void removeObj();

    // Объект изменён пользователем
    void changeObj(long id, BallParam parameter, double value);


private:

    void sceneUpdate();

    void drawObjs();
    void drawObj(Ball &ball, QString modifier);

    PhysSim2D simulation;
    gl::ImageSP_t imgBall;
    gl::ImageSP_t backGround;

    ulong BoundsX = 10000000;
    ulong BoundsY = 10000000;

    bool isCreateObj = 0;
    Ball tempBall;
    MyPoint tempBallCentrPoint;

    bool isStop = 0;
    bool isPause = 0;
    std::mutex sceneUpdateMtx;

    bool isFpsEnable = 1;

    long idOverCursor = 0;

    QTimer *timerSettings;
    long idSelectedObj = -1;

    std::mutex mtxDraw;

    float timeFactor = 1;
    float timeFactorStep = 1;
    float timeFactorMax = 30;

    QSlider *timeSld;
    QLabel *timeLbl;
    QVBoxLayout *vLayout;
    QSpacerItem *spacerV;
    QSpacerItem *spacerH1;
    QSpacerItem *spacerH2;
    QHBoxLayout *hLayout;

    MyTime timeUpdateFps;
    float fpsSum = 0;
    int fpsIter = 0;
    int fps = 0;
};


#endif // PHYSSIMWIDGET_H
