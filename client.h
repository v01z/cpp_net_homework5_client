#ifndef CLIENT_H
#define CLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFile>

//--------------------------------------------------------------------------

struct FileWrHelper{
    QFile file;
    int declared_size;
    bool already_got_header;

    FileWrHelper();
    void clear();
    bool writeFile(const char*, ssize_t);
};

//--------------------------------------------------------------------------

class Client : public QWidget
{
    Q_OBJECT

private:
    QTcpSocket* socket_;
    QTextEdit* txt_info_;
    QLineEdit* txt_input_;
    QLabel* label_;
    QPushButton* push_button_;
    FileWrHelper writer_helper;

    bool isConnected() const;
    void connectToServer(const QString&, uint);
    const int getFileSizeSentByServer(const QString&);
    void updateWrHelper(const QString&);

private slots:
    void slotReadyRead();
    void slotError(QAbstractSocket::SocketError);
    void slotSendToServer();
    void slotConnected();
    void slotDisconnected();
    void pushButtonClicked();

public:
    Client(const QString&, uint, QWidget *parent = nullptr);
    Client() = delete;
    ~Client() = default;
};

//--------------------------------------------------------------------------

#endif // CLIENT_H
