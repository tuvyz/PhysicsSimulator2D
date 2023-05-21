#ifndef OPENGL_H
#define OPENGL_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMessageBox>
#include <QResource>
#include <global.h>
#include <ffclasses.h>



namespace gl {


struct Vec4 {
    Vec4(){}
    Vec4(float r, float g, float b, float a = 1) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }
    float r, g, b, a;
};




#define GL_NAN 2111111111 // Условно неопределённое значение



// Накладываемая картинка
struct OverlayImg {
    QString name; // По имени её потом можно будет удалить
    GLuint textureId = GL_NAN; // Заполнять с помощью метода createTexture
    // Заполнять только один из двух:
    MyPoint posBox = MyPoint(GL_NAN, GL_NAN); // Привязывается к координатам рамки. Если число положительное, то отсчитывается
                                              // от нуля. Если отрицательное, то это число вычитается из ширины/длины виджета
    MyPoint posImage = MyPoint(GL_NAN, GL_NAN); // Координаты будут привязаны к координатам картинки
    // Если выбран posImage:
    bool isScalable = 1; // Масштабирование вместе с картинкой
    MySize size = MySize(GL_NAN, GL_NAN); // Если указан, то будет растянут под заданный размер, иначе берётся размер изображения
    MyPoint nullPoint = MyPoint(0, 0); // Смещение вставляемой картинки относительно нуля для вставки (Всегда для оригинального размера)
    bool isSmoothing = 0; // Сглаживание
    float opacity = 1.0f; // Прозрачность
    int layer = 0; // Слой
    bool showAlways = 0; // Если активно, то игнорируется скрытие наложений
    double x1, y1, x2, y2; // Это не заполнять
};

// Накладываемая линия
struct OverlayLine {
    QString name;
    // Заполнять только один из двух:
    PairPoint posBox = PairPoint(MyPoint(GL_NAN, GL_NAN), MyPoint(GL_NAN, GL_NAN)); // Привязывается к координатам рамки. Если число положительное, то отсчитывается
                                                                                    // от нуля. Если отрицательное, то это число вычитается из ширины/длины виджета
    PairPoint posImage = PairPoint(MyPoint(GL_NAN, GL_NAN), MyPoint(GL_NAN, GL_NAN)); // Координаты будут привязаны к координатам картинки
    float lineWidth = 1;
    Vec4 color = Vec4(1, 1, 1, 1);
    // Связанные линии (например, если надо сделать тень)
    struct ChildLine {
        ChildLine(){}
        ChildLine(PairPoint offsetCoor, Vec4 color, float lineWidth = 1) {
            this->offsetCoor = offsetCoor;
            this->color = color;
            this->lineWidth = lineWidth;
        }
        PairPoint offsetCoor; // Координаты связанных линий представляются как координаты смещения относительно родительской линии
        Vec4 color = Vec4(1, 1, 1, 1);
        float lineWidth = 1;
    };
    vector<ChildLine> childLines;
    int layer = 0; // Слой
    bool isSmoothing = 0;
    bool showAlways = 0; // Если активно, то игнорируется скрытие наложений
    double x1, y1, x2, y2; // Это не заполнять
};

// Накладываемый треугольник
struct OverlayTriangle {
    QString name;
    Vec4 color = Vec4(1, 1, 1, 1);
    MyPoint point1, point2, point3;
    // Выбрать одно из двух:
    bool isPosBox = 0; // Привязывается к координатам рамки. Если число положительное, то отсчитывается
                       // от нуля. Если отрицательное, то это число вычитается из ширины/длины виджета
    bool isPosImage = 0; // Указанные координаты принадлежат изображению
    int layer = 0; // Слой
    bool isSmoothing = 0;
    bool showAlways = 0; // Если активно, то игнорируется скрытие наложений
};

// Накладываемый текст
struct OverlayText {
    QString name;
    QString text;
    bool bold = true;
    QColor color = QColor(0, 0, 0);
    QString fontName = "Arial";
    bool isEnableBackGround = 0;
    float opacity = 1.0f; // Прозрачность
    // Заполнять только один из двух:
    MyPoint posBox = MyPoint(GL_NAN, GL_NAN); // Привязывается к координатам рамки. Если число положительное, то отсчитывается
                                              // от нуля. Если отрицательное, то это число вычитается из ширины/длины виджета
    bool isIncludeIndent = 0; // Относится только к posBox. Если true, то будет рисоваться только на видимой части картинки, не включая отступы
    MyPoint posImage = MyPoint(GL_NAN, GL_NAN); // Координаты будут привязаны к координатам картинки
    // Нужно указать хотя бы одно значение (Если указаны оба текст может быть растянут):
    int width = GL_NAN; // Ширина будет постоянной
    int height = GL_NAN; // Высота будет постоянной
    // Центр смещения будет принят за процентное соотношение (от 0 до 1):
    MyPoint nullPoint = MyPoint(0, 0); // Смещение вставляемого текста относительно нуля для вставки
    bool isScalable = 1; // Масштабирование вместе с картинкой
    int layer = 0; // Слой
    bool isSmoothing = 0;
    bool showAlways = 0; // Если активно, то игнорируется скрытие наложений
};






class GLgraphics : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLgraphics(QWidget *parent = nullptr);

    ~GLgraphics() override {

    }

    GLuint createTexture(const QImage &image); // Создаёт текстуру из изображения


    bool isInit() {
        return isInitialize;
    }

protected:

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:

    std::shared_ptr<QOpenGLShaderProgram> shPrOpacity;

    std::shared_ptr<QOpenGLTexture> frameTex;

    std::optional<GLuint> framImg;
    MySize frameSize;
    std::recursive_mutex paintMtx;

    void setQuadsPoligon(GLint bl_x, GLint bl_y, GLint tp_x, GLint tp_y);


    // Это то, что занесено с помощью add
    std::vector<OverlayImg> sourceOverlayImgs;
    std::vector<OverlayLine> sourceOverlayLines;
    std::vector<OverlayTriangle> sourceOverlayTriangles;
    std::vector<OverlayText> sourceOverlayTexts;

    // Сюда могут вносится изменения
    std::vector<OverlayImg> overlayImgs;
    std::vector<OverlayLine> overlayLines;
    std::vector<OverlayTriangle> overlayTriangles;
    std::vector<OverlayText> overlayTexts;

    std::vector<int> availableLayers;

    bool isInitialize = 0;

    bool visibleOverlays = 1;

    bool isUpdateGL = 0; // При вызове updateGL становится в 1, а когда заканчивается
                         // выполнение paintGL ставится в 0


    int wheelSpinsState = 0;
    bool isSmoothing = 1;
    bool isTrackImage = 0;
    bool isTrackLine = 0;
    Vec4 bgColor = Vec4(0, 0, 0, 1);;


    // Для ресайза
    bool isWheelScale = 0; // По умолчанию скейл по колёсику мыши отключен
    bool rightButton = 0;
    atomic_int isResizing;
    float zoom = 1;
    double currentZoom = 1;
    MyPoint nullPointToZoom; // Локальная координата (0, 0) масштабирования
    MyPoint anchorPointToMove = MyPoint(-1, -1); // Координата для перемещения изображения курсором
    MyPoint indent; // Отступы от границ
    MySize resizeSize;
    MySize originalSize;
    unsigned char colorFields = 0;
    MyPoint cursorPosition;


    // Для режима без основного изображения
    bool isFillMode = 0;
    bool isFillModeInit = 0;
    MySize fillSize;


    bool isCenterViewEnable = 0;
    MyPoint centerView;

public:

    virtual void reset();

    void imgUpdate(GLuint, MySize);

    bool getIsUpdateGL() {
        return isUpdateGL;
    }

    // Отключить наложения (кроме тех, у которых стоит соответствующий флаг)
    void setVisibleOverlays(bool b) {
        visibleOverlays = b;
    }

    void addOverlayImg(OverlayImg);
    void addOverlayLine(OverlayLine);
    void addOverlayTriangle(OverlayTriangle);
    void addOverlayText(OverlayText);

    void addOverlayImg_Fast(OverlayImg); // Использовать только если важна скорость обработки. Не делает
                                         // никаких проверок и работает только если слой всего один

    bool removeOverlayImg(QString name);
    bool removeOverlayLine(QString name);
    bool removeOverlayTriangle(QString name);
    bool removeOverlayText(QString name);

    void clearOverlays();

    std::vector<OverlayImg> getOverlayImgs();
    std::vector<OverlayLine> getOverlayLines();
    std::vector<OverlayTriangle> getOverlayTriangles();
    std::vector<OverlayText> getOverlayTexts();

    OverlayImg getOverlayImg(QString name);
    OverlayLine getOverlayLine(QString name);
    OverlayTriangle getOverlayTriangle(QString name);
    OverlayText getOverlayText(QString name);

    double getZoom() {
        return currentZoom;
    }

    MySize getImgSize() {
        if (!framImg.has_value()) {
            return MySize(-1, -1);
        } else {
            return frameSize;
        }
    }




    // ФЛАГИ
    // Включить режим поля
    void setFillMode(MySize fillSize) {
        this->isFillMode = 1;
        isFillModeInit = 1;
        this->fillSize = fillSize;
    }
    // Вкл/выкл сглажививание
    void setSmoothing(bool isSmoothing) {
        this->isSmoothing = isSmoothing;
    }
    // Вкл/выкл приближение по колёсику мыши
    void setWheelScale(bool isWheelScale) {
        this->isWheelScale = isWheelScale;
        zoom = 1;
        emit updateGL();
    }
    // Задать цвет фона
    void setBackgroundColor(Vec4 color) {
        std::unique_lock<std::recursive_mutex> un(paintMtx);
        bgColor = color;
    }
    // Будет отправляться сигнал "cursorIsOverImage(QString)" когда курсор будет находится над наложенными изображением
    void trackOverlayAllImgs() {
        isTrackImage = 1;
    }
    // Будет отправляться сигнал "cursorIsOverLine(QString)" когда курсор будет находится нам наложенными линиями
    void trackOverlayAllLines() {
        isTrackLine = 1;
    }



    // Преобразование координат из реальных и рамочных на ui
    MyPoint o2bCoor (MyPoint originalPoint) {
        std::unique_lock<std::recursive_mutex> un(paintMtx);
        return (originalPoint - nullPointToZoom) * currentZoom + indent;
    }
    MyPoint b2oCoor (MyPoint labelPoint, bool allowOutOfIndent = 0) {
        std::unique_lock<std::recursive_mutex> un(paintMtx);
        labelPoint = labelPoint - indent;
        MyPoint point = nullPointToZoom + labelPoint / currentZoom;
        if (allowOutOfIndent == 0) {
            point.x = qBound<double>(0, point.x, originalSize.width - 1);
            point.y = qBound<double>(0, point.y, originalSize.height - 1);
        }
        return point;
    }

    MyPoint getCursorPos() {
        return cursorPosition;
    }
    MyPoint getCursorPosOrig() {
        return b2oCoor(cursorPosition);
    }

    MyPoint getIndent() {
        return indent;
    }

    // Будет центрировано в выбранной точке
    void setCenterView(MyPoint centerView) {
        isCenterViewEnable = 1;
        this->centerView = centerView;
    }




protected slots:

    void wheelEvent(QWheelEvent* event) override;

    virtual void keyPressEvent(QKeyEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override {
        if(event->button() == Qt::RightButton)
            rightButton = 1;
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button() == Qt::RightButton) {
            rightButton = 0;
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override;

    virtual void initSlot();


signals:

    void initSignal();

    void updateGL();

    void setEnabledSig(bool);

    void resizeSignal();

    void cursorIsOverImage(QString name);
    void cursorIsOverLine(QString name);

};

}

#endif // OPENGL_H
