#include "physsim2d.h"


void PhysSim2D::init(int numBalls, int BoundsX, int BoundsY) {

    this->BoundsX = BoundsX;
    this->BoundsY = BoundsY;
//    balls.reserve(numBalls);
//    for (int i = 0; i < numBalls; ++i) {
//        Ball ball;
//        ball.setMassAndDensity(random(400, 500));
//        ball.x = random(BoundsX / 2 - 50 * ball.getRadius(), BoundsX / 2 - 25 * ball.getRadius());
//        ball.y = random(BoundsY / 2 - 50 * ball.getRadius(), BoundsY / 2 + 50 * ball.getRadius());
//        ball.setSpeedX(0);
//        ball.setSpeedY(random(100, 200));
//        ball.ax = 0;
//        ball.ay = 0;
//        ball.restCoef = 0.7f;
//        ball.gravCoef = 500;
//        ball.isBlackHole = 0;
//        addObj(ball);
//    }
//    balls.back().x = BoundsX / 2;
//    balls.back().y = BoundsY / 2;
//    balls.back().setSpeedX(0);
//    balls.back().setSpeedY(0);
//    balls.back().isBlackHole = 1;
//    balls.back().isFixPos = 1;
//    balls.back().setMassAndDensity(50000, 0.1);


//    this->BoundsX = BoundsX;
//    this->BoundsY = BoundsY;
//    balls.reserve(numBalls);
//    for (int i = 0; i < numBalls; ++i) {
//        Ball ball;
//        ball.setMassAndDensity(10000000);
//        ball.x = random(BoundsX / 2 - 100000, BoundsX / 2 + 100000);
//        ball.y = random(BoundsY / 2 - 100000, BoundsY / 2 + 100000);
//        ball.setSpeedX(random(-2000, 2000));
//        ball.setSpeedY(random(-2000, 2000));
//        ball.ax = 0;
//        ball.ay = 0;
//        ball.restCoef = 0.5f;
//        ball.gravCoef = 500;
//        ball.isBlackHole = 0;
//        addObj(ball);
//    }


    for (int i = 0; i < 10000; ++i) {
        Ball ball;
        ball.setMassAndDensity(1);
        ball.x = random(0, BoundsX);
        ball.y = random(0, BoundsY);
        ball.setSpeedX(random(-20, 20));
        ball.setSpeedY(random(-20, 20));
        ball.restCoef = 0.5f;
        addObj(ball);
    }


    size_t minSize = INT_MAX;
    for (auto ball : balls) {
        if (minSize > ball.getRadius() * 2)
            minSize = ball.getRadius() * 2;
    }
    grid.init(minSize);
}


void PhysSim2D::update(double dt) {

    std::unique_lock<std::recursive_mutex> un(ballsMtx);

    //updateGravitationalAcceleration();

    for (Ball &ball : balls) {

        if (ball.isFixPos)
            continue;

        // Вычисляем новые координаты с использованием метода Верле
        double x_new = 2 * ball.x - ball.x_old + ball.ax * dt * dt;
        //double y_new = 2 * ball.y - ball.y_old + ball.ay * dt * dt;
        double y_new = 2 * ball.y - ball.y_old + -100 * dt * dt;

        // Сохраняем текущие координаты для следующего шага
        ball.x_old = ball.x;
        ball.y_old = ball.y;

        // Обновляем текущие координаты
        ball.x = x_new;
        ball.y = y_new;
    }

    checkCollisionsWithBounds();
    checkCollisionsBetweenBalls();
}




// Коллизия с границами
void PhysSim2D::checkCollisionsWithBounds() {
    for (Ball &ball : balls) {
        if (ball.x - ball.getRadius() < 0) {
            ball.x = ball.getRadius();
            ball.setSpeedX(-ball.getSpeedX() * ball.restCoef);
        } else if (ball.x + ball.getRadius() > BoundsX) {
            ball.x = BoundsX - ball.getRadius();
            ball.setSpeedX(-ball.getSpeedX() * ball.restCoef);
        }
        if (ball.y - ball.getRadius() < 0) {
            ball.y = ball.getRadius();
            ball.setSpeedY(-ball.getSpeedY() * ball.restCoef);
        } else if (ball.y + ball.getRadius() > BoundsY) {
            ball.y = BoundsY - ball.getRadius();
            ball.setSpeedY(-ball.getSpeedY() * ball.restCoef);
        }
    }
}




// Коллизия объектов
//void PhysSim2D::checkCollisionsBetweenBalls() {
//    MyTime time;
//    for (std::size_t i = 0; i < balls.size(); ++i) {
//        for (std::size_t j = i + 1; j < balls.size(); ++j) {
//            Ball &ball1 = balls[i];
//            Ball &ball2 = balls[j];
//            handleCollision(ball1, ball2);
//        }
//    }
//    qDebug() << time;
//}
void PhysSim2D::checkCollisionsBetweenBalls() {
    MyTime time;
    grid.updateGrid(balls);
    for (Ball& ball : balls) {
        std::vector<Ball*> neighbourBalls = grid.getBallsInNeighbourCells(ball);
        for (Ball* neighbourBall : neighbourBalls) {
            if (ball.getId() < neighbourBall->getId()) {
                handleCollision(ball, *neighbourBall);
            }
        }
    }
    //qDebug() << time;
}

void PhysSim2D::handleCollision(Ball& ball1, Ball& ball2) {
    double dx = ball1.x - ball2.x;
    double dy = ball1.y - ball2.y;
    double distSquared = dx * dx + dy * dy;
    double minDistSquared = (ball1.getRadius() + ball2.getRadius()) * (ball1.getRadius() + ball2.getRadius());

    if (distSquared <= minDistSquared) {
        double distance = std::sqrt(distSquared);
        double overlap = (ball1.getRadius() + ball2.getRadius()) - distance;

        // Нормализация вектора направления
        double nx = dx / distance;
        double ny = dy / distance;

        // Разделяем шары пропорционально их массам, чтобы они не застревали друг в друге
        double totalMass = ball1.getMass() + ball2.getMass();
        double ball1Overlap = (overlap * ball2.getMass()) / totalMass;
        double ball2Overlap = (overlap * ball1.getMass()) / totalMass;

        // Если один из объектов является черной дырой
        if (ball1.isBlackHole || ball2.isBlackHole) {
            Ball *biggerDensityBall;
            Ball *smallerDensityBall;

            // Сравниваем плотности и массы объектов
            if (ball1.getDensity() > ball2.getDensity() || (ball1.getDensity() == ball2.getDensity() && ball1.getMass() > ball2.getMass())) {
                biggerDensityBall = &ball1;
                smallerDensityBall = &ball2;
            } else {
                biggerDensityBall = &ball2;
                smallerDensityBall = &ball1;
            }

            double newMass = biggerDensityBall->getMass() + smallerDensityBall->getMass();

            // Устанавливаем новую массу и сохраняем плотность большего объекта
            biggerDensityBall->setMassAndDensity(newMass, biggerDensityBall->getDensity());

            // Удаляем поглощенный шар из массива
            balls.erase(std::remove_if(balls.begin(), balls.end(), [smallerDensityBall](const Ball &ball) { return &ball == smallerDensityBall; }), balls.end());

            return;
        }

        ball1.x += nx * ball1Overlap;
        ball1.y += ny * ball1Overlap;
        ball2.x -= nx * ball2Overlap;
        ball2.y -= ny * ball2Overlap;

        double kx = (ball1.getSpeedX() - ball2.getSpeedX());
        double ky = (ball1.getSpeedY() - ball2.getSpeedY());
        double p = 2.0 * (nx * kx + ny * ky) / (ball1.getMass() + ball2.getMass());

        // Считаем среднее значение упругости для обоих шаров
        double avgRestitution = (ball1.restCoef + ball2.restCoef) / 2;

        // Обновляем скорости шаров после столкновения, учитывая упругость
        ball1.setSpeedX(ball1.getSpeedX() - p * ball2.getMass() * nx * avgRestitution);
        ball1.setSpeedY(ball1.getSpeedY() - p * ball2.getMass() * ny * avgRestitution);
        ball2.setSpeedX(ball2.getSpeedX() + p * ball1.getMass() * nx * avgRestitution);
        ball2.setSpeedY(ball2.getSpeedY() + p * ball1.getMass() * ny * avgRestitution);
    }
}





// Расчёт гравитационного ускорения
void PhysSim2D::updateGravitationalAcceleration() {
    // Обнуляем ускорение для всех шаров
    for (Ball &ball : balls) {
        ball.ax = 0;
        ball.ay = 0;
    }

    for (std::size_t i = 0; i < balls.size(); ++i) {
        for (std::size_t j = i + 1; j < balls.size(); ++j) {
            Ball &ball1 = balls[i];
            Ball &ball2 = balls[j];

            double dx = ball2.x - ball1.x;
            double dy = ball2.y - ball1.y;
            double distanceSquared = dx * dx + dy * dy;

            double distance = std::sqrt(distanceSquared);

            if (isnan(distance))
                continue;
            if (distance < ball1.getRadius() + ball2.getRadius()) {
                distance = ball1.getRadius() + ball2.getRadius();
                distanceSquared = distance * distance;
            }

            double force = (ball1.gravCoef + ball2.gravCoef) * ball1.getMass() * ball2.getMass() / (distanceSquared);
            double ax = force * dx / distance;
            double ay = force * dy / distance;

            ball1.ax += ax / ball1.getMass();
            ball1.ay += ay / ball1.getMass();
            ball2.ax -= ax / ball2.getMass();
            ball2.ay -= ay / ball2.getMass();
        }
    }
}




// Добавить объект
void PhysSim2D::addObj(Ball &ball) {
    std::unique_lock<std::recursive_mutex> un(ballsMtx);
    ball.setId();
    balls.emplace_back(ball);
}




// Выдать объект по id
Ball *PhysSim2D::getObj(long id) {
    std::unique_lock<std::recursive_mutex> un(ballsMtx);
    auto it = std::lower_bound(balls.begin(), balls.end(), id, [](const Ball& ball, long id) {
        return ball.getId() < id;
    });

    if (it != balls.end() && (*it).getId() == id) {
        return &(*it);
    } else {
        return nullptr;
    }
}



// Даёт любой объект
Ball *PhysSim2D::getAnyObj() {
    std::unique_lock<std::recursive_mutex> un(ballsMtx);
    if (balls.size() == 0)
        return nullptr;
    else
        return &balls[0];
}




// Удалить объект по id
void PhysSim2D::removeObj(long id) {
    std::unique_lock<std::recursive_mutex> un(ballsMtx);
    auto it = std::lower_bound(balls.begin(), balls.end(), id, [](const Ball& ball, long id) {
        return ball.getId() < id;
    });
    if (it != balls.end() && (*it).getId() == id) {
        balls.erase(it);
    }
}




// Изменить объект по id
void PhysSim2D::changeObj(long id, BallParam parameter, double value) {
    std::unique_lock<std::recursive_mutex> un(ballsMtx);
    Ball *ball = getObj(id);
    if (ball == nullptr)
        return;

    switch (parameter) {
    case VX:
        ball->setSpeedX(value);
        break;
    case VY:
        ball->setSpeedY(value);
        break;
    case GRAV_COEF:
        ball->gravCoef = value;
        break;
    case REST_COEF:
        ball->restCoef = value;
        break;
    case IS_BLACK_HOLE:
        ball->isBlackHole = bool(value);
        break;
    }
}



