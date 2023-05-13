#ifndef PHYSSIM2D_H
#define PHYSSIM2D_H

#include <vector>
#include <global.h>
#include <ball.h>



class PhysSim2D {
public:

    std::vector<Ball> balls;

    void init(int numBalls, int BoundsX, int BoundsY);

    void update(double dt);

    // Добавить объект
    void addObj(Ball &ball);

    // Выдать объект по id
    Ball *getObj(long id);

    // Даёт любой объект
    Ball *getAnyObj();

    // Удалить объект по id
    void removeObj(long id);

    // Изменить объект по id
    void changeObj(long id, BallParam parameter, double value);


private:

    // Коллизия с границами
    void checkCollisionsWithBounds();

    int BoundsX, BoundsY;

    std::recursive_mutex ballsMtx;

    // Коллизия объектов
    void checkCollisionsBetweenBalls();

    void updateGravitationalAcceleration();

};

#endif // PHYSSIM2D_H
