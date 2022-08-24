#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include "client.h"

//--------------------------------------------------------------------------

FileWrHelper::FileWrHelper():
        file{},
        declared_size{},
        already_got_header{}
{}

//--------------------------------------------------------------------------

void FileWrHelper::clear() {
    file.close();
    file.fileName().clear();
    declared_size = 0;
    already_got_header = false;
}

//--------------------------------------------------------------------------

bool FileWrHelper::writeFile(const char* buffer, ssize_t size) {

    if(file.fileName().isEmpty() || size == 0)
        return false;

    if(!file.isOpen()) {
        if(!file.open(QIODevice::WriteOnly))
            return false;
    }

    file.write(buffer, size);

    return true;
}
//--------------------------------------------------------------------------

Client::Client(const QString& remote_host, uint remote_port, QWidget *parent)
    : QWidget(parent),
      socket_{ new QTcpSocket(this) },
      txt_info_{ new QTextEdit("Hello. Click \'Connect\' to continue.", this) },
      txt_input_{ new QLineEdit(remote_host + " " + QString::number(remote_port), this) },
      label_{ new QLabel("Not connected.", this) },
      push_button_{ new QPushButton ("Connect", this)},
      writer_helper{}
{

    connect(socket_, SIGNAL(connected()),
            SLOT(slotConnected()));
    connect(socket_, SIGNAL(readyRead()),
            SLOT(slotReadyRead()));
    connect(socket_, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(slotError(QAbstractSocket::SocketError)));
    connect(socket_, SIGNAL(disconnected()),
            SLOT(slotDisconnected()));

    txt_info_->setReadOnly(true);

    connect(push_button_, SIGNAL(clicked()),
            SLOT(pushButtonClicked()));
    connect(txt_input_, SIGNAL(returnPressed()), this,
            SLOT(pushButtonClicked()));

    QVBoxLayout* vbox_layout = new QVBoxLayout;

    vbox_layout->addWidget(label_);
    vbox_layout->addWidget(txt_info_);
    vbox_layout->addWidget(txt_input_);
    vbox_layout->addWidget(push_button_);

    setLayout(vbox_layout);
}

//--------------------------------------------------------------------------

bool Client::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

//--------------------------------------------------------------------------

void Client::connectToServer(const QString& host, uint port) {
   if (socket_ == nullptr)
       return;
   if(host.isEmpty())
       return;
   if(port == 0)
       return;

   socket_->connectToHost(host, port);
}

//--------------------------------------------------------------------------

void Client::slotError(QAbstractSocket::SocketError err) {
    QString str_error = "Error: " +
            (err == QAbstractSocket::HostNotFoundError ?
            "The host was not found.":
            err == QAbstractSocket::RemoteHostClosedError ?
            "The remote host is closed.":
            err == QAbstractSocket::ConnectionRefusedError ?
            "The connection was refused.":
            QString(socket_->errorString()));
}

//--------------------------------------------------------------------------

void Client::slotSendToServer() {
    QByteArray arr_block;
    QDataStream out_stream (&arr_block, QIODevice::WriteOnly);
    out_stream.setVersion(QDataStream::Qt_5_15);

    QString request{ txt_input_->text() };
    updateWrHelper(request);
    out_stream << request;

    socket_->write(arr_block);
    txt_input_->setText("");
}

//--------------------------------------------------------------------------

void Client::slotConnected() {
    QString connection_info{ "Connected to " + socket_->peerName() + " : "
                             + QString::number(socket_->peerPort()) };
    txt_info_->append(connection_info);
    label_->setText(connection_info);
    txt_input_->setText("POST /control/exit");
    push_button_->setText("Send");
}

//--------------------------------------------------------------------------

void Client::slotDisconnected(){
    if(!isConnected())
    {
        QString connection_info{ "Disconnected." };
        txt_info_->append(connection_info);
        label_->setText(connection_info);
        txt_input_->setText("localhost 51511");
        push_button_->setText("Connect");
    }
}

//--------------------------------------------------------------------------

void Client::pushButtonClicked() {
    if(isConnected())
    {
        slotSendToServer();
    }
    else
    {
        QString host{ txt_input_->text().section(" ", 0, 0) };
        QString port{ txt_input_->text().section(" ", 1, 1) };
        if (host.isEmpty() || port.isEmpty())
        {
            txt_info_->append("Hostname/ip and port should be separated with one space.");
            return;
        }
        connectToServer(host, port.toUInt());
    }
}
//--------------------------------------------------------------------------

const int Client::getFileSizeSentByServer(const QString &str) {
    int ret_val{};

    if(str.indexOf("Content-Length: ") != -1) {

        //("\nContent-length: ").length == 16
        QStringRef str_numbers(&str, 16,
    str.indexOf("\r\n", 16) - 16);

        ret_val = str_numbers.toInt();
    }
    return ret_val;
}

//--------------------------------------------------------------------------

void Client::updateWrHelper(const QString &request) {

    writer_helper.clear();

    if(request.indexOf("GET") == 0)
    {
        QString full_path{ request.split(" ")[1]};
        QString filename{ full_path.mid(full_path.lastIndexOf("/") + 1) };

        int point_pos { filename.lastIndexOf(".") };
        if(point_pos != -1)
        {
            int extension_length{ filename.length() - point_pos};
            //Let us suppose that the extension length could be from one to four letters
            if(extension_length > 2 && extension_length < 6) {
                writer_helper.file.setFileName(filename);
            }
        }
    }
}

//--------------------------------------------------------------------------

void Client::slotReadyRead() {

    static bool is_200_OK{};
    while (socket_->bytesAvailable() > 0) {
        if (!writer_helper.already_got_header) {
            QString line{socket_->readLine()};

            if(line.indexOf("HTTP/1.1 200 OK") == 0)
                is_200_OK = true;

            txt_info_->append(line);

            if(is_200_OK)
            {
                if (writer_helper.declared_size == 0)
                    writer_helper.declared_size = getFileSizeSentByServer(line);

                if (line.indexOf("\r\n") == 0) {
                    writer_helper.already_got_header = true;
                    is_200_OK = false;
                }
            }
        } else {

            char temp_buff[1024]{};
            ssize_t block_size = socket_->read(temp_buff, sizeof (temp_buff));
            if(block_size == -1)
            {
                qCritical("Trying to read from closed (?) socket.");
                return;
            }
            if(writer_helper.writeFile(temp_buff, block_size)) {
                txt_info_->append("Got " + QString::number(block_size)
                    + " bytes. (Progress: " +
                        QString::number(writer_helper.file.size()*100/writer_helper.declared_size)
                            + " %");
            }
        }
    }

    if(writer_helper.file.size() != 0 &&
       (writer_helper.file.size() == writer_helper.declared_size))
    {
        txt_info_->append("File " + writer_helper.file.fileName()+
                          + " downloaded successfully.");
        writer_helper.clear();
    }

}

//--------------------------------------------------------------------------
