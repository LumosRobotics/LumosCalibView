#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QDataStream>
#include <vector>

struct DataPoint {
    double timestamp;
    float value;
    int channel = 0;  // For multi-channel data
    
    DataPoint() = default;
    DataPoint(double t, float v, int ch = 0) : timestamp(t), value(v), channel(ch) {}
};

class DataReceiver : public QObject
{
    Q_OBJECT

public:
    explicit DataReceiver(QObject *parent = nullptr);
    ~DataReceiver();

    // Connection management
    void startServer(quint16 port = 8080);
    void stopServer();
    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();
    
    // Configuration
    void setMaxDataPoints(int maxPoints) { m_maxDataPoints = maxPoints; }
    void setUpdateInterval(int msec) { m_updateTimer->setInterval(msec); }
    
    // Data access (thread-safe)
    std::vector<DataPoint> getLatestData();
    void clearData();
    
    bool isConnected() const;

public slots:
    void startReceiving();
    void stopReceiving();

signals:
    void dataReceived(const DataPoint& point);
    void newDataAvailable();
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString& error);

private slots:
    void onNewConnection();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onDataReady();
    void processReceivedData();

private:
    void processIncomingData(const QByteArray& data);
    void addDataPoint(const DataPoint& point);
    
    // Network
    QTcpServer* m_server;
    QTcpSocket* m_socket;
    QByteArray m_dataBuffer;
    
    // Data storage (thread-safe)
    mutable QMutex m_dataMutex;
    QQueue<DataPoint> m_dataQueue;
    int m_maxDataPoints;
    
    // Processing
    QTimer* m_updateTimer;
    bool m_isReceiving;
    
    // Connection state
    bool m_isServer;
    QString m_hostAddress;
    quint16 m_port;
};

class DataReceiverWorker : public QObject
{
    Q_OBJECT

public:
    explicit DataReceiverWorker(QObject *parent = nullptr);

public slots:
    void doWork();
    void stop();

signals:
    void dataReceived(const DataPoint& point);
    void finished();

private:
    bool m_running;
    DataReceiver* m_receiver;
};