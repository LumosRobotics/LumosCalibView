#include "plot_view.h"
#include <QDebug>
#include <QPaintEvent>
#include <QThread>
#include <cmath>

PlotView::PlotView(QWidget *parent)
    : QOpenGLWidget(parent), m_shaderProgram(nullptr), m_plotMode(PLOT_3D), m_showGrid(true), m_showAxes(true), m_zoom(1.0f), m_panOffset(0.0f, 0.0f, 0.0f), m_projectionMode(PERSPECTIVE_PROJECTION), m_fov(45.0f), m_mousePressed(false), m_interactionMode(ROTATE_MODE), m_animationTime(0.0f), m_dataReceiver(nullptr), m_dataThread(nullptr), m_realTimeMode(false), m_maxRealTimePoints(1000)
{
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &PlotView::updateAnimation);
    m_animationTimer->start(16); // ~60 FPS

    // Initialize view angles for a nice 3D perspective
    m_viewAngles.setAngles(M_PI / 4, M_PI / 6);

    // Enable keyboard focus for key events
    setFocusPolicy(Qt::StrongFocus);
}

PlotView::~PlotView()
{
    stopDataReceiver();

    makeCurrent();
    delete m_shaderProgram;
    doneCurrent();
}

void PlotView::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.5f);
    glPointSize(3.0f);

    setupShaders();
    setupBuffers();
    createGridData();
    createAxisData();
}

void PlotView::setupShaders()
{
    // Vertex shader
    const char *vertexShaderSource = R"(
        attribute vec3 aPosition;
        attribute vec3 aColor;
        
        uniform mat4 uMVPMatrix;
        
        varying vec3 vColor;
        
        void main() {
            gl_Position = uMVPMatrix * vec4(aPosition, 1.0);
            vColor = aColor;
        }
    )";

    // Fragment shader
    const char *fragmentShaderSource = R"(
        varying vec3 vColor;
        
        void main() {
            gl_FragColor = vec4(vColor, 1.0);
        }
    )";

    m_shaderProgram = new QOpenGLShaderProgram(this);
    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_shaderProgram->link();
}

void PlotView::setupBuffers()
{
    // Main plot data VAO
    m_vao.create();
    m_vertexBuffer.create();
    m_indexBuffer.create();

    // Grid VAO
    m_gridVAO.create();
    m_gridVertexBuffer.create();

    // Axis VAO
    m_axisVAO.create();
    m_axisVertexBuffer.create();
}

void PlotView::createGridData()
{
    m_gridVertices.clear();

    const float gridSize = 5.0f;
    const int gridLines = 21;
    const float step = gridSize * 2.0f / (gridLines - 1);

    // Grid lines parallel to X-axis
    for (int i = 0; i < gridLines; ++i)
    {
        float z = -gridSize + i * step;
        // Line from (-gridSize, 0, z) to (gridSize, 0, z)
        m_gridVertices.insert(m_gridVertices.end(), {
                                                        -gridSize, 0.0f, z, 0.4f, 0.4f, 0.4f, // start point
                                                        gridSize, 0.0f, z, 0.4f, 0.4f, 0.4f   // end point
                                                    });
    }

    // Grid lines parallel to Z-axis
    for (int i = 0; i < gridLines; ++i)
    {
        float x = -gridSize + i * step;
        // Line from (x, 0, -gridSize) to (x, 0, gridSize)
        m_gridVertices.insert(m_gridVertices.end(), {
                                                        x, 0.0f, -gridSize, 0.4f, 0.4f, 0.4f, // start point
                                                        x, 0.0f, gridSize, 0.4f, 0.4f, 0.4f   // end point
                                                    });
    }

    m_gridVAO.bind();
    m_gridVertexBuffer.bind();
    m_gridVertexBuffer.allocate(m_gridVertices.data(), m_gridVertices.size() * sizeof(float));

    int posLocation = m_shaderProgram->attributeLocation("aPosition");
    int colorLocation = m_shaderProgram->attributeLocation("aColor");

    if (posLocation >= 0)
    {
        glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(posLocation);
    }

    if (colorLocation >= 0)
    {
        glVertexAttribPointer(colorLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(colorLocation);
    }

    m_gridVAO.release();
}

void PlotView::createAxisData()
{
    m_axisVertices.clear();

    const float axisLength = 6.0f;

    // X-axis (red)
    m_axisVertices.insert(m_axisVertices.end(), {
                                                    0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,      // origin
                                                    axisLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f // end point
                                                });

    // Y-axis (green)
    m_axisVertices.insert(m_axisVertices.end(), {
                                                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,      // origin
                                                    0.0f, axisLength, 0.0f, 0.0f, 1.0f, 0.0f // end point
                                                });

    // Z-axis (blue)
    m_axisVertices.insert(m_axisVertices.end(), {
                                                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,      // origin
                                                    0.0f, 0.0f, axisLength, 0.0f, 0.0f, 1.0f // end point
                                                });

    m_axisVAO.bind();
    m_axisVertexBuffer.bind();
    m_axisVertexBuffer.allocate(m_axisVertices.data(), m_axisVertices.size() * sizeof(float));

    int posLocation = m_shaderProgram->attributeLocation("aPosition");
    int colorLocation = m_shaderProgram->attributeLocation("aColor");

    if (posLocation >= 0)
    {
        glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(posLocation);
    }

    if (colorLocation >= 0)
    {
        glVertexAttribPointer(colorLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(colorLocation);
    }

    m_axisVAO.release();
}

void PlotView::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shaderProgram->bind();

    QMatrix4x4 mvpMatrix = getProjectionMatrix() * getViewMatrix();
    m_shaderProgram->setUniformValue("uMVPMatrix", mvpMatrix);

    if (m_showGrid)
    {
        renderGrid();
    }

    if (m_showAxes)
    {
        renderAxes();
    }

    renderData();

    m_shaderProgram->release();
}

void PlotView::paintEvent(QPaintEvent *event)
{
    // First render OpenGL content
    QOpenGLWidget::paintEvent(event);

    // Then overlay text with QPainter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_showAxes)
    {
        renderAxisNumbers(painter);
    }

    // Show current interaction mode
    renderInteractionMode(painter);

    painter.end();
}

void PlotView::renderGrid()
{
    m_gridVAO.bind();
    glDrawArrays(GL_LINES, 0, m_gridVertices.size() / 6);
    m_gridVAO.release();
}

void PlotView::renderAxes()
{
    glLineWidth(3.0f);
    m_axisVAO.bind();
    glDrawArrays(GL_LINES, 0, m_axisVertices.size() / 6);
    m_axisVAO.release();
    glLineWidth(1.5f);
}

void PlotView::renderData()
{
    if (m_plotDataSeries.empty())
    {
        return;
    }

    int posLocation = m_shaderProgram->attributeLocation("aPosition");
    int colorLocation = m_shaderProgram->attributeLocation("aColor");

    for (const auto &plotData : m_plotDataSeries)
    {
        if (plotData.vertices.empty())
        {
            continue;
        }

        // Set line width for this series
        glLineWidth(plotData.lineWidth);

        m_vao.bind();
        m_vertexBuffer.bind();
        m_vertexBuffer.allocate(plotData.vertices.data(), plotData.vertices.size() * sizeof(float));

        if (posLocation >= 0)
        {
            glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(posLocation);
        }

        if (colorLocation >= 0)
        {
            glVertexAttribPointer(colorLocation, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
            glEnableVertexAttribArray(colorLocation);
        }

        if (!plotData.indices.empty())
        {
            m_indexBuffer.bind();
            m_indexBuffer.allocate(plotData.indices.data(), plotData.indices.size() * sizeof(unsigned int));
            glDrawElements(plotData.drawMode, plotData.indices.size(), GL_UNSIGNED_INT, 0);
        }
        else
        {
            glDrawArrays(plotData.drawMode, 0, plotData.vertices.size() / 6);
        }

        m_vao.release();
    }

    // Reset line width to default
    glLineWidth(1.5f);
}

QMatrix4x4 PlotView::getViewMatrix() const
{
    QMatrix4x4 view;
    view.translate(0.0f, 0.0f, -10.0f * m_zoom);

    // Apply panning
    view.translate(m_panOffset);

    if (m_plotMode == PLOT_3D)
    {
        view.rotate(m_viewAngles.getElevation() * 180.0 / M_PI, 1.0f, 0.0f, 0.0f);
        view.rotate(m_viewAngles.getAzimuth() * 180.0 / M_PI, 0.0f, 1.0f, 0.0f);
    }

    return view;
}

QMatrix4x4 PlotView::getProjectionMatrix() const
{
    QMatrix4x4 projection;
    float aspect = (float)width() / height();

    if (m_plotMode == PLOT_2D || m_projectionMode == ORTHOGRAPHIC_PROJECTION)
    {
        projection.ortho(-5.0f * aspect * m_zoom, 5.0f * aspect * m_zoom,
                         -5.0f * m_zoom, 5.0f * m_zoom, 0.1f, 100.0f);
    }
    else
    {
        projection.perspective(m_fov, aspect, 0.1f, 100.0f);
    }

    return projection;
}

void PlotView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void PlotView::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
    m_mousePressed = true;
}

void PlotView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mousePressed)
        return;

    QPoint delta = event->pos() - m_lastMousePos;

    switch (m_interactionMode)
    {
    case ROTATE_MODE:
        if (m_plotMode == PLOT_3D)
        {
            double deltaAzimuth = delta.x() * 0.01;
            double deltaElevation = delta.y() * 0.01;
            m_viewAngles.changeAnglesWithDelta(deltaAzimuth, deltaElevation);
        }
        break;

    case ZOOM_MODE:
    {
        float zoomFactor = 1.0f + (-delta.y() * 0.01f);
        m_zoom = qMax(0.1f, qMin(5.0f, m_zoom * zoomFactor));
    }
    break;

    case PAN_MODE:
    {
        float panSpeed = 0.01f * m_zoom;
        m_panOffset.setX(m_panOffset.x() + delta.x() * panSpeed);
        m_panOffset.setY(m_panOffset.y() - delta.y() * panSpeed); // Invert Y for intuitive panning
    }
    break;
    }

    m_lastMousePos = event->pos();
    update();
}

void PlotView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_mousePressed = false;
}

void PlotView::wheelEvent(QWheelEvent *event)
{
    float zoomFactor = 1.0f + (event->angleDelta().y() / 1200.0f);
    m_zoom = qMax(0.1f, qMin(5.0f, m_zoom * zoomFactor));
    update();
}

void PlotView::updateAnimation()
{
    m_animationTime += 0.016f;
    update();
}

void PlotView::setPlotData(const PlotData &data)
{
    m_plotDataSeries.clear();
    m_plotDataSeries.push_back(data);
    update();
}

void PlotView::addDataPoint(float x, float y, float z)
{
    if (m_plotDataSeries.empty())
    {
        m_plotDataSeries.resize(1);
    }

    // Add vertex with position and color to the first series
    m_plotDataSeries[0].vertices.insert(m_plotDataSeries[0].vertices.end(), {
                                                                                x, y, z, 1.0f, 1.0f, 0.0f // yellow color
                                                                            });
    m_plotDataSeries[0].drawMode = GL_POINTS;
    update();
}

void PlotView::addDataSeries(const std::vector<float> &xData, const std::vector<float> &yData,
                             const std::vector<float> &zData, float lineWidth)
{
    PlotData newSeries;
    newSeries.vertices.clear();

    size_t count = std::min(xData.size(), yData.size());
    bool hasZ = !zData.empty() && zData.size() >= count;

    for (size_t i = 0; i < count; ++i)
    {
        float z = hasZ ? zData[i] : 0.0f;
        float colorR = (float)i / count; // Gradient from red to cyan
        float colorG = 1.0f - colorR;
        float colorB = 0.8f;

        newSeries.vertices.insert(newSeries.vertices.end(), {xData[i], yData[i], z, colorR, colorG, colorB});
    }

    newSeries.drawMode = GL_LINE_STRIP;
    newSeries.lineWidth = lineWidth;

    m_plotDataSeries.push_back(newSeries);
    update();
}

void PlotView::addPlotData(const PlotData &data)
{
    m_plotDataSeries.push_back(data);
    update();
}

void PlotView::clearData()
{
    m_plotDataSeries.clear();
    update();
}

void PlotView::setPlotMode(PlotMode mode)
{
    m_plotMode = mode;
    update();
}

void PlotView::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void PlotView::setShowAxes(bool show)
{
    m_showAxes = show;
    update();
}

void PlotView::resetView()
{
    m_viewAngles.setAngles(M_PI / 4, M_PI / 6);
    m_zoom = 1.0f;
    update();
}

void PlotView::setViewAngles(double azimuth, double elevation)
{
    m_viewAngles.setAngles(azimuth, elevation);
    update();
}

void PlotView::setAxisLabels(const QString &xLabel, const QString &yLabel, const QString &zLabel)
{
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    m_zLabel = zLabel;
}

QVector3D PlotView::worldToScreen(const QVector3D &worldPos) const
{
    QMatrix4x4 mvpMatrix = getProjectionMatrix() * getViewMatrix();
    QVector4D worldPos4(worldPos, 1.0f);
    QVector4D clipPos = mvpMatrix * worldPos4;

    if (clipPos.w() == 0.0f)
    {
        return QVector3D(-1, -1, -1); // Invalid
    }

    // Perspective divide
    QVector3D ndcPos = clipPos.toVector3D() / clipPos.w();

    // Convert to screen coordinates
    float screenX = (ndcPos.x() + 1.0f) * 0.5f * width();
    float screenY = (1.0f - ndcPos.y()) * 0.5f * height(); // Flip Y axis

    return QVector3D(screenX, screenY, ndcPos.z());
}

void PlotView::renderAxisNumbers(QPainter &painter)
{
    painter.setPen(QPen(Qt::white, 1));
    painter.setFont(QFont("Arial", 9));

    const float axisLength = 6.0f;
    const int numTicks = 6;

    // X-axis numbers
    for (int i = 0; i <= numTicks; ++i)
    {
        float x = (float)i / numTicks * axisLength;
        QVector3D worldPos(x, 0.0f, 0.0f);
        QVector3D screenPos = worldToScreen(worldPos);

        if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
        { // Within view frustum
            QString text = QString::number(x, 'f', 1);
            QRect textRect = painter.fontMetrics().boundingRect(text);

            // Position text below the axis
            int textX = (int)screenPos.x() - textRect.width() / 2;
            int textY = (int)screenPos.y() + 20;

            if (textX >= 0 && textX + textRect.width() <= width() &&
                textY >= 0 && textY <= height())
            {
                painter.drawText(textX, textY, text);
            }
        }
    }

    // Y-axis numbers
    for (int i = 0; i <= numTicks; ++i)
    {
        float y = (float)i / numTicks * axisLength;
        QVector3D worldPos(0.0f, y, 0.0f);
        QVector3D screenPos = worldToScreen(worldPos);

        if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
        { // Within view frustum
            QString text = QString::number(y, 'f', 1);
            QRect textRect = painter.fontMetrics().boundingRect(text);

            // Position text to the left of the axis
            int textX = (int)screenPos.x() - textRect.width() - 10;
            int textY = (int)screenPos.y() + textRect.height() / 2;

            if (textX >= 0 && textX + textRect.width() <= width() &&
                textY >= 0 && textY <= height())
            {
                painter.drawText(textX, textY, text);
            }
        }
    }

    // Z-axis numbers (only in 3D mode)
    if (m_plotMode == PLOT_3D)
    {
        for (int i = 0; i <= numTicks; ++i)
        {
            float z = (float)i / numTicks * axisLength;
            QVector3D worldPos(0.0f, 0.0f, z);
            QVector3D screenPos = worldToScreen(worldPos);

            if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
            { // Within view frustum
                QString text = QString::number(z, 'f', 1);
                QRect textRect = painter.fontMetrics().boundingRect(text);

                // Position text to the right of the axis
                int textX = (int)screenPos.x() + 10;
                int textY = (int)screenPos.y() + textRect.height() / 2;

                if (textX >= 0 && textX + textRect.width() <= width() &&
                    textY >= 0 && textY <= height())
                {
                    painter.drawText(textX, textY, text);
                }
            }
        }
    }

    // Axis labels
    if (!m_xLabel.isEmpty())
    {
        QVector3D xLabelPos = worldToScreen(QVector3D(axisLength + 0.5f, 0.0f, 0.0f));
        if (xLabelPos.z() > -1.0f && xLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::red, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)xLabelPos.x() + 5, (int)xLabelPos.y(), m_xLabel);
        }
    }

    if (!m_yLabel.isEmpty())
    {
        QVector3D yLabelPos = worldToScreen(QVector3D(0.0f, axisLength + 0.5f, 0.0f));
        if (yLabelPos.z() > -1.0f && yLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::green, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)yLabelPos.x() + 5, (int)yLabelPos.y(), m_yLabel);
        }
    }

    if (!m_zLabel.isEmpty() && m_plotMode == PLOT_3D)
    {
        QVector3D zLabelPos = worldToScreen(QVector3D(0.0f, 0.0f, axisLength + 0.5f));
        if (zLabelPos.z() > -1.0f && zLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::blue, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)zLabelPos.x() + 5, (int)zLabelPos.y(), m_zLabel);
        }
    }
}

void PlotView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Z:
        m_interactionMode = ZOOM_MODE;
        setCursor(Qt::SizeVerCursor);
        update();
        break;
    case Qt::Key_P:
        m_interactionMode = PAN_MODE;
        setCursor(Qt::OpenHandCursor);
        update();
        break;
    case Qt::Key_R:
    case Qt::Key_Escape:
        m_interactionMode = ROTATE_MODE;
        setCursor(Qt::ArrowCursor);
        update();
        break;
    case Qt::Key_V:
        toggleProjectionMode();
        break;
    case Qt::Key_N:
        decreaseFOV();
        break;
    case Qt::Key_M:
        increaseFOV();
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        break;
    }
}

void PlotView::keyReleaseEvent(QKeyEvent *event)
{
    QOpenGLWidget::keyReleaseEvent(event);
}

void PlotView::renderInteractionMode(QPainter &painter)
{
    painter.setPen(QPen(Qt::yellow, 1));
    painter.setFont(QFont("Arial", 12, QFont::Bold));

    QString modeText;
    switch (m_interactionMode)
    {
    case ROTATE_MODE:
        modeText = "ROTATE (R) - Drag to rotate view";
        break;
    case ZOOM_MODE:
        modeText = "ZOOM (Z) - Drag up/down to zoom";
        break;
    case PAN_MODE:
        modeText = "PAN (P) - Drag to pan view";
        break;
    }

    // Add projection mode info to the mode text
    QString projectionInfo = QString(" | %1 | FOV: %2°")
        .arg(m_projectionMode == PERSPECTIVE_PROJECTION ? "Perspective" : "Orthographic")
        .arg(QString::number(m_fov, 'f', 1));
    modeText += projectionInfo;

    // Draw mode indicator in top-left corner
    QRect textRect = painter.fontMetrics().boundingRect(modeText);
    painter.fillRect(5, 5, textRect.width() + 10, textRect.height() + 6,
                     QColor(0, 0, 0, 128)); // Semi-transparent background
    painter.drawText(10, 20, modeText);

    // Show view angles when in rotate mode with left mouse button pressed
    if (m_interactionMode == ROTATE_MODE && m_mousePressed)
    {
        painter.setPen(QPen(Qt::white, 1));
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        
        QString angleText = QString("(%1°, %2°)")
            .arg(QString::number(m_viewAngles.getAzimuth() * 180.0 / M_PI, 'f', 1))
            .arg(QString::number(m_viewAngles.getElevation() * 180.0 / M_PI, 'f', 1));
        
        QRect angleRect = painter.fontMetrics().boundingRect(angleText);
        int angleX = width() - angleRect.width() - 15;
        int angleY = height() - 30;
        
        // Semi-transparent background for better readability
        painter.fillRect(angleX - 5, angleY - angleRect.height() - 5, angleRect.width() + 10, angleRect.height() + 6,
                         QColor(0, 0, 0, 128));
        painter.drawText(angleX, angleY - 5, angleText);
    }
    else
    {
        // Show keyboard shortcuts in bottom-left corner when not showing view angles
        painter.setPen(QPen(Qt::lightGray, 1));
        painter.setFont(QFont("Arial", 9));

        QStringList shortcuts = {
            "R - Rotate mode",
            "Z - Zoom mode", 
            "P - Pan mode",
            "V - Toggle projection",
            "N/M - FOV (perspective)",
            "ESC - Reset to rotate"};

        int y = height() - 100;
        for (const QString &shortcut : shortcuts)
        {
            painter.drawText(10, y, shortcut);
            y += 15;
        }
    }
}

// Real-time data methods
void PlotView::startDataReceiver(quint16 port)
{
    if (m_dataReceiver)
    {
        stopDataReceiver();
    }

    m_dataThread = new QThread(this);
    m_dataReceiver = new DataReceiver();
    m_dataReceiver->moveToThread(m_dataThread);

    // Connect signals
    connect(m_dataThread, &QThread::started, m_dataReceiver, &DataReceiver::startReceiving);
    connect(m_dataReceiver, &DataReceiver::newDataAvailable, this, &PlotView::onNewDataReceived);
    connect(m_dataReceiver, &DataReceiver::connectionStatusChanged, this, &PlotView::onDataReceiverConnected);
    connect(m_dataReceiver, &DataReceiver::errorOccurred, this, &PlotView::onDataReceiverError);

    // Start server
    m_dataReceiver->startServer(port);
    m_dataThread->start();

    qDebug() << "Started data receiver on port" << port;
}

void PlotView::stopDataReceiver()
{
    if (m_dataThread && m_dataReceiver)
    {
        m_dataReceiver->stopReceiving();
        m_dataThread->quit();
        m_dataThread->wait(3000);

        delete m_dataReceiver;
        delete m_dataThread;
        m_dataReceiver = nullptr;
        m_dataThread = nullptr;

        qDebug() << "Stopped data receiver";
    }
}

void PlotView::connectToDataSource(const QString &host, quint16 port)
{
    if (m_dataReceiver)
    {
        m_dataReceiver->connectToHost(host, port);
        qDebug() << "Connecting to data source at" << host << ":" << port;
    }
}

void PlotView::setRealTimeMode(bool enabled)
{
    m_realTimeMode = enabled;
    if (!enabled)
    {
        m_realTimeBuffer.clear();
    }
    qDebug() << "Real-time mode" << (enabled ? "enabled" : "disabled");
}

void PlotView::setMaxRealTimePoints(int maxPoints)
{
    m_maxRealTimePoints = maxPoints;

    // Trim buffer if necessary
    while (m_realTimeBuffer.size() > m_maxRealTimePoints)
    {
        m_realTimeBuffer.erase(m_realTimeBuffer.begin());
    }
}

bool PlotView::isReceivingData() const
{
    return m_dataReceiver && m_dataReceiver->isConnected();
}

void PlotView::onNewDataReceived()
{
    if (!m_realTimeMode || !m_dataReceiver)
    {
        return;
    }

    // Get latest data from receiver
    std::vector<DataPoint> newData = m_dataReceiver->getLatestData();

    // Add to real-time buffer
    for (const auto &point : newData)
    {
        m_realTimeBuffer.push_back(point);
    }

    // Limit buffer size
    while (m_realTimeBuffer.size() > m_maxRealTimePoints)
    {
        m_realTimeBuffer.erase(m_realTimeBuffer.begin());
    }

    // Update visualization
    if (!m_realTimeBuffer.empty())
    {
        // Convert to plot format
        std::vector<float> xData, yData, zData;

        for (const auto &point : m_realTimeBuffer)
        {
            xData.push_back(static_cast<float>(point.timestamp));
            yData.push_back(point.value);
            zData.push_back(static_cast<float>(point.channel)); // Use channel as Z
        }

        // Clear existing data and add real-time series
        clearData();
        addDataSeries(xData, yData, zData, 2.0f); // Thicker line for real-time data
    }

    // Clear receiver buffer to avoid accumulation
    m_dataReceiver->clearData();
}

void PlotView::onDataReceiverConnected(bool connected)
{
    qDebug() << "Data receiver connection status:" << connected;
    if (connected && !m_realTimeMode)
    {
        setRealTimeMode(true);
    }
}

void PlotView::onDataReceiverError(const QString &error)
{
    qDebug() << "Data receiver error:" << error;
}

// Projection control methods
void PlotView::toggleProjectionMode()
{
    if (m_projectionMode == PERSPECTIVE_PROJECTION)
    {
        m_projectionMode = ORTHOGRAPHIC_PROJECTION;
    }
    else
    {
        m_projectionMode = PERSPECTIVE_PROJECTION;
    }
    update();
}

void PlotView::setProjectionMode(ProjectionMode mode)
{
    m_projectionMode = mode;
    update();
}

PlotView::ProjectionMode PlotView::getProjectionMode() const
{
    return m_projectionMode;
}

void PlotView::increaseFOV()
{
    if (m_projectionMode == PERSPECTIVE_PROJECTION)
    {
        m_fov = qMin(120.0f, m_fov + 5.0f); // Max FOV 120 degrees
        update();
    }
}

void PlotView::decreaseFOV()
{
    if (m_projectionMode == PERSPECTIVE_PROJECTION)
    {
        m_fov = qMax(10.0f, m_fov - 5.0f); // Min FOV 10 degrees
        update();
    }
}

void PlotView::setFOV(float fov)
{
    m_fov = qBound(10.0f, fov, 120.0f); // Clamp between 10 and 120 degrees
    update();
}

float PlotView::getFOV() const
{
    return m_fov;
}
