#ifndef BALL_H
#define BALL_H

#include <cmath>
#include <QObject>
#include <global.h>

class Ball {
public:

    Ball() {
        currentCell.first = 0;
        currentCell.second = 0;
    }

    double x = 0, y = 0;
    double x_old = 0, y_old = 0; // Не заполнять
    double ax = 0, ay = 0; // Текущее ускорение
    double gravCoef = 0;
    double restCoef = 1;
    bool isBlackHole = 0;
    bool isFixPos = 0;

    std::pair<int, int> currentCell;


    // Поставить id
    void setId() {
        id = idStatic++;
    }

    void setSpeedX(double speedX) {
        x_old = x - speedX * DT_PHIS_SIM;
    }
    void setSpeedY(double speedY) {
        y_old = y - speedY * DT_PHIS_SIM;
    }
    double getSpeedX() {
        return (x - x_old) / DT_PHIS_SIM;
    }
    double getSpeedY() {
        return (y - y_old) / DT_PHIS_SIM;
    }

    // Задать массу и радиус
    void setMassAndRadius(double mass, double radius) {
        this->mass = mass;
        this->radius = radius;
        updateDensity();
    }
    // Задать массу и плотность
    void setMassAndDensity(double mass, double density = 0.001) {
        this->mass = mass;
        this->density = density;
        this->radius = std::cbrt((3 * mass) / (4 * M_PI * density));
    }
    // Задать радиус и плотность
    void setRadiusAndDensity(double radius, double density = 0.001) {
        this->radius = radius;
        this->density = density;
        this->mass = (4.0 / 3.0) * M_PI * std::pow(radius, 3) * density;
    }

    double getRadius() {
        return radius;
    }
    double getMass() {
        return mass;
    }
    double getDensity() {
        return density;
    }
    long getId() const {
        return id;
    }

private:
    static long idStatic;
    long id;

    double radius = 0;
    double mass = 0;
    double density = 0;

    void updateDensity() {
        if (radius == 0) {
            density = 0;
        } else {
            density = (3 * mass) / (4 * M_PI * std::pow(radius, 3)); // Возвращаем сантиметры
        }
    }
};

Q_DECLARE_METATYPE(Ball*)

enum BallParam {
    X, Y, AX, AY, VX, VY, GRAV_COEF, REST_COEF,
    IS_BLACK_HOLE, RADIUS, MASS, DENSITY, IS_FIX_POS
};

#endif // BALL_H
