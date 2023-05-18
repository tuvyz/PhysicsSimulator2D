#ifndef PHYSSIM2D_H
#define PHYSSIM2D_H

#include <vector>
#include <global.h>
#include <ball.h>




class Grid {
public:

    void init(double cellSize) {
        this->cellSize = cellSize;
    }

    void updateGrid(std::vector<Ball>& balls) {
        cells.clear();
        for (Ball& ball : balls) {
            std::pair<int, int> cell = {int(ball.x / cellSize), int(ball.y / cellSize)};
            cells[cell].push_back(&ball);
            ball.currentCell = cell; // Сохраняем текущую ячейку в объекте ball
        }
    }

    std::vector<Ball*> getBallsInNeighbourCells(Ball& ball) {
        std::vector<Ball*> neighbourBalls;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                std::pair<int, int> neighbourCell = {ball.currentCell.first + i, ball.currentCell.second + j};
                auto neighbourCellIt = cells.find(neighbourCell);
                if (neighbourCellIt != cells.end()) {
                    neighbourBalls.insert(neighbourBalls.end(), neighbourCellIt->second.begin(), neighbourCellIt->second.end());
                }
            }
        }

        return neighbourBalls;
    }

private:

    struct pair_hash {
        template <class T1, class T2>
        std::size_t operator () (const std::pair<T1,T2> &p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ h2;
        }
    };

    double cellSize;
    std::unordered_map<std::pair<int, int>, std::vector<Ball*>, pair_hash> cells;
};







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

    // Даёт количество объектов симуляции
    size_t getBallNumber() {
        return balls.size();
    };

    // Удалить объект по id
    void removeObj(long id);

    // Изменить объект по id
    void changeObj(long id, BallParam parameter, double value);


private:

    // Коллизия с границами
    void checkCollisionsWithBounds();

    int BoundsX, BoundsY;

    Grid grid;

    std::recursive_mutex ballsMtx;

    // Коллизия объектов
    void checkCollisionsBetweenBalls();
    void handleCollision(Ball& ball1, Ball& ball2);

    void updateGravitationalAcceleration();

};

#endif // PHYSSIM2D_H
