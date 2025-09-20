#include <QApplication>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QSplitter>
#include <QTimer>
#include <QFontDatabase>
#include <QDir>
#include <QDebug>
#include <QWindow>
#include <QPushButton>
#include <QMouseEvent>
#include <QFrame>
#include <QMenuBar>
#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QAction>
#include <QLabel>
#include <QKeyEvent>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QGridLayout>
#include "modules/plot_view.h"

#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <memory>
#include <signal.h>
#include <iostream>


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QMainWindow window;
    window.setWindowTitle("LumosCalibView - Hardware Calibration Tool");
    window.resize(800, 600);
    
    QWidget *centralWidget = new QWidget;
    window.setCentralWidget(centralWidget);
    
    QGridLayout *layout = new QGridLayout(centralWidget);
    
    // Create PlotView widget for top left corner
    PlotView *plotWidget = new PlotView;
    plotWidget->setMinimumSize(400, 300);
    plotWidget->setStyleSheet("border: 1px solid gray;");
    
    // Add first data series with normal line thickness
    std::vector<float> xData1, yData1, zData1;
    for (int i = 0; i < 100; ++i) {
        float t = i * 0.1f;
        xData1.push_back(t);
        yData1.push_back(sin(t) * cos(t * 0.5f));
        zData1.push_back(cos(t) * 0.5f);
    }
    plotWidget->addDataSeries(xData1, yData1, zData1, 1.5f); // Normal thickness
    
    // Add second data series with double line thickness
    std::vector<float> xData2, yData2, zData2;
    for (int i = 0; i < 100; ++i) {
        float t = i * 0.1f;
        xData2.push_back(t);
        yData2.push_back(sin(t * 1.5f) * 0.7f); // Different frequency and amplitude
        zData2.push_back(sin(t) * 0.3f);        // Different Z pattern
    }
    plotWidget->addDataSeries(xData2, yData2, zData2, 3.0f); // Double thickness
    
    plotWidget->setAxisLabels("Time", "Signal", "Amplitude");
    
    QLabel *titleLabel = new QLabel("Hardware Connection & Calibration Interface");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");
    
    QLabel *statusLabel = new QLabel("Status: Ready to connect");
    
    // Layout: PlotView widget in top left, title spans top right
    layout->addWidget(plotWidget, 0, 0, 2, 1);  // row 0-1, col 0
    layout->addWidget(titleLabel, 0, 1);          // row 0, col 1
    layout->addWidget(statusLabel, 1, 1);         // row 1, col 1
    
    // Set column stretch to give more space to the right side
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 2);
    
    window.show();
    
    return app.exec();
}
