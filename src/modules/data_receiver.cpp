#include "data_receiver.h"
#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

DataReceiver::DataReceiver(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_socket(nullptr)
    , m_maxDataPoints(10000)
    , m_isReceiving(false)
    , m_isServer(false)
    , m_port(8080)
{
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(16); // ~60 FPS updates
    connect(m_updateTimer, &QTimer::timeout, this, &DataReceiver::processReceivedData);
}

DataReceiver::~DataReceiver()
{
    stopReceiving();
    stopServer();
    disconnectFromHost();
}

void DataReceiver::startServer(quint16 port)
{
    stopServer(); // Stop any existing server
    
    m_server = new QTcpServer(this);
    m_port = port;
    m_isServer = true;
    
    connect(m_server, &QTcpServer::newConnection, this, &DataReceiver::onNewConnection);
    
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "TCP Server started on port" << port;
        emit connectionStatusChanged(false); // Not connected yet, just listening
    } else {
        emit errorOccurred(QString("Failed to start server: %1").arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
    }
}

void DataReceiver::stopServer()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
        qDebug() << "TCP Server stopped";
    }
}

void DataReceiver::connectToHost(const QString& host, quint16 port)
{
    disconnectFromHost(); // Disconnect any existing connection
    
    m_socket = new QTcpSocket(this);
    m_hostAddress = host;
    m_port = port;
    m_isServer = false;
    
    connect(m_socket, &QTcpSocket::connected, this, &DataReceiver::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &DataReceiver::onSocketDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &DataReceiver::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &DataReceiver::onDataReady);
    
    qDebug() << "Connecting to" << host << ":" << port;
    m_socket->connectToHost(host, port);
}

void DataReceiver::disconnectFromHost()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
        delete m_socket;
        m_socket = nullptr;
    }
}

std::vector<DataPoint> DataReceiver::getLatestData()
{
    QMutexLocker locker(&m_dataMutex);
    std::vector<DataPoint> result;
    result.reserve(m_dataQueue.size());
    
    for (const auto& point : m_dataQueue) {
        result.push_back(point);
    }
    
    return result;
}

void DataReceiver::clearData()
{
    QMutexLocker locker(&m_dataMutex);
    m_dataQueue.clear();
}

bool DataReceiver::isConnected() const
{
    if (m_socket) {
        return m_socket->state() == QAbstractSocket::ConnectedState;
    }
    return false;
}

void DataReceiver::startReceiving()
{
    if (!m_isReceiving) {
        m_isReceiving = true;
        m_updateTimer->start();
        qDebug() << "Started receiving data";
    }
}

void DataReceiver::stopReceiving()
{
    if (m_isReceiving) {
        m_isReceiving = false;
        m_updateTimer->stop();
        qDebug() << "Stopped receiving data";
    }
}

void DataReceiver::onNewConnection()
{
    if (m_server) {
        QTcpSocket* clientSocket = m_server->nextPendingConnection();
        
        // For simplicity, handle only one client at a time
        if (m_socket) {
            m_socket->deleteLater();
        }
        
        m_socket = clientSocket;
        
        connect(m_socket, &QTcpSocket::connected, this, &DataReceiver::onSocketConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &DataReceiver::onSocketDisconnected);
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                this, &DataReceiver::onSocketError);
        connect(m_socket, &QTcpSocket::readyRead, this, &DataReceiver::onDataReady);
        
        qDebug() << "Client connected:" << clientSocket->peerAddress().toString();
        emit connectionStatusChanged(true);
        
        if (!m_isReceiving) {
            startReceiving();
        }
    }
}

void DataReceiver::onSocketConnected()
{
    qDebug() << "Connected to server";
    emit connectionStatusChanged(true);
    
    if (!m_isReceiving) {
        startReceiving();
    }
}

void DataReceiver::onSocketDisconnected()
{
    qDebug() << "Disconnected from server";
    emit connectionStatusChanged(false);
    stopReceiving();
}

void DataReceiver::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_socket) {
        emit errorOccurred(QString("Socket error: %1").arg(m_socket->errorString()));
        qDebug() << "Socket error:" << m_socket->errorString();
    }
}

void DataReceiver::onDataReady()
{
    if (m_socket && m_socket->bytesAvailable() > 0) {
        QByteArray newData = m_socket->readAll();
        m_dataBuffer.append(newData);
        
        // Process complete messages (assuming newline-delimited JSON)
        while (m_dataBuffer.contains('\n')) {
            int index = m_dataBuffer.indexOf('\n');
            QByteArray message = m_dataBuffer.left(index);
            m_dataBuffer.remove(0, index + 1);
            
            processIncomingData(message);
        }
    }
}

void DataReceiver::processIncomingData(const QByteArray& data)
{
    // Try to parse as JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        // Try simple format: "timestamp,value" or "timestamp,value,channel"
        QString str = QString::fromUtf8(data).trimmed();
        QStringList parts = str.split(',');
        
        if (parts.size() >= 2) {
            bool ok1, ok2;
            double timestamp = parts[0].toDouble(&ok1);
            float value = parts[1].toFloat(&ok2);
            
            if (ok1 && ok2) {
                int channel = 0;
                if (parts.size() > 2) {
                    channel = parts[2].toInt();
                }
                
                DataPoint point(timestamp, value, channel);
                addDataPoint(point);
                emit dataReceived(point);
            }
        }
        return;
    }
    
    // Parse JSON format
    QJsonObject obj = doc.object();
    if (obj.contains("timestamp") && obj.contains("value")) {
        double timestamp = obj["timestamp"].toDouble();
        float value = static_cast<float>(obj["value"].toDouble());
        int channel = obj.value("channel").toInt(0);
        
        DataPoint point(timestamp, value, channel);
        addDataPoint(point);
        emit dataReceived(point);
    }
}

void DataReceiver::addDataPoint(const DataPoint& point)
{
    QMutexLocker locker(&m_dataMutex);
    
    m_dataQueue.enqueue(point);
    
    // Limit queue size
    while (m_dataQueue.size() > m_maxDataPoints) {
        m_dataQueue.dequeue();
    }
}

void DataReceiver::processReceivedData()
{
    if (!m_dataQueue.isEmpty()) {
        emit newDataAvailable();
    }
}

// Worker class implementation
DataReceiverWorker::DataReceiverWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_receiver(nullptr)
{
}

void DataReceiverWorker::doWork()
{
    m_running = true;
    m_receiver = new DataReceiver(this);
    
    connect(m_receiver, &DataReceiver::dataReceived, this, &DataReceiverWorker::dataReceived);
    
    // Start server or connect to host based on configuration
    m_receiver->startServer(8080);
    
    // Keep the thread alive
    while (m_running) {
        QThread::msleep(100);
    }
    
    delete m_receiver;
    m_receiver = nullptr;
    emit finished();
}

void DataReceiverWorker::stop()
{
    m_running = false;
}