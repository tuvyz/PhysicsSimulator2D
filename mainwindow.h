#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "physsim2d.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:

    void updateSettings(Ball *ball);
    void closeSettings();

    void on_closeSettingsBtn_clicked();

    void on_removeObjBtn_clicked();

    void on_vxSpn_valueChanged(double arg1);
    void on_vySpn_valueChanged(double arg1);
    void on_gSpn_valueChanged(double arg1);
    void on_kSpn_valueChanged(double arg1);
    void on_mSpn_valueChanged(double arg1);
    void on_rSpn_valueChanged(double arg1);
    void on_pSpn_valueChanged(double arg1);

    void on_isBlackHole_stateChanged(int arg1);

signals:

    void closeSettingsSig();
    void removeObj();

    void changeObj(long id, BallParam parameter, double value);

private:
    Ui::MainWindow *ui;

    long curObjId;
};

#endif // MAINWINDOW_H
