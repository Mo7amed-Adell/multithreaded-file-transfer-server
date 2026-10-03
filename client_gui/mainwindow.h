#pragma once
#include <QWidget>
#include <QAbstractSocket>
// forward declarations
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QTcpSocket;  
class QFile;

class MainWindow : public QWidget {
 Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
private slots:
    void onDownloadClicked();
    void onConnected();                  
    void onDisconnected(); 
    void onReadyRead();   
    void onErrorOccurred(QAbstractSocket::SocketError socketError); 

private:
    QLineEdit    *filenameEdit;
    QPushButton  *downloadButton;
    QProgressBar *progressBar;
    QLabel       *statusLabel;
    QTcpSocket   *socket;
    QFile        *outputFile = nullptr;   
    bool          headerReceived = false; 
    qint64        expectedSize = 0;       
    qint64        bytesReceived = 0;      
    QByteArray    headerBuffer;
};