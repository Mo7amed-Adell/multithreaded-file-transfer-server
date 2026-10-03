#include "mainwindow.h"
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QTcpSocket>
#include <QFile>



MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("File Transfer Client");

    filenameEdit   = new QLineEdit(this);
    filenameEdit->setPlaceholderText("Filename on the server, e.g. test.txt");

    downloadButton = new QPushButton("Download", this);

    progressBar    = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    statusLabel    = new QLabel("Idle", this);
   
    auto *layout = new QVBoxLayout(this);   // "this" = the layout manages this window
    layout->addWidget(filenameEdit);
    layout->addWidget(downloadButton);
    layout->addWidget(progressBar);
    layout->addWidget(statusLabel);

    socket = new QTcpSocket(this); 

     connect(downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
     connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
     connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
     connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
     connect(socket, &QTcpSocket::errorOccurred, this,&MainWindow::onErrorOccurred);
}

void MainWindow::onDownloadClicked() {
    statusLabel->setText("Connecting...");
    socket->connectToHost("127.0.0.1", 9000);   
}

void MainWindow::onConnected() {
    statusLabel->setText("Connected! Requesting file...");

    headerReceived = false;
    bytesReceived = 0;
    expectedSize = 0;
    headerBuffer.clear();

    QString filename = filenameEdit->text();
    QString request = "GET " + filename + "\n";
    socket->write(request.toUtf8());
}

void MainWindow::onDisconnected() {
    if (outputFile) {
        outputFile->close();
        delete outputFile;
        outputFile = nullptr;
    }
    if (bytesReceived == expectedSize && expectedSize > 0) {
        statusLabel->setText("Download complete!");
    } else {
        statusLabel->setText("Disconnected (incomplete transfer).");
    }
}

void MainWindow::onReadyRead() {
    if (!headerReceived) {
        headerBuffer.append(socket->readAll());

        int newlineIndex = headerBuffer.indexOf('\n');
        if (newlineIndex == -1) {
            return;   // header not complete yet, wait for more data
        }

        QByteArray headerLine = headerBuffer.left(newlineIndex);
        QByteArray remainder  = headerBuffer.mid(newlineIndex + 1);  // bytes after the header = start of file body

        QString header = QString::fromUtf8(headerLine);
        if (header.startsWith("OK ")) {
            expectedSize = header.mid(3).toLongLong();
            outputFile = new QFile(filenameEdit->text(), this);
            if (!outputFile->open(QIODevice::WriteOnly)) {
                statusLabel->setText("Failed to open file for writing!");
                socket->disconnectFromHost();
                return;
            }
            headerReceived = true;
            statusLabel->setText("Downloading...");

            if (!remainder.isEmpty()) {
                outputFile->write(remainder);
                bytesReceived += remainder.size();
                progressBar->setValue(static_cast<int>(100.0 * bytesReceived / expectedSize));
            }
        } else {
            statusLabel->setText("Server error: " + header);
            socket->disconnectFromHost();
        }
        return;
    }

    // header already parsed — everything from here on is pure file data
    QByteArray chunk = socket->readAll();
    outputFile->write(chunk);
    bytesReceived += chunk.size();
    progressBar->setValue(static_cast<int>(100.0 * bytesReceived / expectedSize));
}
void MainWindow::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    statusLabel->setText("Connection error: " + socket->errorString());
      if (outputFile) {
        outputFile->close();
        delete outputFile;
        outputFile = nullptr;
    }
}
