#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QVector3D>
#include <vector>
#include "view_angles.h"
#include "data_receiver.h"

class PlotView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    struct PlotData {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        GLenum drawMode = GL_TRIANGLES;
        float lineWidth = 1.0f;
    };

    enum PlotMode {
        PLOT_2D,
        PLOT_3D
    };

    enum InteractionMode {
        ROTATE_MODE,
        ZOOM_MODE,
        PAN_MODE
    };

    enum ProjectionMode {
        PERSPECTIVE_PROJECTION,
        ORTHOGRAPHIC_PROJECTION
    };

    explicit PlotView(QWidget *parent = nullptr);
    ~PlotView() override;

    // Data management
    void setPlotData(const PlotData& data);
    void addDataPoint(float x, float y, float z = 0.0f);
    void addDataSeries(const std::vector<float>& xData, const std::vector<float>& yData, 
                       const std::vector<float>& zData = {}, float lineWidth = 1.0f);
    void addPlotData(const PlotData& data);
    void clearData();

    // Plot configuration
    void setPlotMode(PlotMode mode);
    void setShowGrid(bool show);
    void setShowAxes(bool show);
    void setAxisLabels(const QString& xLabel, const QString& yLabel, const QString& zLabel = "");

    // View control
    void resetView();
    void setViewAngles(double azimuth, double elevation);
    
    // Projection control
    void toggleProjectionMode();
    void setProjectionMode(ProjectionMode mode);
    ProjectionMode getProjectionMode() const;
    void increaseFOV();
    void decreaseFOV();
    void setFOV(float fov);
    float getFOV() const;
    
    // Real-time data
    void startDataReceiver(quint16 port = 8080);
    void stopDataReceiver();
    void connectToDataSource(const QString& host, quint16 port);
    void setRealTimeMode(bool enabled);
    void setMaxRealTimePoints(int maxPoints);
    
    bool isReceivingData() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void updateAnimation();
    void onNewDataReceived();
    void onDataReceiverConnected(bool connected);
    void onDataReceiverError(const QString& error);

private:
    void setupShaders();
    void setupBuffers();
    void createGridData();
    void createAxisData();
    void createOriginPlaneData();
    void createBackgroundPlaneData();
    void renderGrid();
    void renderAxes();
    void renderOriginPlanes();
    void renderBackgroundPlanes();
    void renderData();
    void renderAxisNumbers(QPainter& painter);
    void renderAxisLabels(QPainter& painter);
    void renderInteractionMode(QPainter& painter);
    
    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;
    QVector3D worldToScreen(const QVector3D& worldPos) const;
    
    // Dynamic grid helpers
    float calculateOptimalGridStep() const;
    float getVisibleWorldSize() const;

    // OpenGL resources
    QOpenGLShaderProgram* m_shaderProgram;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_indexBuffer;
    QOpenGLVertexArrayObject m_vao;
    
    QOpenGLBuffer m_gridVertexBuffer;
    QOpenGLVertexArrayObject m_gridVAO;
    
    QOpenGLBuffer m_axisVertexBuffer;
    QOpenGLVertexArrayObject m_axisVAO;
    
    QOpenGLBuffer m_originPlaneVertexBuffer;
    QOpenGLVertexArrayObject m_originPlaneVAO;
    
    QOpenGLBuffer m_backgroundPlaneVertexBuffer;
    QOpenGLVertexArrayObject m_backgroundPlaneVAO;

    // Plot data
    std::vector<PlotData> m_plotDataSeries;
    std::vector<float> m_gridVertices;
    std::vector<float> m_axisVertices;
    std::vector<float> m_originPlaneVertices;
    std::vector<float> m_backgroundPlaneVertices;
    
    // View state
    ViewAngles m_viewAngles;
    PlotMode m_plotMode;
    bool m_showGrid;
    bool m_showAxes;
    float m_zoom;
    QVector3D m_panOffset;
    
    // Projection state
    ProjectionMode m_projectionMode;
    float m_fov;
    
    // Mouse interaction
    QPoint m_lastMousePos;
    bool m_mousePressed;
    InteractionMode m_interactionMode;
    
    // Animation
    QTimer* m_animationTimer;
    float m_animationTime;
    
    // Labels
    QString m_xLabel, m_yLabel, m_zLabel;
    
    // Real-time data
    DataReceiver* m_dataReceiver;
    QThread* m_dataThread;
    bool m_realTimeMode;
    int m_maxRealTimePoints;
    std::vector<DataPoint> m_realTimeBuffer;
};