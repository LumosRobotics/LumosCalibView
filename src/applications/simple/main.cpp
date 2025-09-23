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
#include "modules/multi_plot_container.h"

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
    
    // Create multi-plot container that fills the entire window
    MultiPlotContainer *multiPlotContainer = new MultiPlotContainer;
    multiPlotContainer->setStyleSheet("background-color: #f0f0f0;");
    window.setCentralWidget(multiPlotContainer);
    
    // Create a single plot view covering the whole window
    multiPlotContainer->createGridLayout(1, 1);
    
    // Configure the first plot view with TCP data receiver
    const auto& plotViews = multiPlotContainer->getPlotViews();
    if (!plotViews.isEmpty()) {
        PlotView* firstPlot = plotViews[0];
        firstPlot->setAxisLabels("Time", "Signal", "Amplitude");
        firstPlot->startDataReceiver(8080);  // Listen on port 8080
    }
    
    // Add status bar for instructions (optional overlay)
    QLabel *statusLabel = new QLabel("Hardware Calibration Tool - Hold Cmd and click to resize/move plot views", multiPlotContainer);
    statusLabel->setStyleSheet("background-color: rgba(0,0,0,128); color: white; padding: 5px; border-radius: 3px;");
    statusLabel->setWordWrap(true);
    statusLabel->move(10, 10);  // Top-left overlay
    statusLabel->adjustSize();
    
    window.show();
    
    return app.exec();
}
