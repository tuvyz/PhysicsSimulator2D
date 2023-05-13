#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Симулятор 2D");

    ui->groupSettings->setVisible(0);

    connect(ui->graphWgt, &PhysSimWidget::updateSelectedObjSig, this, &MainWindow::updateSettings);
    connect(ui->graphWgt, &PhysSimWidget::deselectObjSig, this, &MainWindow::closeSettings);
    connect(this, &MainWindow::closeSettingsSig, ui->graphWgt, &PhysSimWidget::deselectObj);
    connect(this, &MainWindow::removeObj, ui->graphWgt, &PhysSimWidget::removeObj);
    connect(this, &MainWindow::changeObj, ui->graphWgt, &PhysSimWidget::changeObj);
}

MainWindow::~MainWindow()
{
    delete ui;
}




void MainWindow::updateSettings(Ball *ball) {
    curObjId = ball->getId();
    ui->groupSettings->setVisible(1);
    ui->groupSettings->setTitle(QString::number(curObjId));
    if (!ui->vxSpn->hasFocus())
        ui->vxSpn->setValue(ball->getSpeedX());
    if (!ui->vySpn->hasFocus())
        ui->vySpn->setValue(ball->getSpeedY());
    if (!ui->gSpn->hasFocus())
        ui->gSpn->setValue(ball->gravCoef);
    if (!ui->kSpn->hasFocus())
        ui->kSpn->setValue(ball->restCoef);
    if (!ui->mSpn->hasFocus())
        ui->mSpn->setValue(ball->getMass());
    if (!ui->rSpn->hasFocus())
        ui->rSpn->setValue(ball->getRadius());
    if (!ui->pSpn->hasFocus())
        ui->pSpn->setValue(ball->getDensity());
    if (!ui->isBlackHole->hasFocus())
        ui->isBlackHole->setCheckState(ball->isBlackHole ? Qt::Checked : Qt::Unchecked);
}


void MainWindow::closeSettings() {
    ui->groupSettings->setVisible(0);
}


void MainWindow::on_closeSettingsBtn_clicked()
{
    emit closeSettingsSig();
}


void MainWindow::on_removeObjBtn_clicked()
{
    emit removeObj();
}




// Ручное изменение настроек объекта
void MainWindow::on_vxSpn_valueChanged(double arg1)
{
    if (!ui->vxSpn->hasFocus())
        return;
    emit changeObj(curObjId, VX, arg1);
}
void MainWindow::on_vySpn_valueChanged(double arg1)
{
    if (!ui->vySpn->hasFocus())
        return;
    emit changeObj(curObjId, VY, arg1);
}
void MainWindow::on_gSpn_valueChanged(double arg1)
{
    if (!ui->gSpn->hasFocus())
        return;
    emit changeObj(curObjId, GRAV_COEF, arg1);
}
void MainWindow::on_kSpn_valueChanged(double arg1)
{
    if (!ui->kSpn->hasFocus())
        return;
    emit changeObj(curObjId, REST_COEF, arg1);
}
void MainWindow::on_isBlackHole_stateChanged(int arg1)
{
    if (!ui->isBlackHole->hasFocus())
        return;
    emit changeObj(curObjId, IS_BLACK_HOLE, arg1);
}
void MainWindow::on_mSpn_valueChanged(double arg1)
{
    if (!ui->mSpn->hasFocus())
        return;

}
void MainWindow::on_rSpn_valueChanged(double arg1)
{
    if (!ui->rSpn->hasFocus())
        return;

}
void MainWindow::on_pSpn_valueChanged(double arg1)
{
    if (!ui->pSpn->hasFocus())
        return;

}

