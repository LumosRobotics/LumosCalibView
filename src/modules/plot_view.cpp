#include "plot_view.h"
#include <QDebug>
#include <QPaintEvent>
#include <QThread>
#include <cmath>

PlotView::PlotView(QWidget *parent)
    : QOpenGLWidget(parent), m_shaderProgram(nullptr), m_plotMode(PLOT_3D), m_showGrid(true), m_showAxes(false), m_zoom(1.0f), m_panOffset(0.0f, 0.0f, 0.0f), m_projectionMode(PERSPECTIVE_PROJECTION), m_fov(45.0f), m_mousePressed(false), m_interactionMode(ROTATE_MODE), m_animationTime(0.0f), m_dataReceiver(nullptr), m_dataThread(nullptr), m_realTimeMode(false), m_maxRealTimePoints(1000)
{
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &PlotView::updateAnimation);
    m_animationTimer->start(16); // ~60 FPS

    // Initialize view angles for 3D plotting with X right, Y up
    m_viewAngles.setAngles(0.0, 0.0);

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
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // White background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.5f);
    glPointSize(3.0f);

    setupShaders();
    setupBuffers();
    createGridData();
    createAxisData();
    createOriginPlaneData();
    createBackgroundPlaneData();
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

    // Origin plane VAO
    m_originPlaneVAO.create();
    m_originPlaneVertexBuffer.create();

    // Background plane VAO
    m_backgroundPlaneVAO.create();
    m_backgroundPlaneVertexBuffer.create();
}

void PlotView::createGridData()
{
    m_gridVertices.clear();

    // Dynamic grid spacing based on zoom level
    const float step = calculateOptimalGridStep();
    const float visibleSize = getVisibleWorldSize();
    const float boxSize = visibleSize * 0.6f; // Size of the grid box

    // Get current view angles to determine which box faces are visible
    const double azimuth = m_viewAngles.getAzimuth();
    const double elevation = m_viewAngles.getElevation();

    // Box frame stays centered at origin, but grid lines shift with panning
    const float boxHalfSize = boxSize * 0.5f;
    const float boxMinX = -boxHalfSize;
    const float boxMaxX = boxHalfSize;
    const float boxMinY = -boxHalfSize;
    const float boxMaxY = boxHalfSize;
    const float boxMinZ = -boxHalfSize;
    const float boxMaxZ = boxHalfSize;

    // Create a smooth sliding grid within the fixed box frame
    // The pan offset creates fractional shifts that make lines slide continuously

    // Calculate fractional offset within one grid step
    // Grid lines should move in same direction as pan for intuitive behavior
    const float fracOffsetX = fmod(m_panOffset.x(), step);
    const float fracOffsetY = fmod(m_panOffset.y(), step);
    const float fracOffsetZ = -fmod(m_panOffset.z(), step);

    // Create enough grid lines to cover the box plus some margin for sliding
    const float margin = step * 2.0f; // Extra lines for smooth sliding
    const float extendedMinX = boxMinX - margin;
    const float extendedMaxX = boxMaxX + margin;
    const float extendedMinY = boxMinY - margin;
    const float extendedMaxY = boxMaxY + margin;
    const float extendedMinZ = boxMinZ - margin;
    const float extendedMaxZ = boxMaxZ + margin;

    // Calculate grid line positions, including fractional offset for smooth sliding
    const int startX = (int)floor(extendedMinX / step);
    const int endX = (int)ceil(extendedMaxX / step);
    const int startY = (int)floor(extendedMinY / step);
    const int endY = (int)ceil(extendedMaxY / step);
    const int startZ = (int)floor(extendedMinZ / step);
    const int endZ = (int)ceil(extendedMaxZ / step);

    // Determine which box faces to show based on viewing direction
    // Use sin/cos to determine if we're looking from positive or negative side
    const bool showPositiveZ = cos(elevation) * cos(azimuth) > 0; // Looking towards +Z
    const bool showPositiveY = sin(elevation) > 0;                // Looking towards +Y
    const bool showPositiveX = cos(elevation) * sin(azimuth) > 0; // Looking towards +X

    // Select Z plane position (XY plane at far Z)
    const float zPlane = showPositiveZ ? boxMinZ : boxMaxZ;

    // Select Y plane position (XZ plane at far Y)
    const float yPlane = showPositiveY ? boxMinY : boxMaxY;

    // Select X plane position (YZ plane at far X)
    const float xPlane = showPositiveX ? boxMaxX : boxMinX;

    // Grid colors - black on white background
    const float mainColor[3] = {0.0f, 0.0f, 0.0f}; // Main planes - black
    const float sideColor[3] = {0.2f, 0.2f, 0.2f}; // Side planes - dark gray

    // XY Plane Grid (at far Z position)
    // Vertical lines (parallel to Y-axis) - slide with fractional X offset
    for (int i = startX; i <= endX; ++i)
    {
        float x = i * step + fracOffsetX; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {x, boxMinY, zPlane, mainColor[0], mainColor[1], mainColor[2],
                                                     x, boxMaxY, zPlane, mainColor[0], mainColor[1], mainColor[2]});
    }

    // Horizontal lines (parallel to X-axis) - slide with fractional Y offset
    for (int i = startY; i <= endY; ++i)
    {
        float y = i * step + fracOffsetY; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {boxMinX, y, zPlane, mainColor[0], mainColor[1], mainColor[2],
                                                     boxMaxX, y, zPlane, mainColor[0], mainColor[1], mainColor[2]});
    }

    // XZ Plane Grid (at far Y position)
    // Lines parallel to X-axis - slide with fractional Z offset
    for (int i = startZ; i <= endZ; ++i)
    {
        float z = i * step + fracOffsetZ; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {boxMinX, yPlane, z, sideColor[0], sideColor[1], sideColor[2],
                                                     boxMaxX, yPlane, z, sideColor[0], sideColor[1], sideColor[2]});
    }

    // Lines parallel to Z-axis - slide with fractional X offset
    for (int i = startX; i <= endX; ++i)
    {
        float x = i * step + fracOffsetX; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {x, yPlane, boxMinZ, sideColor[0], sideColor[1], sideColor[2],
                                                     x, yPlane, boxMaxZ, sideColor[0], sideColor[1], sideColor[2]});
    }

    // YZ Plane Grid (at far X position)
    // Lines parallel to Y-axis - slide with fractional Z offset
    for (int i = startZ; i <= endZ; ++i)
    {
        float z = i * step + fracOffsetZ; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {xPlane, boxMinY, z, sideColor[0], sideColor[1], sideColor[2],
                                                     xPlane, boxMaxY, z, sideColor[0], sideColor[1], sideColor[2]});
    }

    // Lines parallel to Z-axis - slide with fractional Y offset
    for (int i = startY; i <= endY; ++i)
    {
        float y = i * step + fracOffsetY; // Add fractional offset for smooth sliding

        m_gridVertices.insert(m_gridVertices.end(), {xPlane, y, boxMinZ, sideColor[0], sideColor[1], sideColor[2],
                                                     xPlane, y, boxMaxZ, sideColor[0], sideColor[1], sideColor[2]});
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

void PlotView::createOriginPlaneData()
{
    m_originPlaneVertices.clear();

    // Get current view parameters to determine line extent
    const float visibleSize = getVisibleWorldSize();
    const float extent = visibleSize * 1.0f; // Make lines extend beyond visible area

    // Origin plane line color - darker than grid for emphasis
    const float originColor[3] = {0.0f, 0.0f, 0.0f};

    // X-axis line (horizontal line through origin)
    m_originPlaneVertices.insert(m_originPlaneVertices.end(), {-extent, 0.0f, 0.0f, originColor[0], originColor[1], originColor[2],
                                                               extent, 0.0f, 0.0f, originColor[0], originColor[1], originColor[2]});

    // Y-axis line (vertical line through origin)
    m_originPlaneVertices.insert(m_originPlaneVertices.end(), {0.0f, -extent, 0.0f, originColor[0], originColor[1], originColor[2],
                                                               0.0f, extent, 0.0f, originColor[0], originColor[1], originColor[2]});

    // Z-axis line (depth line through origin)
    m_originPlaneVertices.insert(m_originPlaneVertices.end(), {0.0f, 0.0f, -extent, originColor[0], originColor[1], originColor[2],
                                                               0.0f, 0.0f, extent, originColor[0], originColor[1], originColor[2]});

    m_originPlaneVAO.bind();
    m_originPlaneVertexBuffer.bind();
    m_originPlaneVertexBuffer.allocate(m_originPlaneVertices.data(), m_originPlaneVertices.size() * sizeof(float));

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

    m_originPlaneVAO.release();
}

void PlotView::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_shaderProgram->bind();

    QMatrix4x4 mvpMatrix = getProjectionMatrix() * getViewMatrix();
    m_shaderProgram->setUniformValue("uMVPMatrix", mvpMatrix);

    if (m_showGrid)
    {
        renderBackgroundPlanes(); // Render solid background planes first
        renderGrid();
        renderOriginPlanes(); // Always show origin plane lines when grid is shown
    }

    // Always render origin axes for reference
    renderAxes();

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

    // Always render axis numbers even if axis lines are disabled
    renderAxisNumbers(painter);
    
    // Render axis labels (X, Y, Z) at the end of each axis line
    renderAxisLabels(painter);

    // Show current interaction mode
    renderInteractionMode(painter);

    painter.end();
}

void PlotView::renderGrid()
{
    // Create a view matrix without pan offset for grid lines (same as background planes)
    QMatrix4x4 gridView;
    gridView.translate(0.0f, 0.0f, -10.0f * m_zoom);
    // Skip applying m_panOffset here - this keeps grid aligned with background planes

    if (m_plotMode == PLOT_3D)
    {
        gridView.rotate(m_viewAngles.getElevation() * 180.0 / M_PI, 1.0f, 0.0f, 0.0f);
        gridView.rotate(m_viewAngles.getAzimuth() * 180.0 / M_PI, 0.0f, 1.0f, 0.0f);
    }

    // Set the pan-independent MVP matrix for grid lines
    QMatrix4x4 gridMVP = getProjectionMatrix() * gridView;
    m_shaderProgram->setUniformValue("uMVPMatrix", gridMVP);

    m_gridVAO.bind();
    glDrawArrays(GL_LINES, 0, m_gridVertices.size() / 6);
    m_gridVAO.release();

    // Restore the normal MVP matrix for subsequent rendering
    QMatrix4x4 normalMVP = getProjectionMatrix() * getViewMatrix();
    m_shaderProgram->setUniformValue("uMVPMatrix", normalMVP);
}

void PlotView::renderAxes()
{
    glLineWidth(3.0f);
    m_axisVAO.bind();
    glDrawArrays(GL_LINES, 0, m_axisVertices.size() / 6);
    m_axisVAO.release();
    glLineWidth(1.5f);
}

void PlotView::renderOriginPlanes()
{
    glLineWidth(2.5f); // Thicker than grid (1.5f) but thinner than axis lines (3.0f)
    m_originPlaneVAO.bind();
    glDrawArrays(GL_LINES, 0, m_originPlaneVertices.size() / 6);
    m_originPlaneVAO.release();
    glLineWidth(1.5f); // Reset to default
}

void PlotView::createBackgroundPlaneData()
{
    m_backgroundPlaneVertices.clear();

    // Get current view parameters to determine which planes to show
    const double azimuth = m_viewAngles.getAzimuth();
    const double elevation = m_viewAngles.getElevation();

    // Calculate box size to maintain constant visual appearance
    // The planes should appear the same size regardless of zoom/pan
    const float visibleSize = getVisibleWorldSize();

    // Use different size multipliers for different projection modes
    float sizeMultiplier;
    if (m_projectionMode == ORTHOGRAPHIC_PROJECTION)
    {
        sizeMultiplier = 0.8f; // Smaller in orthographic mode
    }
    else
    {
        sizeMultiplier = 1.5f; // Larger in perspective mode
    }

    const float boxSize = visibleSize * sizeMultiplier;
    const float boxHalfSize = boxSize * 0.5f;

    // Keep the box centered at origin - don't move with panning
    // The large size ensures it always provides backdrop regardless of pan position
    const float boxMinX = -boxHalfSize;
    const float boxMaxX = boxHalfSize;
    const float boxMinY = -boxHalfSize;
    const float boxMaxY = boxHalfSize;
    const float boxMinZ = -boxHalfSize;
    const float boxMaxZ = boxHalfSize;

    // Determine which box faces to show based on viewing direction
    const bool showPositiveZ = cos(elevation) * cos(azimuth) > 0;
    const bool showPositiveY = sin(elevation) > 0;
    const bool showPositiveX = cos(elevation) * sin(azimuth) > 0;

    // Select plane positions (far edges of the box)
    const float zPlane = showPositiveZ ? boxMinZ : boxMaxZ;
    const float yPlane = showPositiveY ? boxMinY : boxMaxY;
    const float xPlane = showPositiveX ? boxMaxX : boxMinX;

    // 80% grey color (0.8, 0.8, 0.8)
    const float bgColor[3] = {0.8f, 0.8f, 0.8f};

    // XY Plane (at far Z position) - create two triangles to form a rectangle
    m_backgroundPlaneVertices.insert(m_backgroundPlaneVertices.end(), {// Triangle 1
                                                                       boxMinX, boxMinY, zPlane, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, boxMinY, zPlane, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, boxMaxY, zPlane, bgColor[0], bgColor[1], bgColor[2],
                                                                       // Triangle 2
                                                                       boxMinX, boxMinY, zPlane, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, boxMaxY, zPlane, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMinX, boxMaxY, zPlane, bgColor[0], bgColor[1], bgColor[2]});

    // XZ Plane (at far Y position)
    m_backgroundPlaneVertices.insert(m_backgroundPlaneVertices.end(), {// Triangle 1
                                                                       boxMinX, yPlane, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, yPlane, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, yPlane, boxMaxZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       // Triangle 2
                                                                       boxMinX, yPlane, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMaxX, yPlane, boxMaxZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       boxMinX, yPlane, boxMaxZ, bgColor[0], bgColor[1], bgColor[2]});

    // YZ Plane (at far X position)
    m_backgroundPlaneVertices.insert(m_backgroundPlaneVertices.end(), {// Triangle 1
                                                                       xPlane, boxMinY, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       xPlane, boxMaxY, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       xPlane, boxMaxY, boxMaxZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       // Triangle 2
                                                                       xPlane, boxMinY, boxMinZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       xPlane, boxMaxY, boxMaxZ, bgColor[0], bgColor[1], bgColor[2],
                                                                       xPlane, boxMinY, boxMaxZ, bgColor[0], bgColor[1], bgColor[2]});

    m_backgroundPlaneVAO.bind();
    m_backgroundPlaneVertexBuffer.bind();
    m_backgroundPlaneVertexBuffer.allocate(m_backgroundPlaneVertices.data(), m_backgroundPlaneVertices.size() * sizeof(float));

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

    m_backgroundPlaneVAO.release();
}

void PlotView::renderBackgroundPlanes()
{
    // Create a view matrix without pan offset for background planes
    QMatrix4x4 backgroundView;
    backgroundView.translate(0.0f, 0.0f, -10.0f * m_zoom);
    // Skip applying m_panOffset here - this keeps planes centered in view

    if (m_plotMode == PLOT_3D)
    {
        backgroundView.rotate(m_viewAngles.getElevation() * 180.0 / M_PI, 1.0f, 0.0f, 0.0f);
        backgroundView.rotate(m_viewAngles.getAzimuth() * 180.0 / M_PI, 0.0f, 1.0f, 0.0f);
    }

    // Set the pan-independent MVP matrix for background planes
    QMatrix4x4 backgroundMVP = getProjectionMatrix() * backgroundView;
    m_shaderProgram->setUniformValue("uMVPMatrix", backgroundMVP);

    m_backgroundPlaneVAO.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_backgroundPlaneVertices.size() / 6);
    m_backgroundPlaneVAO.release();

    // Restore the normal MVP matrix for subsequent rendering
    QMatrix4x4 normalMVP = getProjectionMatrix() * getViewMatrix();
    m_shaderProgram->setUniformValue("uMVPMatrix", normalMVP);
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
            createGridData();            // Update grid for new view angles
            createOriginPlaneData();     // Update origin planes for new view angles
            createBackgroundPlaneData(); // Update background planes for new view angles
        }
        break;

    case ZOOM_MODE:
    {
        float zoomFactor = 1.0f + (-delta.y() * 0.01f);
        m_zoom = qMax(0.1f, qMin(5.0f, m_zoom * zoomFactor));
        createGridData();            // Update grid for new zoom level
        createOriginPlaneData();     // Update origin planes for new zoom level
        createBackgroundPlaneData(); // Update background planes to maintain constant visual size
    }
    break;

    case PAN_MODE:
    {
        float panSpeed = 0.01f * m_zoom;
        
        // Convert mouse movement to 3D world coordinates based on current view angles
        double azimuth = m_viewAngles.getAzimuth();
        double elevation = m_viewAngles.getElevation();
        
        // Calculate the right and up vectors in world space based on view orientation
        // Right vector (screen X direction in world space)
        float rightX = cos(azimuth);
        float rightY = sin(azimuth);
        float rightZ = 0.0f;
        
        // Up vector (screen Y direction in world space) 
        float upX = -sin(azimuth) * sin(elevation);
        float upY = cos(azimuth) * sin(elevation);
        float upZ = cos(elevation);
        
        // Apply mouse delta to 3D pan offset using view-oriented vectors
        float deltaX = delta.x() * panSpeed;
        float deltaY = -delta.y() * panSpeed; // Invert Y for intuitive panning
        
        m_panOffset.setX(m_panOffset.x() + deltaX * rightX + deltaY * upX);
        m_panOffset.setY(m_panOffset.y() + deltaX * rightY + deltaY * upY);
        m_panOffset.setZ(m_panOffset.z() + deltaX * rightZ + deltaY * upZ);
        
        createGridData();                                         // Update grid lines to shift within the fixed box frame
        createOriginPlaneData();                                  // Update origin planes for new pan position
        // Background planes stay centered and don't need updates for panning
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
    createGridData();            // Update grid for new zoom level
    createOriginPlaneData();     // Update origin planes for new zoom level
    createBackgroundPlaneData(); // Update background planes to maintain constant visual size
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
    m_panOffset = QVector3D(0.0f, 0.0f, 0.0f);
    createGridData();            // Update grid for reset zoom level
    createOriginPlaneData();     // Update origin planes for reset zoom level
    createBackgroundPlaneData(); // Update background planes for reset view
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
    painter.setPen(QPen(Qt::black, 1));
    painter.setFont(QFont("Arial", 8));

    // Dynamic grid parameters matching createGridData()
    const float step = calculateOptimalGridStep();
    const float visibleSize = getVisibleWorldSize();
    const float boxSize = visibleSize * 0.6f;

    // Get current view angles to determine which box faces are visible (matching createGridData)
    const double azimuth = m_viewAngles.getAzimuth();
    const double elevation = m_viewAngles.getElevation();

    // Box frame stays centered at origin, but grid lines shift with panning (matching createGridData)
    const float boxHalfSize = boxSize * 0.5f;
    const float boxMinX = -boxHalfSize;
    const float boxMaxX = boxHalfSize;
    const float boxMinY = -boxHalfSize;
    const float boxMaxY = boxHalfSize;
    const float boxMinZ = -boxHalfSize;
    const float boxMaxZ = boxHalfSize;

    // With smooth sliding grid, numbers should show actual world coordinates (matching createGridData)

    // Calculate fractional offset within one grid step (matching createGridData)
    // Grid lines should move in same direction as pan for intuitive behavior
    const float fracOffsetX = fmod(m_panOffset.x(), step);
    const float fracOffsetY = fmod(m_panOffset.y(), step);
    const float fracOffsetZ = -fmod(m_panOffset.z(), step);

    // Use extended range for numbers (matching createGridData)
    const float margin = step * 2.0f;
    const float extendedMinX = boxMinX - margin;
    const float extendedMaxX = boxMaxX + margin;
    const float extendedMinY = boxMinY - margin;
    const float extendedMaxY = boxMaxY + margin;
    const float extendedMinZ = boxMinZ - margin;
    const float extendedMaxZ = boxMaxZ + margin;

    // Calculate grid line positions (matching createGridData)
    const int startX = (int)floor(extendedMinX / step);
    const int endX = (int)ceil(extendedMaxX / step);
    const int startY = (int)floor(extendedMinY / step);
    const int endY = (int)ceil(extendedMaxY / step);
    const int startZ = (int)floor(extendedMinZ / step);
    const int endZ = (int)ceil(extendedMaxZ / step);

    // Determine which box faces to show (matching createGridData)
    const bool showPositiveZ = cos(elevation) * cos(azimuth) > 0;
    const bool showPositiveY = sin(elevation) > 0;
    const bool showPositiveX = cos(elevation) * sin(azimuth) > 0;

    // Select plane positions (matching createGridData)
    const float zPlane = showPositiveZ ? boxMinZ : boxMaxZ;
    const float yPlane = showPositiveY ? boxMinY : boxMaxY;
    const float xPlane = showPositiveX ? boxMaxX : boxMinX;

    // Determine decimal places for clean number display
    int decimalPlaces = (step >= 1.0f) ? 0 : (step >= 0.1f) ? 1
                                                            : 2;

    // X-axis numbers along XY plane (at far Z position)
    for (int i = startX; i <= endX; ++i)
    {
        float x = i * step + fracOffsetX; // Include fractional offset for smooth sliding
        QVector3D worldPos(x, boxMinY, zPlane);
        QVector3D screenPos = worldToScreen(worldPos);

        if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
        {
            // Show the "nice" step value, not the offset position
            float labelValue = i * step;
            QString text = QString::number(labelValue, 'f', decimalPlaces);
            QRect textRect = painter.fontMetrics().boundingRect(text);

            int textX = (int)screenPos.x() - textRect.width() / 2;
            int textY = (int)screenPos.y() + textRect.height() + 5;

            if (textX >= 0 && textX + textRect.width() <= width() &&
                textY >= 0 && textY <= height())
            {
                painter.drawText(textX, textY, text);
            }
        }
    }

    // Y-axis numbers along XY plane (at far Z position)
    for (int i = startY; i <= endY; ++i)
    {
        float y = i * step + fracOffsetY; // Include fractional offset for smooth sliding
        QVector3D worldPos(boxMinX, y, zPlane);
        QVector3D screenPos = worldToScreen(worldPos);

        if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
        {
            // Show the "nice" step value, not the offset position
            float labelValue = i * step;
            QString text = QString::number(labelValue, 'f', decimalPlaces);
            QRect textRect = painter.fontMetrics().boundingRect(text);

            int textX = (int)screenPos.x() - textRect.width() - 5;
            int textY = (int)screenPos.y() + textRect.height() / 2;

            if (textX >= 0 && textX + textRect.width() <= width() &&
                textY >= 0 && textY <= height())
            {
                painter.drawText(textX, textY, text);
            }
        }
    }

    // Z-axis numbers along XZ plane (at far Y position) when in 3D mode
    if (m_plotMode == PLOT_3D || m_projectionMode == PERSPECTIVE_PROJECTION)
    {
        for (int i = startZ; i <= endZ; ++i)
        {
            float z = i * step + fracOffsetZ; // Include fractional offset for smooth sliding
            QVector3D worldPos(boxMinX, yPlane, z);
            QVector3D screenPos = worldToScreen(worldPos);

            if (screenPos.z() > -1.0f && screenPos.z() < 1.0f)
            {
                // Show the "nice" step value, not the offset position
                float labelValue = i * step;
                QString text = QString::number(labelValue, 'f', decimalPlaces);
                QRect textRect = painter.fontMetrics().boundingRect(text);

                int textX = (int)screenPos.x() + 5;
                int textY = (int)screenPos.y() + textRect.height() / 2;

                if (textX >= 0 && textX + textRect.width() <= width() &&
                    textY >= 0 && textY <= height())
                {
                    painter.setPen(QPen(Qt::cyan, 1)); // Different color for Z
                    painter.drawText(textX, textY, text);
                    painter.setPen(QPen(Qt::black, 1)); // Reset color
                }
            }
        }
    }

    // Axis labels positioned at box edges
    const float labelOffset = boxHalfSize + 1.0f;

    if (!m_xLabel.isEmpty())
    {
        QVector3D xLabelPos = worldToScreen(QVector3D(boxMaxX + 1.0f, boxMinY, zPlane));
        if (xLabelPos.z() > -1.0f && xLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::red, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)xLabelPos.x() + 5, (int)xLabelPos.y(), m_xLabel);
        }
    }

    if (!m_yLabel.isEmpty())
    {
        QVector3D yLabelPos = worldToScreen(QVector3D(boxMinX, boxMaxY + 1.0f, zPlane));
        if (yLabelPos.z() > -1.0f && yLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::green, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)yLabelPos.x() + 5, (int)yLabelPos.y(), m_yLabel);
        }
    }

    if (!m_zLabel.isEmpty() && m_plotMode == PLOT_3D)
    {
        QVector3D zLabelPos = worldToScreen(QVector3D(boxMinX, yPlane, boxMaxZ + 1.0f));
        if (zLabelPos.z() > -1.0f && zLabelPos.z() < 1.0f)
        {
            painter.setPen(QPen(Qt::blue, 1));
            painter.setFont(QFont("Arial", 10, QFont::Bold));
            painter.drawText((int)zLabelPos.x() + 5, (int)zLabelPos.y(), m_zLabel);
        }
    }

    // Add plane labels at center of each background plane
    painter.setPen(QPen(Qt::black, 2));
    painter.setFont(QFont("Arial", 16, QFont::Bold));

    // XY plane label (at center of XY plane at far Z position)
    QVector3D xyPlaneCenter = worldToScreen(QVector3D(0.0f, 0.0f, zPlane));
    if (xyPlaneCenter.z() > -1.0f && xyPlaneCenter.z() < 1.0f)
    {
        QString planeLabel = "XY";
        QRect textRect = painter.fontMetrics().boundingRect(planeLabel);
        int textX = (int)xyPlaneCenter.x() - textRect.width() / 2;
        int textY = (int)xyPlaneCenter.y() + textRect.height() / 2;

        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, planeLabel);
        }
    }

    // XZ plane label (at center of XZ plane at far Y position)
    QVector3D xzPlaneCenter = worldToScreen(QVector3D(0.0f, yPlane, 0.0f));
    if (xzPlaneCenter.z() > -1.0f && xzPlaneCenter.z() < 1.0f)
    {
        QString planeLabel = "XZ";
        QRect textRect = painter.fontMetrics().boundingRect(planeLabel);
        int textX = (int)xzPlaneCenter.x() - textRect.width() / 2;
        int textY = (int)xzPlaneCenter.y() + textRect.height() / 2;

        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, planeLabel);
        }
    }

    // YZ plane label (at center of YZ plane at far X position)
    QVector3D yzPlaneCenter = worldToScreen(QVector3D(xPlane, 0.0f, 0.0f));
    if (yzPlaneCenter.z() > -1.0f && yzPlaneCenter.z() < 1.0f)
    {
        QString planeLabel = "YZ";
        QRect textRect = painter.fontMetrics().boundingRect(planeLabel);
        int textX = (int)yzPlaneCenter.x() - textRect.width() / 2;
        int textY = (int)yzPlaneCenter.y() + textRect.height() / 2;

        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, planeLabel);
        }
    }
}

void PlotView::renderAxisLabels(QPainter &painter)
{
    // Get the axis length that matches createAxisData()
    const float axisLength = 6.0f;
    
    // Set font and color for axis labels
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    
    // X-axis label (red)
    QVector3D xAxisEnd = worldToScreen(QVector3D(axisLength, 0.0f, 0.0f));
    if (xAxisEnd.z() > -1.0f && xAxisEnd.z() < 1.0f)
    {
        painter.setPen(QPen(Qt::red, 2));
        QString text = "X";
        QRect textRect = painter.fontMetrics().boundingRect(text);
        int textX = (int)xAxisEnd.x() + 5;
        int textY = (int)xAxisEnd.y() + textRect.height() / 2;
        
        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, text);
        }
    }
    
    // Y-axis label (green)
    QVector3D yAxisEnd = worldToScreen(QVector3D(0.0f, axisLength, 0.0f));
    if (yAxisEnd.z() > -1.0f && yAxisEnd.z() < 1.0f)
    {
        painter.setPen(QPen(Qt::green, 2));
        QString text = "Y";
        QRect textRect = painter.fontMetrics().boundingRect(text);
        int textX = (int)yAxisEnd.x() - textRect.width() / 2;
        int textY = (int)yAxisEnd.y() - 5;
        
        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, text);
        }
    }
    
    // Z-axis label (blue)
    QVector3D zAxisEnd = worldToScreen(QVector3D(0.0f, 0.0f, axisLength));
    if (zAxisEnd.z() > -1.0f && zAxisEnd.z() < 1.0f)
    {
        painter.setPen(QPen(Qt::blue, 2));
        QString text = "Z";
        QRect textRect = painter.fontMetrics().boundingRect(text);
        int textX = (int)zAxisEnd.x() + 5;
        int textY = (int)zAxisEnd.y() - 5;
        
        if (textX >= 0 && textX + textRect.width() <= width() &&
            textY >= 0 && textY <= height())
        {
            painter.drawText(textX, textY, text);
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
        // Convert to plot format with newest data at x=0
        std::vector<float> xData, yData, zData;

        // Get the timestamp of the most recent data point
        double latestTimestamp = m_realTimeBuffer.back().timestamp;

        for (const auto &point : m_realTimeBuffer)
        {
            // Calculate age of each point relative to the latest data
            float age = static_cast<float>(latestTimestamp - point.timestamp);
            xData.push_back(age); // Age 0 = newest data at x=0, older data at positive x
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
    createBackgroundPlaneData(); // Update background planes for new projection mode
    update();
}

void PlotView::setProjectionMode(ProjectionMode mode)
{
    m_projectionMode = mode;
    createBackgroundPlaneData(); // Update background planes for new projection mode
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

// Dynamic grid helper functions
float PlotView::getVisibleWorldSize() const
{
    // Calculate the visible world size based on zoom and projection
    if (m_projectionMode == ORTHOGRAPHIC_PROJECTION)
    {
        // For orthographic projection, zoom multiplies the visible area
        // Higher zoom = larger visible area (zoomed out)
        return 10.0f * m_zoom; // Base size of 10 units, scaled by zoom
    }
    else
    {
        // For perspective projection, it's more complex and depends on distance
        // For simplicity, use similar calculation but consider FOV
        float baseSizeFromFOV = 10.0f * tan(m_fov * M_PI / 360.0f); // Half FOV in radians
        return baseSizeFromFOV * m_zoom;                            // Same direction as orthographic
    }
}

float PlotView::calculateOptimalGridStep() const
{
    const float visibleSize = getVisibleWorldSize();
    const int targetGridLines = 10; // Target number of grid lines visible

    // Calculate raw step size to get target number of lines
    float rawStep = visibleSize / targetGridLines;

    // Round to "nice" numbers (1, 2, 5, 10, 20, 50, etc.)
    float magnitude = pow(10.0f, floor(log10(rawStep)));
    float normalizedStep = rawStep / magnitude;

    float niceStep;
    if (normalizedStep <= 1.0f)
        niceStep = 1.0f;
    else if (normalizedStep <= 2.0f)
        niceStep = 2.0f;
    else if (normalizedStep <= 5.0f)
        niceStep = 5.0f;
    else
        niceStep = 10.0f;

    return niceStep * magnitude;
}
