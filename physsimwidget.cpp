#include "physsimwidget.h"

PhysSimWidget::PhysSimWidget(QWidget *parent) : GLgraphics(parent) {

    setFillMode(MySize(BoundsX, BoundsY));
    setWheelScale(1);
    setBackgroundColor(gl::Vec4(1, 1, 1, 1));

    timerSettings = new QTimer;
    connect(timerSettings, &QTimer::timeout, this, &PhysSimWidget::updateSelectedObjSlot);

    trackOverlayAllImgs();
    QObject::connect(this, &gl::GLgraphics::cursorIsOverImage, this, &PhysSimWidget::cursorIsOverImage);

    simulation.init(100, BoundsX, BoundsY);

    connect(this, &PhysSimWidget::sceneUpdateSig, this, &PhysSimWidget::sceneUpdateSlot);
    std::thread th(&PhysSimWidget::sceneUpdate, this);
    th.detach();

    // Загружаем картинку шара
    QImage qBall = QImage(":/new/prefix1/Images/ball.png").convertToFormat(QImage::Format_RGBA8888);

    // Наложение виджетов
    timeSld = new QSlider(Qt::Horizontal, this);
    timeLbl = new QLabel(this);
    vLayout = new QVBoxLayout(this);
    spacerV = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
    hLayout = new QHBoxLayout(this);
    spacerH1 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Ignored);
    spacerH2 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Ignored);
    timeSld->setCursor(Qt::PointingHandCursor);
    timeSld->setMinimum(1);
    timeSld->setMaximum(timeFactorMax / timeFactorStep);
    timeSld->setSingleStep(1);
    connect(timeSld, &QSlider::valueChanged, this, &PhysSimWidget::timeSldChanged);
    timeSld->setValue(1 / timeFactorStep);
    this->setLayout(vLayout);
    vLayout->addItem(spacerV);
    vLayout->addLayout(hLayout);
    hLayout->addItem(spacerH1);
    hLayout->addWidget(timeSld);
    hLayout->addWidget(timeLbl);
    hLayout->addItem(spacerH2);
    timeSld->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    timeLbl->setFixedWidth(21);
    timeLbl->setText(QString("X%1").arg(timeFactor));
}

PhysSimWidget::~PhysSimWidget() {
    isStop = 1;
    std::unique_lock<std::mutex> un(sceneUpdateMtx);
    timerSettings->stop();
    delete timerSettings;
}



// Цикл обновление сцены
void PhysSimWidget::sceneUpdate() {

    std::unique_lock<std::mutex> un(sceneUpdateMtx);

    MyTime timer;
    int maxFps = 60;
    long minTime = 1000000 / maxFps;

    while (!isStop) {

        if (isPause == 1) {
            MyTime::sleep(100000);
            timer(1);
            continue;
        }

        if (getIsUpdateGL()) {
            MyTime::sleep(100);
            continue;
        }

        MyTime::sleep(qMax<long>(0, minTime - timer(2)));

        float deltaTime = timer(2) / 1000000.0f;
        timer(1);

        // Устанавливаем число итераций
        int numIterations = static_cast<int>(std::ceil(timeFactor));

        // Разделим время симуляции на число итераций
        float dtIteration = (DT_PHIS_SIM * timeFactor) / static_cast<float>(numIterations);

        for (int i = 0; i < numIterations; ++i) {
            simulation.update(dtIteration);
        }

        if (idSelectedObj != -1) {
            auto ball = simulation.getObj(idSelectedObj);
            if (ball != nullptr)
                setCenterView(MyPoint(ball->x, ball->y));
        }

        emit sceneUpdateSig();

        // Расчёт FPS
        fpsSum += 1 / deltaTime;
        fpsIter++;
        if (timeUpdateFps(2) > 0.5 * 1000000) {
            fps = std::round(fpsSum / fpsIter);
            timeUpdateFps(1);
            fpsSum = 0;
            fpsIter = 0;
        }

    }
}


// Обновление сцены
void PhysSimWidget::sceneUpdateSlot() {
    drawObjs();
    emit updateGL();
}





// Изменение слайдера ускорения/замедления
void PhysSimWidget::timeSldChanged(int value) {
    timeFactor = value * timeFactorStep;
    timeLbl->setText(QString("X%1").arg(timeFactor));
}


// Клик мыши
void PhysSimWidget::mousePressEvent(QMouseEvent *e) {
    GLgraphics::mousePressEvent(e);
    if(e->button() == Qt::LeftButton) {
        if (idOverCursor == -1) {
            tempBallCentrPoint = MyPoint(getCursorPosOrig().x, getCursorPosOrig().y);
            isCreateObj = 1;
        } else {
            selectObj();
        }
    }
}


// Обратный клик мыши
void PhysSimWidget::mouseReleaseEvent(QMouseEvent *e) {
    GLgraphics::mouseReleaseEvent(e);
    if (isCreateObj == 1) {
        simulation.addObj(tempBall);
        isCreateObj = 0;
        drawObjs();
        emit updateGL();
    }
}

void PhysSimWidget::mouseMoveEvent(QMouseEvent *e) {
    GLgraphics::mouseMoveEvent(e);
    // Создание объекта
    if (isCreateObj == 1) {
        tempBall.setRadiusAndDensity(double(std::sqrt(std::pow(getCursorPosOrig().x - tempBallCentrPoint.x, 2)
                                                       + std::pow(getCursorPosOrig().y - tempBallCentrPoint.y, 2))));
        tempBall.x = tempBallCentrPoint.x;
        tempBall.y = tempBallCentrPoint.y;
        tempBall.x_old = tempBall.x;
        tempBall.y_old = tempBall.y;
//        tempBall.vx = 0;
//        tempBall.vy = random(-500, 500);
        tempBall.gravCoef = 500;
        tempBall.restCoef = 0.7f;
        tempBall.isBlackHole = 0;
        drawObjs();
        emit updateGL();
    }
}


// Пауза по пробелу
void PhysSimWidget::keyPressEvent(QKeyEvent *event) {
    GLgraphics::keyPressEvent(event);
    if(event->key() == Qt::Key_Space)
        isPause = !isPause;
}


// Курсор над объектом
void PhysSimWidget::cursorIsOverImage(QString name) {
    if (name.mid(0, 4) == "ball") {
        idOverCursor = name.remove(0, 4).toULong();
        setCursor(Qt::PointingHandCursor);
    } else {
        idOverCursor = -1;
        setCursor(Qt::ArrowCursor);
    }
}

// Шлёт обновления для объекта в главное окно
void PhysSimWidget::updateSelectedObjSlot() {
    Ball *ball = simulation.getObj(idSelectedObj);
    if (ball == nullptr) {
        ball = simulation.getAnyObj();
        if (ball == nullptr) {
            deselectObjSig();
            return;
        }
        idSelectedObj = ball->getId();
    }
    emit updateSelectedObjSig(ball);
}

// Открыть настройки в главном окне
void PhysSimWidget::selectObj() {
    idSelectedObj = idOverCursor;
    timerSettings->start(200);
}




// Закрыть настройки в главном окне
void PhysSimWidget::deselectObj() {
    idSelectedObj = -1;
    timerSettings->stop();
    emit deselectObjSig();
}

// Удалить объект по сигналу из настроек
void PhysSimWidget::removeObj() {
    simulation.removeObj(idSelectedObj);
    drawObjs();
    emit updateGL();
}

// Объект изменён пользователем
void PhysSimWidget::changeObj(long id, BallParam parameter, double value) {
    simulation.changeObj(id, parameter, value);
    drawObjs();
    emit updateGL();
}


void PhysSimWidget::drawObjs() {
    std::unique_lock<std::mutex> un(mtxDraw);
    clearOverlays();
    for (Ball &ball : simulation.balls) {
        drawObj(ball, QString::number(ball.getId()));
    }
    if (isCreateObj == 1)
        drawObj(tempBall, "_temp");
    if (isFpsEnable) {
        gl::OverlayText oText;
        oText.name = "FPS";
        oText.text = "FPS: " + QString::number(fps);
        oText.posBox = MyPoint(1, -1);
        oText.nullPoint = MyPoint(0, 1);
        oText.height = 14;
        addOverlayText(oText);
    }
    if (isObjSizeEnable) {
        gl::OverlayText oText;
        oText.name = "Obj";
        oText.text = "Obj: " + QString::number(simulation.getBallNumber());
        oText.posBox = MyPoint(1, -17);
        oText.nullPoint = MyPoint(0, 1);
        oText.height = 14;
        addOverlayText(oText);
    }
}

void PhysSimWidget::drawObj(Ball &ball, QString modifier) {
    gl::OverlayImg overlay;
    overlay.name = QString("ball%1").arg(modifier);

    static QImage qBall = QImage(":/new/prefix1/Images/ball.png").convertToFormat(QImage::Format_RGBA8888);
    static GLuint ballTexture = createTexture(qBall);
    overlay.textureId = ballTexture;

    overlay.posImage = MyPoint(ball.x - ball.getRadius(), ball.y - ball.getRadius());
    overlay.size = MySize(ball.getRadius() * 2, ball.getRadius() * 2);
    overlay.isSmoothing = 1;
    addOverlayImg_Fast(overlay);
}
