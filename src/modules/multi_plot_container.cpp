#include "multi_plot_container.h"
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <algorithm>

MultiPlotContainer::MultiPlotContainer(QWidget *parent)
    : QWidget(parent)
    , m_interactionMode(NONE)
    , m_currentHandle(NO_HANDLE)
    , m_activeView(nullptr)
    , m_cmdPressed(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    // Make sure we get focus and can receive keyboard events
    setFocus();
}

MultiPlotContainer::~MultiPlotContainer()
{
    clearPlotViews();
}

void MultiPlotContainer::addPlotView(PlotView* plotView, const QRect& geometry)
{
    if (!plotView) return;
    
    plotView->setParent(this);
    plotView->setGeometry(geometry);
    plotView->show();
    
    // Install event filter to intercept mouse events when Cmd is pressed
    plotView->installEventFilter(this);
    
    PlotViewInfo info;
    info.widget = plotView;
    info.geometry = geometry;
    info.isSelected = false;
    
    m_plotViewInfos.append(info);
    m_plotViews.append(plotView);
    
    update();
}

void MultiPlotContainer::removePlotView(PlotView* plotView)
{
    for (int i = 0; i < m_plotViewInfos.size(); ++i) {
        if (m_plotViewInfos[i].widget == plotView) {
            m_plotViewInfos.removeAt(i);
            m_plotViews.removeOne(plotView);
            plotView->setParent(nullptr);
            break;
        }
    }
    update();
}

void MultiPlotContainer::clearPlotViews()
{
    for (auto& info : m_plotViewInfos) {
        info.widget->setParent(nullptr);
    }
    m_plotViewInfos.clear();
    m_plotViews.clear();
    update();
}

void MultiPlotContainer::createGridLayout(int rows, int cols)
{
    clearPlotViews();
    
    int cellWidth = width() / cols;
    int cellHeight = height() / rows;
    
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            PlotView* plotView = new PlotView(this);
            
            QRect geometry(col * cellWidth, row * cellHeight, cellWidth, cellHeight);
            addPlotView(plotView, geometry);
        }
    }
}

void MultiPlotContainer::createCustomLayout()
{
    clearPlotViews();
    
    // Create a few example plot views
    PlotView* plot1 = new PlotView(this);
    PlotView* plot2 = new PlotView(this);
    PlotView* plot3 = new PlotView(this);
    
    addPlotView(plot1, QRect(10, 10, 300, 200));
    addPlotView(plot2, QRect(320, 10, 250, 150));
    addPlotView(plot3, QRect(10, 220, 400, 180));
}

void MultiPlotContainer::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    
    if (!m_cmdPressed) return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw resize handles for selected views
    for (const auto& info : m_plotViewInfos) {
        if (info.isSelected || &info == m_activeView) {
            drawResizeHandles(painter, info.geometry);
        }
    }
}

void MultiPlotContainer::mousePressEvent(QMouseEvent* event)
{
    // Always ensure we have keyboard focus when clicked
    setFocus();
    
    
    if (!m_cmdPressed) {
        QWidget::mousePressEvent(event);
        return;
    }
    
    m_lastMousePos = event->pos();
    PlotViewInfo* viewInfo = getPlotViewAt(event->pos());
    
    
    if (viewInfo) {
        // Clear other selections
        for (auto& info : m_plotViewInfos) {
            info.isSelected = false;
        }
        
        viewInfo->isSelected = true;
        m_activeView = viewInfo;
        
        // Check if clicking on resize handle
        m_currentHandle = getResizeHandle(viewInfo, event->pos());
        
        
        if (m_currentHandle != NO_HANDLE) {
            m_interactionMode = RESIZING;
        } else {
            m_interactionMode = MOVING;
        }
        
        update();
        event->accept();
    } else {
        // Clear all selections
        for (auto& info : m_plotViewInfos) {
            info.isSelected = false;
        }
        m_activeView = nullptr;
        m_interactionMode = NONE;
        update();
        QWidget::mousePressEvent(event);
    }
}

void MultiPlotContainer::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_cmdPressed) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    updateCursor(event->pos());
    
    if (m_interactionMode == NONE || !m_activeView) {
        return;
    }
    
    QPoint delta = event->pos() - m_lastMousePos;
    
    if (m_interactionMode == MOVING) {
        // Move the widget while preserving size
        QRect newGeometry = m_activeView->geometry.translated(delta);
        
        // Keep within bounds while preserving size
        if (newGeometry.left() < 0) {
            newGeometry.moveLeft(0);
        }
        if (newGeometry.top() < 0) {
            newGeometry.moveTop(0);
        }
        if (newGeometry.right() > width()) {
            newGeometry.moveRight(width());
        }
        if (newGeometry.bottom() > height()) {
            newGeometry.moveBottom(height());
        }
        
        m_activeView->geometry = newGeometry;
        m_activeView->widget->setGeometry(newGeometry);
    }
    else if (m_interactionMode == RESIZING) {
        // Resize the widget
        applyResize(m_activeView, delta, m_currentHandle);
    }
    
    m_lastMousePos = event->pos();
    update();
    event->accept();
}

void MultiPlotContainer::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_interactionMode != NONE) {
        m_interactionMode = NONE;
        m_currentHandle = NO_HANDLE;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void MultiPlotContainer::keyPressEvent(QKeyEvent* event)
{
    
    if (event->key() == Qt::Key_Meta || event->key() == Qt::Key_Control) { // Cmd on Mac, Ctrl on others
        m_cmdPressed = true;
        setMouseTracking(true);
        update();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void MultiPlotContainer::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Meta || event->key() == Qt::Key_Control) {
        m_cmdPressed = false;
        m_interactionMode = NONE;
        m_currentHandle = NO_HANDLE;
        m_activeView = nullptr;
        
        // Clear selections
        for (auto& info : m_plotViewInfos) {
            info.isSelected = false;
        }
        
        setCursor(Qt::ArrowCursor);
        update();
        event->accept();
    } else {
        QWidget::keyReleaseEvent(event);
    }
}

void MultiPlotContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    // Update widget geometries to stay within bounds
    for (auto& info : m_plotViewInfos) {
        QRect& geom = info.geometry;
        
        // Ensure widget stays within container bounds
        geom.setRight(qMin(width(), geom.right()));
        geom.setBottom(qMin(height(), geom.bottom()));
        
        ensureMinimumSize(geom);
        info.widget->setGeometry(geom);
    }
}

MultiPlotContainer::PlotViewInfo* MultiPlotContainer::getPlotViewAt(const QPoint& pos)
{
    // Search in reverse order (top widgets first)
    for (int i = m_plotViewInfos.size() - 1; i >= 0; --i) {
        if (m_plotViewInfos[i].geometry.contains(pos)) {
            return &m_plotViewInfos[i];
        }
    }
    return nullptr;
}

MultiPlotContainer::ResizeHandle MultiPlotContainer::getResizeHandle(const PlotViewInfo* info, const QPoint& pos)
{
    if (!info) return NO_HANDLE;
    
    const QRect& geom = info->geometry;
    const int margin = HANDLE_SIZE;
    
    bool left = pos.x() >= geom.left() && pos.x() <= geom.left() + margin;
    bool right = pos.x() >= geom.right() - margin && pos.x() <= geom.right();
    bool top = pos.y() >= geom.top() && pos.y() <= geom.top() + margin;
    bool bottom = pos.y() >= geom.bottom() - margin && pos.y() <= geom.bottom();
    
    if (top && left) return TOP_LEFT;
    if (top && right) return TOP_RIGHT;
    if (bottom && left) return BOTTOM_LEFT;
    if (bottom && right) return BOTTOM_RIGHT;
    if (left) return LEFT_EDGE;
    if (right) return RIGHT_EDGE;
    if (top) return TOP_EDGE;
    if (bottom) return BOTTOM_EDGE;
    
    return NO_HANDLE;
}

void MultiPlotContainer::updateCursor(const QPoint& pos)
{
    if (!m_cmdPressed) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    PlotViewInfo* viewInfo = getPlotViewAt(pos);
    if (!viewInfo) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    ResizeHandle handle = getResizeHandle(viewInfo, pos);
    
    switch (handle) {
    case TOP_LEFT:
    case BOTTOM_RIGHT:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TOP_RIGHT:
    case BOTTOM_LEFT:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case LEFT_EDGE:
    case RIGHT_EDGE:
        setCursor(Qt::SizeHorCursor);
        break;
    case TOP_EDGE:
    case BOTTOM_EDGE:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        setCursor(Qt::SizeAllCursor); // Move cursor for center area
        break;
    }
}

void MultiPlotContainer::drawResizeHandles(QPainter& painter, const QRect& geometry)
{
    painter.setPen(QPen(Qt::blue, 2));
    painter.setBrush(QBrush(Qt::white));
    
    const int h = HANDLE_SIZE;
    
    // Corner handles
    painter.drawRect(geometry.left() - h/2, geometry.top() - h/2, h, h);
    painter.drawRect(geometry.right() - h/2, geometry.top() - h/2, h, h);
    painter.drawRect(geometry.left() - h/2, geometry.bottom() - h/2, h, h);
    painter.drawRect(geometry.right() - h/2, geometry.bottom() - h/2, h, h);
    
    // Edge handles
    painter.drawRect(geometry.left() - h/2, geometry.center().y() - h/2, h, h);
    painter.drawRect(geometry.right() - h/2, geometry.center().y() - h/2, h, h);
    painter.drawRect(geometry.center().x() - h/2, geometry.top() - h/2, h, h);
    painter.drawRect(geometry.center().x() - h/2, geometry.bottom() - h/2, h, h);
    
    // Selection border
    painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(geometry);
}

void MultiPlotContainer::applyResize(PlotViewInfo* info, const QPoint& delta, ResizeHandle handle)
{
    if (!info) return;
    
    QRect& geom = info->geometry;
    
    switch (handle) {
    case TOP_LEFT:
        geom.setTopLeft(geom.topLeft() + delta);
        break;
    case TOP_RIGHT:
        geom.setTopRight(geom.topRight() + delta);
        break;
    case BOTTOM_LEFT:
        geom.setBottomLeft(geom.bottomLeft() + delta);
        break;
    case BOTTOM_RIGHT:
        geom.setBottomRight(geom.bottomRight() + delta);
        break;
    case LEFT_EDGE:
        geom.setLeft(geom.left() + delta.x());
        break;
    case RIGHT_EDGE:
        geom.setRight(geom.right() + delta.x());
        break;
    case TOP_EDGE:
        geom.setTop(geom.top() + delta.y());
        break;
    case BOTTOM_EDGE:
        geom.setBottom(geom.bottom() + delta.y());
        break;
    default:
        break;
    }
    
    ensureMinimumSize(geom);
    
    // Keep within container bounds
    geom.setLeft(qMax(0, geom.left()));
    geom.setTop(qMax(0, geom.top()));
    geom.setRight(qMin(width(), geom.right()));
    geom.setBottom(qMin(height(), geom.bottom()));
    
    info->widget->setGeometry(geom);
}

void MultiPlotContainer::ensureMinimumSize(QRect& geometry)
{
    if (geometry.width() < MIN_WIDGET_WIDTH) {
        geometry.setWidth(MIN_WIDGET_WIDTH);
    }
    if (geometry.height() < MIN_WIDGET_HEIGHT) {
        geometry.setHeight(MIN_WIDGET_HEIGHT);
    }
}

bool MultiPlotContainer::eventFilter(QObject* object, QEvent* event)
{
    // Intercept mouse events from child PlotView widgets when Cmd is pressed
    if (m_cmdPressed && qobject_cast<PlotView*>(object)) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            // Convert coordinates from child widget to container coordinates
            PlotView* plotView = qobject_cast<PlotView*>(object);
            QPoint containerPos = plotView->mapToParent(mouseEvent->pos());
            
            
            // Create a new mouse event with container coordinates and send to container
            QMouseEvent containerEvent(mouseEvent->type(), containerPos, containerPos, 
                                     mouseEvent->button(), mouseEvent->buttons(), mouseEvent->modifiers());
            mousePressEvent(&containerEvent);
            return true; // Event handled
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            PlotView* plotView = qobject_cast<PlotView*>(object);
            QPoint containerPos = plotView->mapToParent(mouseEvent->pos());
            
            QMouseEvent containerEvent(mouseEvent->type(), containerPos, containerPos, 
                                     mouseEvent->button(), mouseEvent->buttons(), mouseEvent->modifiers());
            mouseMoveEvent(&containerEvent);
            return true; // Event handled
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            PlotView* plotView = qobject_cast<PlotView*>(object);
            QPoint containerPos = plotView->mapToParent(mouseEvent->pos());
            
            QMouseEvent containerEvent(mouseEvent->type(), containerPos, containerPos, 
                                     mouseEvent->button(), mouseEvent->buttons(), mouseEvent->modifiers());
            mouseReleaseEvent(&containerEvent);
            return true; // Event handled
        }
    }
    
    // Pass the event to the base class
    return QWidget::eventFilter(object, event);
}