#include "glgraphics.h"


using namespace gl;



GLgraphics::GLgraphics(QWidget *parent) : QOpenGLWidget(parent)
{

}





void GLgraphics::initializeGL()
{
    initializeOpenGLFunctions();

    glGenTextures(1, &overlayTexture);
    glBindTexture(GL_TEXTURE_2D, overlayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width(), height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!shPrOpacity) {
        shPrOpacity.reset(new QOpenGLShaderProgram);
        shPrOpacity->create();
        if (!shPrOpacity->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/new/prefix1/Shaders/shader.vert")) {
            QMessageBox::critical(this, " ", QString("Возникла ошибка при добавлении вершинного шейдера! %1").arg(shPrOpacity->log()));
            shPrOpacity.reset();
            return;
        }
        if (!shPrOpacity->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/new/prefix1/Shaders/shader.frag")) {
            QMessageBox::critical(this, " ", QString("Возникла ошибка при добавлении фрагментного шейдера! %1").arg(shPrOpacity->log()));
            shPrOpacity.reset();
            return;
        }
    }
    if (!shPrOpacity->link()) {
        QMessageBox::critical(this, "Ошибка", QString("Не могу слинковать шейдер! %1").arg(shPrOpacity->log()));
        shPrOpacity.reset();
        return;
    }

    frameTex.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    frameTex->setMinificationFilter(QOpenGLTexture::Nearest);
    frameTex->setMagnificationFilter(QOpenGLTexture::Linear);
    frameTex->create();

    crossQImg = QImage(":/new/prefix1/Image/PointRed.png").convertToFormat(QImage::Format_RGBA8888);
    this->crossImg = ff::Image(crossQImg.bits(), crossQImg.bytesPerLine(), crossQImg.height(), crossQImg.width(), AV_PIX_FMT_RGBA);

    connect(this, &GLgraphics::updateGL, this, [this](){
        isUpdateGL = 1;
        update();
    }, Qt::QueuedConnection);

    connect(this, SIGNAL(setEnabledSig(bool)), this, SLOT(setEnabled(bool)));
    isResizing = 1;

    connect(this, SIGNAL(initSignal()), this, SLOT(initSlot()));
    emit initSignal();
}



void GLgraphics::initSlot() {
    setMouseTracking(1);
    setFocusPolicy(Qt::StrongFocus);
    disconnect(this, SIGNAL(initSignal()), this, SLOT(initSlot()));
}














// Сбросить всё
void GLgraphics::reset() {
    std::unique_lock<std::recursive_mutex> un(paintMtx);
    clearOverlays();
    framImg.reset();
    wheelSpinsState = 0;
    isResizing = 1;
    zoom = 1;
}





void GLgraphics::resizeGL(int w, int h)
{
    glMatrixMode(GL_PROJECTION); // Текущей матрицей становится матрица проекции (?)
    glLoadIdentity(); // Заменяет текущую матрицу на единичную (?)

    glOrtho(0, w, 0, h, -1, 1); // Умножение текущей матрицы на ортогональную (по сути проекция в виде коробки)

    glBindTexture(GL_TEXTURE_2D, overlayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    isResizing++;
}






void GLgraphics::setQuadsPoligon(GLint bl_x, GLint bl_y, GLint tp_x, GLint tp_y)
{
    glEnable(GL_TEXTURE_2D);
    // Задание примитива (прямоугольного)
    glBegin(GL_QUADS); {
        // Вершины задаются через glVertex против часовой стрелки от левого верхнего угла
        // glTexCoord - устанавливает координаты текстуры
        // glVertex - углы полигона, с котрыми будет ассациироваться последни вызов glTexCoord
        glTexCoord2f(0.0f, 0.0f); glVertex2i(bl_x,  tp_y);
        glTexCoord2f(0.0f, 1.0f); glVertex2i(bl_x,  bl_y);
        glTexCoord2f(1.0f, 1.0f); glVertex2i(tp_x,  bl_y);
        glTexCoord2f(1.0f, 0.0f); glVertex2i(tp_x,  tp_y);
    } glEnd();
    glDisable(GL_TEXTURE_2D);
}








void GLgraphics::paintGL()
{
    std::unique_lock<std::recursive_mutex> un(paintMtx);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(bgColor.r, bgColor.g, bgColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);



    if (isFillMode == 1) {
        originalSize = fillSize;
    } else {
        if (framImg == nullptr)
            return;
        originalSize.height = framImg->height;
        originalSize.width = framImg->width;
    }

    double viewX1, viewX2, viewY1, viewY2;
    bool isZooming = 0;

    // Это та координата (в координатной системе изображения), на которую указывает курсор при приближении
    MyPoint anchorPointToZoom = b2oCoor(cursorPosition);

    // Обработчик прокрутки
    if (wheelSpinsState == 1) {
        zoom *= 1.1;
        wheelSpinsState = 0;
        isZooming = 1;
    } else if (wheelSpinsState == -1) {
        zoom /= 1.1;
        wheelSpinsState = 0;
        isZooming = 1;
    }

    if (isResizing > 0 || isZooming) {

        if (isFillMode == 0)
            zoom = qBound<float>(1, zoom, 255);

        resizeSize = MySize(width(), height());

        // ПЕРЕРАСЧЁТ ЗУМА И РАСЧЁТ НУЛЕВОЙ КООРДИНАТЫ ДЛЯ СКЕЙЛА
        double lastZoom = currentZoom;
        double ratioHeight = (double)resizeSize.height / originalSize.height;
        double ratioWidth = (double)resizeSize.width / originalSize.width;

        // Если уровень зума равен 1, то изображение вписывается в заданные рамки 1:1. Для этого
        // расчитывается реальный уровень зума (с плавающей запятой) и размер полос по краям
        if (zoom == 1 && isFillMode == 0) {

            currentZoom = qMin<double>(ratioHeight, ratioWidth);
            if (ratioHeight == ratioWidth)
                indent = MyPoint(0, 0);
            else
                indent = MyPoint(resizeSize.width - currentZoom * originalSize.width, resizeSize.height - currentZoom * originalSize.height) / 2;

            indent.x = ceil(indent.x);
            indent.y = ceil(indent.y);

            nullPointToZoom = MyPoint(0, 0);

        } else {

            // Если уровень зума настолько низок, что не позволяет растянуть изображение на заданный
            // размер, то увеличиваем его во избежание полос по краям
            currentZoom = qMax<double>(qMax<double>(zoom, ratioWidth), ratioHeight);

            if (lastZoom != currentZoom) {

                // Расчёт нулевой координаты, (той, с которой начинается цикл для оригинального изображения)
                nullPointToZoom = anchorPointToZoom - cursorPosition / currentZoom;
                nullPointToZoom.x = qBound<double>(0, nullPointToZoom.x, originalSize.width - resizeSize.width / currentZoom);
                nullPointToZoom.y = qBound<double>(0, nullPointToZoom.y, originalSize.height - resizeSize.height / currentZoom);

                indent = MyPoint(0, 0);
            }
        }

    }

    // Перемещение изображения
    if (rightButton) {
        if (anchorPointToMove == MyPoint(-1, -1))
            anchorPointToMove = anchorPointToZoom;
        nullPointToZoom.x = qBound<double>(0, nullPointToZoom.x + anchorPointToMove.x - anchorPointToZoom.x, originalSize.width - resizeSize.width / currentZoom);
        nullPointToZoom.y = qBound<double>(0, nullPointToZoom.y + anchorPointToMove.y - anchorPointToZoom.y, originalSize.height - resizeSize.height / currentZoom );
    } else
        anchorPointToMove = MyPoint(-1, -1);


    if (isFillMode && isFillModeInit) {
        isFillModeInit = 0;
        nullPointToZoom.x = fillSize.width / 2 - resizeSize.width / (2 * currentZoom);
        nullPointToZoom.y = fillSize.height / 2 - resizeSize.height / (2 * currentZoom);
    }


    if (isCenterViewEnable) {
        isCenterViewEnable = 0;
        nullPointToZoom.x = centerView.x - resizeSize.width / (2 * currentZoom);
        nullPointToZoom.y = centerView.y - resizeSize.height / (2 * currentZoom);
    }


    viewX1 = indent.x - nullPointToZoom.x * currentZoom;
    viewY1 = indent.y - nullPointToZoom.y * currentZoom;
    viewX2 = viewX1 + originalSize.width * currentZoom;
    viewY2 = viewY1 + originalSize.height * currentZoom;





    // Картинка
    if (!isFillMode) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isSmoothing ? GL_LINEAR : GL_NEAREST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, frameTex->textureId()); // Именует и создаёт текстурный объект для изображения текстуры
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, framImg->channels,
                     framImg->width,
                     framImg->height,
                     0,
                     framImg->channels == 4 ? GL_RGBA : GL_RGB,
                     GL_UNSIGNED_BYTE,
                     *framImg->data); // Карта текстуры
        setQuadsPoligon(viewX1, viewY1, viewX2, viewY2);
    }






    // Вычисление доступных слоёв
    std::vector<int> availableLayers;
    for (auto ov : overlayImgs)
        availableLayers.push_back(ov.layer);
    for (auto ov : overlayLines)
        availableLayers.push_back(ov.layer);
    for (auto ov : overlayTriangles)
        availableLayers.push_back(ov.layer);
    for (auto ov : overlayTexts)
        availableLayers.push_back(ov.layer);
    auto result = std::unique(availableLayers.begin(), availableLayers.end());
    availableLayers.erase(result, availableLayers.end());
    std::sort(availableLayers.begin(), availableLayers.end());



    // Наложения
    glBindTexture(GL_TEXTURE_2D, overlayTexture);
    for (int curLayer : availableLayers) {

        // Наложение треугольников
        for (auto tri : overlayTriangles) {

            if (visibleOverlays == 0 && tri.showAlways == 0)
                continue;
            if (tri.layer > curLayer)
                break;
            if (tri.layer < curLayer)
                continue;

            glBegin(GL_TRIANGLES);
            glColor4f(tri.color.r, tri.color.g, tri.color.b, tri.color.a);
            MyPoint point1, point2, point3;
            if (tri.isPosBox == 1) {
                point1 = tri.point1;
                point2 = tri.point2;
                point3 = tri.point3;
                if (point1.x < 0)
                    point1.x = width() - point1.x;
                if (point1.y < 0)
                    point1.y = height() - point1.y;
                if (point2.x < 0)
                    point2.x = width() - point2.x;
                if (point2.y < 0)
                    point2.y = height() - point2.y;
                if (point3.x < 0)
                    point3.x = width() - point3.x;
                if (point3.y < 0)
                    point3.y = height() - point3.y;
            } else if (tri.isPosImage == 1) {
                point1 = o2bCoor(tri.point1);
                point2 = o2bCoor(tri.point2);
                point3 = o2bCoor(tri.point3);
            }
            glVertex2d(point1.x, point1.y);
            glVertex2d(point2.x, point2.y);
            glVertex2d(point3.x, point3.y);
            glEnd();
            glColor4f(1, 1, 1, 1);
        }




        // Наложение картинок
        double x1, x2, y1, y2;
        for (auto &oImg : overlayImgs) {

            if (visibleOverlays == 0 && oImg.showAlways == 0)
                continue;
            if (oImg.layer > curLayer)
                break;
            if (oImg.layer < curLayer)
                continue;

            if (shPrOpacity->bind()) {

                if (oImg.posBox.x != GL_NAN) {
                    x1 = oImg.posBox.x;
                    y1 = oImg.posBox.y;
                    if (x1 < 0)
                        x1 = width() + x1;
                    if (y1 < 0)
                        y1 = height() + y1;
                } else {
                    MyPoint pos = o2bCoor(oImg.posImage);
                    x1 = pos.x;
                    y1 = pos.y;
                }

                // Преобразование размера и нулевой точки
                MySize size = oImg.size;
                MyPoint nullPoint = oImg.nullPoint;
                if (size.width == GL_NAN) {
                    size = MySize(oImg.img->width, oImg.img->height);
                } else {
                    nullPoint.x = oImg.nullPoint.x * size.width / oImg.img->width;
                    nullPoint.y = oImg.nullPoint.y * size.height / oImg.img->height;
                }

                if (oImg.isScalable == 1) {
                    size.width *= currentZoom;
                    size.height *= currentZoom;
                    x1 -= currentZoom * nullPoint.x;
                    y1 -= currentZoom * nullPoint.y;
                } else {
                    x1 -= nullPoint.x;
                    y1 -= nullPoint.y;
                }

                x2 = x1 + size.width;
                y2 = y1 + size.height;

                oImg.x1 = x1;
                oImg.x2 = x2;
                oImg.y1 = y1;
                oImg.y2 = y2;


                // Расчёт перечения картинки и поля зрения
                PairPoint a = PairPoint(x1, y1, x2, y2);
                PairPoint b = PairPoint(0, 0, width(), height());
                bool rectangle = (
                            (
                                (
                                    ( a.x1()>=b.x1() && a.x1()<=b.x2() )||( a.x2()>=b.x1() && a.x2()<=b.x2()  )
                                    ) && (
                                    ( a.y1()>=b.y1() && a.y1()<=b.y2() )||( a.y2()>=b.y1() && a.y2()<=b.y2() )
                                    )
                                )||(
                                (
                                    ( b.x1()>=a.x1() && b.x1()<=a.x2() )||( b.x2()>=a.x1() && b.x2()<=a.x2()  )
                                    ) && (
                                    ( b.y1()>=a.y1() && b.y1()<=a.y2() )||( b.y2()>=a.y1() && b.y2()<=a.y2() )
                                    )
                                )
                            )||(
                            (
                                (
                                    ( a.x1()>=b.x1() && a.x1()<=b.x2() )||( a.x2()>=b.x1() && a.x2()<=b.x2()  )
                                    ) && (
                                    ( b.y1()>=a.y1() && b.y1()<=a.y2() )||( b.y2()>=a.y1() && b.y2()<=a.y2() )
                                    )
                                )||(
                                (
                                    ( b.x1()>=a.x1() && b.x1()<=a.x2() )||( b.x2()>=a.x1() && b.x2()<=a.x2()  )
                                    ) && (
                                    ( a.y1()>=b.y1() && a.y1()<=b.y2() )||( a.y2()>=b.y1() && a.y2()<=b.y2() )
                                    )
                                )
                            );


                // Отрисовка происходит, если картинка в зоне видимости
                if (rectangle) {
                    shPrOpacity->setUniformValue("image", 0);
                    shPrOpacity->setUniformValue("opacity", oImg.opacity);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, oImg.isSmoothing ? GL_LINEAR : GL_NEAREST);
                    glTexImage2D(GL_TEXTURE_2D, 0, oImg.img->channels,
                                 oImg.img->width,
                                 oImg.img->height,
                                 0,
                                 oImg.img->channels == 4 ? GL_RGBA : GL_RGB,
                                 GL_UNSIGNED_BYTE,
                                 *oImg.img->data);
                    setQuadsPoligon(x1, y1, x2, y2);
                }

                shPrOpacity->release();
            }

        }




        // Наложение линий
        for (auto &line : overlayLines) {

            if (visibleOverlays == 0 && line.showAlways == 0)
                continue;
            if (line.layer > curLayer)
                break;
            if (line.layer < curLayer)
                continue;

            // Сглаживание
            if (line.isSmoothing) {
                glEnable(GL_LINE_SMOOTH);
                glHint(GL_LINE_SMOOTH, GL_NICEST);
            }

            glLineWidth(line.lineWidth);
            glBegin(GL_LINES);
            glColor4f(line.color.r, line.color.g, line.color.b, line.color.a);

            if (line.posBox.first.x != GL_NAN) {
                x1 = line.posBox.first.x;
                y1 = line.posBox.first.y;
                x2 = line.posBox.second.x;
                y2 = line.posBox.second.y;
                if (x1 < 0)
                    x1 = width() + x1;
                if (x2 < 0)
                    x2 = width() + x2;
                if (y1 < 0)
                    y1 = height() + y1;
                if (y2 < 0)
                    y2 = height() + y2;
            } else {
                MyPoint fP = o2bCoor(line.posImage.first);
                MyPoint sP = o2bCoor(line.posImage.second);
                x1 = fP.x;
                y1 = fP.y;
                x2 = sP.x;
                y2 = sP.y;
            }

            line.x1 = x1;
            line.x2 = x2;
            line.y1 = y1;
            line.y2 = y2;

            glVertex2f(x1, y1);
            glVertex2f(x2, y2);
            glEnd();

            for (auto chLine : line.childLines) {
                glLineWidth(chLine.lineWidth);
                glBegin(GL_LINES);
                glColor4f(chLine.color.r, chLine.color.g, chLine.color.b, chLine.color.a);
                glLineWidth(chLine.lineWidth);
                glVertex2f(x1 + chLine.offsetCoor.first.x, y1 + chLine.offsetCoor.first.y);
                glVertex2f(x2 + chLine.offsetCoor.second.x, y2 + chLine.offsetCoor.second.y);
                glEnd();
            }

            if (line.isSmoothing)
                glDisable(GL_LINE_SMOOTH);
            glColor4f(1, 1, 1, 1);
        }







        // Наложение текста
        for (auto oText : overlayTexts) {

            if (oText.text.isEmpty())
                continue;

            if (visibleOverlays == 0 && oText.showAlways == 0)
                continue;
            if (oText.layer > curLayer)
                break;
            if (oText.layer < curLayer)
                continue;

            MyPoint coor; // Текущая координата
            MyPoint nullPoint = oText.nullPoint; // Смещение относительно нулевой координаты
            MySize size; // Размер одного символа
            int wordWidth; // Размер всего слова

            // Определение начальных координат
            if (oText.posBox.x != GL_NAN) {
                MyPoint ind(0, 0);
                if (oText.isIncludeIndent == 1)
                    ind = indent;
                if (oText.posBox.x >= 0)
                    coor.x = oText.posBox.x + ind.x;
                else
                    coor.x = width() + oText.posBox.x - ind.x;
                if (oText.posBox.y >= 0)
                    coor.y = oText.posBox.y + ind.y;
                else
                    coor.y = height() + oText.posBox.y - ind.y;
            } else if (oText.posImage.x != GL_NAN)
                coor = o2bCoor(oText.posImage);



            // Создаем QFont и настраиваем его размер, стиль и название шрифта
            QFont font(oText.fontName);
            font.setBold(oText.bold);
            QFontMetricsF fm(font);



            // Определение размера текста
            if (oText.width != GL_NAN && oText.height != GL_NAN) {
                wordWidth = oText.width;
                size.width = wordWidth / oText.text.size();
                size.height = oText.height;
            } else if (oText.width == GL_NAN && oText.height != GL_NAN) {
                size.height = oText.height;
                font.setPixelSize(size.height);
                fm = QFontMetricsF(font); // Обновляем QFontMetricsF с учетом нового размера шрифта

                wordWidth = 0;
                for (auto symbol : oText.text) {
                    int symbolWidth = fm.horizontalAdvance(symbol);
                    wordWidth += symbolWidth;
                }
                size.width = wordWidth / oText.text.size();
            } else if (oText.width != GL_NAN && oText.height == GL_NAN) {
                wordWidth = oText.width;
                size.width = wordWidth / oText.text.size();

                // Найдем масштабный коэффициент для размера шрифта
                int totalWidth = 0;
                for (int i = 0; i < oText.text.size(); i++) {
                    QString symbol = QString(QChar(oText.text.at(i)));
                    int symbolWidth = fm.horizontalAdvance(symbol);
                    totalWidth += symbolWidth;
                }
                float scaleFactor = static_cast<float>(wordWidth) / static_cast<float>(totalWidth);
                font.setPixelSize(static_cast<int>(fm.height() * scaleFactor));
                fm = QFontMetricsF(font); // Обновляем QFontMetricsF с учетом нового размера шрифта
                size.height = fm.height();
            }



            // Определение смещения
            nullPoint.x = wordWidth * oText.nullPoint.x;
            nullPoint.y = size.height * oText.nullPoint.y;


            // Сдвиг начальной координаты и скейл размера
            if (oText.isScalable) {
                size.width *= currentZoom;
                size.height *= currentZoom;
                coor.x -= currentZoom * nullPoint.x;
                coor.y -= currentZoom * nullPoint.y;
            } else {
                coor.x -= nullPoint.x;
                coor.y -= nullPoint.y;
            }


            // Фон
            if (oText.isEnableBackGround == 1) {
                MyPoint coorRectangle1 = coor - 1;
                MyPoint coorRectangle2 = MyPoint(coor.x + wordWidth, coor.y + size.height) + 1;

                glColor4f(0.5f, 0.5f, 0.5f, 0.75f);
                glRectd(coorRectangle1.x, coorRectangle1.y, coorRectangle2.x, coorRectangle2.y);
                glColor4f(1, 1, 1, 1);
            }



            // Сохраняем состояние OpenGL
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            // Создаем объект QPainter для отрисовки текста
            QPainter painter(this);
            painter.setFont(font);
            painter.setRenderHint(QPainter::Antialiasing);

            // Устанавливаем прозрачность текста и его цвет
            QColor textColor = oText.color;
            textColor.setAlphaF(oText.opacity);
            painter.setPen(textColor);

            // Определяем координаты начала текста
            QPoint textStartPoint(coor.x, height() - coor.y - size.height + fm.ascent());

            // Рисуем текст
            for (int i = 0; i < oText.text.size(); i++) {
                QString symbol = QString(QChar(oText.text.at(i)));
                int symbolWidth = fm.horizontalAdvance(symbol);

                painter.drawText(textStartPoint, symbol);
                textStartPoint.setX(textStartPoint.x() + symbolWidth);
            }

            // Удаляем объект QPainter
            painter.end();

            // Восстанавливаем состояние OpenGL
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glPopAttrib();

        }

    }
    glBindTexture(GL_TEXTURE_2D, 0);

    if (isZooming)
        emit resizeSignal();
    else if (isResizing > 0) {
        isResizing--;
        emit resizeSignal();
    }

}









void GLgraphics::imgUpdate(ImageSP_t *image) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (*image == nullptr)
        return;

    if (framImg == nullptr)
        isResizing = 1;
    else
        if (framImg->width != image->get()->width ||
                framImg->height != image->get()->height)
            isResizing = 1;

    this->framImg.reset();
    this->framImg = *image;

}









void GLgraphics::addOverlayImg(OverlayImg oImg) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (oImg.name.isEmpty()) {
        qDebug() << "Не задано имя";
        return;
    }
    if ((oImg.posBox.x == GL_NAN && oImg.posImage.x == GL_NAN) ||
            (oImg.posBox.x != GL_NAN && oImg.posImage.x != GL_NAN)) {
        qDebug() << "Координатная неопределённость" << oImg.name;
        return;
    }


    // Добавление оригинальной структуры
    auto nameMatchOr = std::find_if(sourceOverlayImgs.begin(), sourceOverlayImgs.end(), [oImg](const OverlayImg &Ov){
        return Ov.name == oImg.name;
    });
    if (nameMatchOr != sourceOverlayImgs.end()) {
        *nameMatchOr = oImg;
    } else {
        sourceOverlayImgs.emplace_back(oImg);
    }


    // Если такое имя уже есть, то оно заменяется
    bool isSortNeeded = 0;
    auto nameMatch = std::find_if(overlayImgs.begin(), overlayImgs.end(), [oImg](const OverlayImg &Ov){
        return Ov.name == oImg.name;
    });
    if (nameMatch != overlayImgs.end()) {
        if (nameMatch->layer != oImg.layer)
            isSortNeeded = 1;
        *nameMatch = oImg;
    } else {
        if (overlayImgs.size() > 0)
            if (overlayImgs.back().layer > oImg.layer)
                isSortNeeded = 1;
        overlayImgs.emplace_back(oImg);
    }

    // Сортировка по номеру слоя
    if (isSortNeeded == 1) {
        std::sort(overlayImgs.begin(), overlayImgs.end(), [](const OverlayImg &v1, const OverlayImg &v2){
            return v1.layer < v2.layer;
        });
    }

}

bool GLgraphics::removeOverlayImg(QString name) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    auto searchResult = std::find_if(overlayImgs.begin(), overlayImgs.end(), [name](const OverlayImg &Ov){
        return Ov.name == name;
    });
    if (searchResult != overlayImgs.end()) {
        overlayImgs.erase(searchResult);
        auto searchResultOr = std::find_if(sourceOverlayImgs.begin(), sourceOverlayImgs.end(), [name](const OverlayImg &Ov){
            return Ov.name == name;
        });
        sourceOverlayImgs.erase(searchResultOr);
        return 1;
    } else
        return 0;
}







void GLgraphics::addOverlayLine(OverlayLine line) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (line.name.isEmpty()) {
        qDebug() << "Не задано имя";
        return;
    }
    if ((line.posBox.first.x == GL_NAN && line.posImage.first.x == GL_NAN) ||
            (line.posBox.first.x != GL_NAN && line.posImage.first.x != GL_NAN)) {
        qDebug() << "Координатная неопределённость" << line.name;
        return;
    }


    // Добавление оригинальной структуры
    auto nameMatchOr = std::find_if(sourceOverlayLines.begin(), sourceOverlayLines.end(), [line](const OverlayLine &Ov){
        return Ov.name == line.name;
    });
    if (nameMatchOr != sourceOverlayLines.end()) {
        *nameMatchOr = line;
    } else {
        sourceOverlayLines.emplace_back(line);
    }


    bool isSortNeeded = 0;
    auto nameMatch = std::find_if(overlayLines.begin(), overlayLines.end(), [line](const OverlayLine &Ov){
        return Ov.name == line.name;
    });
    if (nameMatch != overlayLines.end()) {
        if (nameMatch->layer != line.layer)
            isSortNeeded = 1;
        *nameMatch = line;
    } else {
        if (overlayLines.size() > 0)
            if (overlayLines.back().layer > line.layer)
                isSortNeeded = 1;
        overlayLines.emplace_back(line);
    }

    // Сортировка по номеру слоя
    if (isSortNeeded == 1) {
        std::sort(overlayLines.begin(), overlayLines.end(), [](const OverlayLine &v1, const OverlayLine &v2){
            return v1.layer < v2.layer;
        });
    }

}

bool GLgraphics::removeOverlayLine(QString name) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    auto searchResult = std::find_if(overlayLines.begin(), overlayLines.end(), [name](const OverlayLine &Ov){
        return Ov.name == name;
    });
    if (searchResult != overlayLines.end()) {
        overlayLines.erase(searchResult);
        auto searchResultOr = std::find_if(sourceOverlayLines.begin(), sourceOverlayLines.end(), [name](const OverlayLine &Ov){
            return Ov.name == name;
        });
        sourceOverlayLines.erase(searchResultOr);
        return 1;
    } else
        return 0;
}








void GLgraphics::addOverlayTriangle(OverlayTriangle tri) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (tri.name.isEmpty()) {
        qDebug() << "Не задано имя";
        return;
    }
    if ((tri.isPosBox == 0 && tri.isPosImage == 0) ||
            (tri.isPosBox == 1 && tri.isPosImage == 1)) {
        qDebug() << "Координатная неопределённость" << tri.name;
        return;
    }


    // Добавление оригинальной структуры
    auto nameMatchOr = std::find_if(sourceOverlayTriangles.begin(), sourceOverlayTriangles.end(), [tri](const OverlayTriangle &Ov){
        return Ov.name == tri.name;
    });
    if (nameMatchOr != sourceOverlayTriangles.end()) {
        *nameMatchOr = tri;
    } else {
        sourceOverlayTriangles.emplace_back(tri);
    }


    bool isSortNeeded = 0;
    auto nameMatch = std::find_if(overlayTriangles.begin(), overlayTriangles.end(), [tri](const OverlayTriangle &Ov){
        return Ov.name == tri.name;
    });
    if (nameMatch != overlayTriangles.end()) {
        if (nameMatch->layer != tri.layer)
            isSortNeeded = 1;
        *nameMatch = tri;
    } else {
        if (overlayTriangles.size() > 0)
            if (overlayTriangles.back().layer > tri.layer)
                isSortNeeded = 1;
        overlayTriangles.emplace_back(tri);
    }

    // Сортировка по номеру слоя
    if (isSortNeeded == 1) {
        std::sort(overlayTriangles.begin(), overlayTriangles.end(), [](const OverlayTriangle &v1, const OverlayTriangle &v2){
            return v1.layer < v2.layer;
        });
    }

}

bool GLgraphics::removeOverlayTriangle(QString name) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    auto searchResult = std::find_if(overlayTriangles.begin(), overlayTriangles.end(), [name](const OverlayTriangle &Ov){
        return Ov.name == name;
    });
    if (searchResult != overlayTriangles.end()) {
        overlayTriangles.erase(searchResult);
        auto searchResultOr = std::find_if(sourceOverlayTriangles.begin(), sourceOverlayTriangles.end(), [name](const OverlayTriangle &Ov){
            return Ov.name == name;
        });
        sourceOverlayTriangles.erase(searchResultOr);
        return 1;
    } else
        return 0;
}








void GLgraphics::addOverlayText(OverlayText oText) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (oText.name.isEmpty()) {
        qDebug() << "Не задано имя";
        return;
    }
    if (oText.width == GL_NAN && oText.height == GL_NAN) {
        qDebug() << "Не указан размер" << oText.name;
        return;
    }
    if ((oText.posBox.x == GL_NAN && oText.posImage.x == GL_NAN) ||
            (oText.posBox.x != GL_NAN && oText.posImage.x != GL_NAN)) {
        qDebug() << "Координатная неопределённость" << oText.name;
        return;
    }


    // Добавление оригинальной структуры
    auto nameMatchOr = std::find_if(sourceOverlayTexts.begin(), sourceOverlayTexts.end(), [oText](const OverlayText &Ov){
        return Ov.name == oText.name;
    });
    if (nameMatchOr != sourceOverlayTexts.end()) {
        *nameMatchOr = oText;
    } else {
        sourceOverlayTexts.emplace_back(oText);
    }


    bool isSortNeeded = 0;
    auto nameMatch = std::find_if(overlayTexts.begin(), overlayTexts.end(), [oText](const OverlayText &Ov){
        return Ov.name == oText.name;
    });
    if (nameMatch != overlayTexts.end()) {
        if (nameMatch->layer != oText.layer)
            isSortNeeded = 1;
        *nameMatch = oText;
    } else {
        if (overlayTexts.size() > 0)
            if (overlayTexts.back().layer > oText.layer)
                isSortNeeded = 1;
        overlayTexts.emplace_back(oText);
    }

    // Сортировка по номеру слоя
    if (isSortNeeded == 1) {
        std::sort(overlayTexts.begin(), overlayTexts.end(), [](const OverlayText &v1, const OverlayText &v2){
            return v1.layer < v2.layer;
        });
    }

}

bool GLgraphics::removeOverlayText(QString name) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    auto searchResult = std::find_if(overlayTexts.begin(), overlayTexts.end(), [name](const OverlayText &Ov){
        return Ov.name == name;
    });
    if (searchResult != overlayTexts.end()) {
        overlayTexts.erase(searchResult);
        auto searchResultOr = std::find_if(sourceOverlayTexts.begin(), sourceOverlayTexts.end(), [name](const OverlayText &Ov){
            return Ov.name == name;
        });
        sourceOverlayTexts.erase(searchResultOr);
        return 1;
    } else
        return 0;
}






// Выдать накладываемые объекты
std::vector<OverlayImg> GLgraphics::getOverlayImgs() {
    return sourceOverlayImgs;
}
std::vector<OverlayLine> GLgraphics::getOverlayLines() {
    return sourceOverlayLines;
}
std::vector<OverlayTriangle> GLgraphics::getOverlayTriangles() {
    return sourceOverlayTriangles;
}
std::vector<OverlayText> GLgraphics::getOverlayTexts() {
    return sourceOverlayTexts;
}








// Найти накладываемый объект по имени (если не нашёл, то выдаст с пустым именем)
OverlayImg GLgraphics::getOverlayImg(QString name) {
    auto searchResult = std::find_if(sourceOverlayImgs.begin(), sourceOverlayImgs.end(), [name](const OverlayImg &Ov){
        return Ov.name == name;
    });
    if (searchResult == sourceOverlayImgs.end()) {
        OverlayImg nullImg;
        nullImg.name = "";
        return nullImg;
    } else
        return *searchResult;
}
OverlayLine GLgraphics::getOverlayLine(QString name) {
    auto searchResult = std::find_if(sourceOverlayLines.begin(), sourceOverlayLines.end(), [name](const OverlayLine &Ov){
        return Ov.name == name;
    });
    if (searchResult == sourceOverlayLines.end()) {
        OverlayLine nullLine;
        nullLine.name = "";
        return nullLine;
    } else
        return *searchResult;
}
OverlayTriangle GLgraphics::getOverlayTriangle(QString name) {
    auto searchResult = std::find_if(sourceOverlayTriangles.begin(), sourceOverlayTriangles.end(), [name](const OverlayTriangle &Ov){
        return Ov.name == name;
    });
    if (searchResult == sourceOverlayTriangles.end()) {
        OverlayTriangle nullTri;
        nullTri.name = "";
        return nullTri;
    } else
        return *searchResult;
}
OverlayText GLgraphics::getOverlayText(QString name) {
    auto searchResult = std::find_if(sourceOverlayTexts.begin(), sourceOverlayTexts.end(), [name](const OverlayText &Ov){
        return Ov.name == name;
    });
    if (searchResult == sourceOverlayTexts.end()) {
        OverlayText nullText;
        nullText.name = "";
        return nullText;
    } else
        return *searchResult;
}









// Удалить все наложения
void GLgraphics::clearOverlays() {
    std::unique_lock<std::recursive_mutex> un(paintMtx);
    overlayImgs.clear();
    overlayLines.clear();
    overlayTexts.clear();
    overlayTriangles.clear();
}







// Цифрозум по кнопкам
void GLgraphics::keyPressEvent(QKeyEvent *event) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (framImg == nullptr && isFillMode == 0)
        return;

    if (isWheelScale == 1) {
        if(event->key() == Qt::Key_Up) {
            wheelSpinsState = 1;
            emit updateGL();
        } else if(event->key() == Qt::Key_Down) {
            wheelSpinsState = -1;
            emit updateGL();
        }
    }
}







// Колёсико мыши
void GLgraphics::wheelEvent(QWheelEvent* event) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (framImg == nullptr && isFillMode == 0)
        return;

    if (isWheelScale == 1) {
        QPoint Wheel = event->angleDelta() / 8;
        if (Wheel.y() > 0)
            wheelSpinsState = 1;
        if (Wheel.y() < 0)
            wheelSpinsState = -1;
        emit updateGL();
    }
}







// Позиция мыши
void GLgraphics::mouseMoveEvent(QMouseEvent *event) {

    std::unique_lock<std::recursive_mutex> un(paintMtx);

    if (framImg == nullptr && isFillMode == 0)
        return;

    setFocus();
    int x = event->pos().x();
    int y = event->pos().y();
    y = height() - 1 - y;
    cursorPosition = MyPoint(x, y);
    if (rightButton == 1)
        emit updateGL();

    // Определение не находится ли курсор над изображением, если включен трекинг
    if (isTrackImage) {

        int available = 0;
        for (auto &img : overlayImgs) {
            if (pointInAreaBetweenPoints(cursorPosition, PairPoint(img.x1, img.y1, img.x2, img.y2))) {
                emit cursorIsOverImage(img.name);
                available = 1;
                break;
            }
        }
        if (available == 0)
            emit cursorIsOverImage("");
    }




    // Определение не находится ли курсор над линией, если включен трекинг
    if (isTrackLine) {
        int available = 0;

        for (auto &line : overlayLines) {

            if (pointInAreaBetweenPoints(cursorPosition, PairPoint(line.x1, line.y1, line.x2, line.y2))) {
                emit cursorIsOverLine(line.name);
                available = 1;
                break;
            }
            for (auto chLine : line.childLines) {
                if (pointInAreaBetweenPoints(cursorPosition, PairPoint(line.x1 + chLine.offsetCoor.first.x,
                                                                       line.y1 + chLine.offsetCoor.first.y,
                                                                       line.x2 + chLine.offsetCoor.second.x,
                                                                       line.y2 + chLine.offsetCoor.second.y))) {
                    emit cursorIsOverLine(line.name);
                    available = 1;
                    break;
                }
            }
        }
        if (available == 0)
            emit cursorIsOverLine("");
    }

}





