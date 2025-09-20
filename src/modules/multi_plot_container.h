#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QVector>
#include <QRect>
#include <QPainter>
#include "plot_view.h"

class MultiPlotContainer : public QWidget
{
    Q_OBJECT

public:
    explicit MultiPlotContainer(QWidget *parent = nullptr);
    ~MultiPlotContainer() override;

    // Add/remove plot views
    void addPlotView(PlotView* plotView, const QRect& geometry);
    void removePlotView(PlotView* plotView);
    void clearPlotViews();
    
    // Layout management
    void createGridLayout(int rows, int cols);
    void createCustomLayout();
    
    // Get plot views
    const QVector<PlotView*>& getPlotViews() const { return m_plotViews; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    enum InteractionMode {
        NONE,
        MOVING,
        RESIZING
    };
    
    enum ResizeHandle {
        NO_HANDLE,
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        LEFT_EDGE,
        RIGHT_EDGE,
        TOP_EDGE,
        BOTTOM_EDGE
    };

    struct PlotViewInfo {
        PlotView* widget;
        QRect geometry;
        bool isSelected;
    };

    // Helper methods
    PlotViewInfo* getPlotViewAt(const QPoint& pos);
    ResizeHandle getResizeHandle(const PlotViewInfo* info, const QPoint& pos);
    QRect getResizeHandleRect(const QRect& geometry, ResizeHandle handle);
    void updateCursor(const QPoint& pos);
    void drawResizeHandles(QPainter& painter, const QRect& geometry);
    void applyResize(PlotViewInfo* info, const QPoint& delta, ResizeHandle handle);
    void ensureMinimumSize(QRect& geometry);
    
    // Member variables
    QVector<PlotViewInfo> m_plotViewInfos;
    QVector<PlotView*> m_plotViews; // For easy access
    
    // Interaction state
    InteractionMode m_interactionMode;
    ResizeHandle m_currentHandle;
    PlotViewInfo* m_activeView;
    QPoint m_lastMousePos;
    bool m_cmdPressed;
    
    // Constants
    static const int HANDLE_SIZE = 8;
    static const int MIN_WIDGET_WIDTH = 100;
    static const int MIN_WIDGET_HEIGHT = 75;
};