#include "global.h"





qint64 random(qint64 a, qint64 b) {
    static bool isFirst = true;

    if (isFirst) {
        QRandomGenerator::securelySeeded();
        isFirst = false;
    }

    if (a == b)
        return a;

    if (a > b) {
        qSwap(a, b);
    }

    return QRandomGenerator::global()->bounded(a, b);
}



// Проверка находится ли точка в области, ограниченной двумя другими точками
bool pointInAreaBetweenPoints(const MyPoint &point,
                              PairPoint twoPoints,
                              bool pointCanBeOnBorder) {

    MyPoint pointA = twoPoints.first;
    MyPoint pointB = twoPoints.second;

    if (pointCanBeOnBorder) {
        if ((point.x >= std::min(pointA.x, pointB.x) && point.x <= std::max(pointA.x, pointB.x)) &&
            (point.y >= std::min(pointA.y, pointB.y) && point.y <= std::max(pointA.y, pointB.y)))
            return true;
    } else {
        if ((point.x > std::min(pointA.x, pointB.x) && point.x < std::max(pointA.x, pointB.x)) &&
            (point.y > std::min(pointA.y, pointB.y) && point.y < std::max(pointA.y, pointB.y)))
            return true;
    }

    return false;
}






WaitWindow::WaitWindow(QObject *parent) : QObject(parent) {

}

// Создать окно ожидания
void WaitWindow::open(QString title, QString text, QWidget *parent) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    if (isOpen == 1)
        return;
    isOpen = 1;
    mesBox = new QMessageBox(parent);
    connect(mesBox, &QDialog::rejected, this, &WaitWindow::closedByUserSlot);
    mesBox->setWindowTitle(title);
    mesBox->addButton("Отмена", QMessageBox::RejectRole);
    mesBox->setWindowModality(Qt::ApplicationModal);
    setText(text);
    mesBox->setIcon(QMessageBox::Information);
    mesBox->show();
}

// Задать текст окна
void WaitWindow::setText(QString text) {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    if (isOpen == 0)
        return;
    mesBox->setText(text);
}

// Удалить окно ожидания
void WaitWindow::close() {
    std::unique_lock<std::recursive_mutex> un(mainMtx);
    if (isOpen == 0)
        return;
    isOpen = 0;
    disconnect(mesBox, &QDialog::rejected, this, &WaitWindow::closedByUserSlot);
    mesBox->deleteLater();
}

// Закрытие окна ожидания пользователем
void WaitWindow::closedByUserSlot() {
    close();
    emit closedByUser();
}







// Замер времени
long MyTime::operator ()(uint8_t measurementNumber1or2) {

    std::unique_lock<std::recursive_mutex> un(timerMtx);

    switch (measurementNumber1or2) {
    case 1: {

        begin = chrono::steady_clock::now(); // Первый замер времени
        pause = 0;
        timeBeforePause = 0;
        deltaTime = 0;
        return 1;

    }
    case 2: {

        if (pause == 1)
            return timeBeforePause + deltaTime;

        auto end = chrono::steady_clock::now(); // Второй замер времени
        auto elapsed_mc = chrono::duration_cast
                <chrono::microseconds>(end - begin); // Разница в микросекундах;
        auto delay = elapsed_mc.count();

        return delay + deltaTime;

    }
    default:
        return 0;
    }
}






MyTime::operator QString const() {
    long delay = operator()(2);
    QString str;
    if (delay >= 1000000)
        str = QString::number(delay / 1000000) + " секунд";
    else if (delay >= 1000 && delay < 1000000)
        str = QString::number(delay / 1000) + " миллисекунд";
    else if (delay < 1000)
        str = QString::number(delay) + " микросекунд";
    return str;
}






// Пауза таймера
void MyTime::setPauseTimer(bool pause) {

    std::unique_lock<std::recursive_mutex> un(timerMtx);

    if (this->pause == pause)
        return;

    this->pause = pause;

    auto end = chrono::steady_clock::now();
    auto elapsed_mc = chrono::duration_cast
            <chrono::microseconds>(end - begin);
    long absoluteTime = elapsed_mc.count();

    if (pause == 1) {
        timeBeforePause = absoluteTime;
    } else {
        deltaTime -= (absoluteTime - timeBeforePause);
    }

}






// Установить нужное значение таймера
void MyTime::setTimerValue(long value) {

    std::unique_lock<std::recursive_mutex> un(timerMtx);

    if (pause == 0) {
        long delay = operator()(2);
        deltaTime += value - delay;
    } else {
        timeBeforePause = value - deltaTime;
    }

}








// Сон
void MyTime::sleep(long delayInMicroseconds, bool processEvents) {
    if (delayInMicroseconds <= 0)
        return;
    if (processEvents == 1) {
        MyTime time(1);
        while (time(2) < delayInMicroseconds)
            qApp->processEvents();
    } else
        std::this_thread::sleep_for(std::chrono::microseconds(delayInMicroseconds));
}
















namespace std {
MyPoint floor(MyPoint point) {
    return MyPoint(std::floor(point.x), std::floor(point.y));
}
MyPoint ceil(MyPoint point) {
    return MyPoint(std::ceil(point.x), std::ceil(point.y));
}
MyPoint round(MyPoint point) {
    return MyPoint(std::round(point.x), std::round(point.y));
}
MyPoint abs(MyPoint point) {
    return MyPoint(std::abs(point.x), std::abs(point.y));
}
}

