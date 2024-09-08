#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFontMetrics>
#include "debug.h"
#include "config.h"

MainWindow::MainWindow(QApplication* app, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , application(app)
    , lyric(new LyricWidget(this))
{
    ui->setupUi(this);
    ui->verticalLayout->addWidget(lyric);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool | Qt::WindowTransparentForInput);

    connect(this, &MainWindow::needReloadSetting, this, &MainWindow::reloadSetting);
    connect(this, &MainWindow::reciveCommand, this, &MainWindow::handleCommand);

    // setup server
    server = new QUdpSocket(this);
    if (!server->bind(QHostAddress::LocalHost, PORT))
    {
        qInfo() << "已经有个实例正在运行了！";
        server->close();
        delete server;
        server = nullptr;
        return;
    }
    DEBUG("Running at port" << PORT);
    connect(server, SIGNAL(readyRead()), this, SLOT(receiveLyric()));

    emit needReloadSetting();
}

MainWindow::~MainWindow()
{
    delete ui;

    if (server) {
        server->close();
        delete server;
    }

    delete lyric;
}

bool MainWindow::isRunning()
{
    return server != nullptr;
}

void MainWindow::reloadSetting()
{
    DEBUG("reload setting");
    Setting setting;

    int w = QString::fromStdString(setting.get(KEY_WIDTH, "800")).toInt();
    int h = QString::fromStdString(setting.get(KEY_HEIGHT, "80")).toInt();

    setBaseSize(w, h);
    if (setting.has(KEY_POS_X))
    {
        setGeometry(
            QString::fromStdString(setting.get(KEY_POS_X)).toInt(),
            QString::fromStdString(setting.get(KEY_POS_Y)).toInt(),
            w, h
        );
    }

    QFont font(QString::fromStdString(setting.get(KEY_FONT_FAMILY)));
    font.setBold(setting.getBool(KEY_FONT_BOLD));
    font.setItalic(setting.getBool(KEY_FONT_ITALIC));
    font.setPointSize(QString::fromStdString(setting.get(KEY_FONT_SIZE)).toInt());
    // setGeometry(0, 0, width(), QFontMetrics(font).height() * 2 + 10);
    defaultFont = QFont(font);

    lyric->setFont(defaultFont);
    lyric->setColor(QString::fromStdString(setting.get(KEY_FONT_COLOR)));
    lyric->setSpeed(11);
    lyric->setBaseSize(width(), height());

    setWindowFlag(Qt::WindowStaysOnTopHint, setting.getBool(KEY_FLAGS_STAY_ON_TOP));
    setWindowFlag(Qt::FramelessWindowHint, setting.getBool(KEY_FRAME_LESS));

    setVisible(true);

    // ui->labelLyric->setFont(font);
    // ui->labelLyric->setStyleSheet(QString("color: %1").arg(setting.get(KEY_FONT_COLOR, "#000000").c_str()));
}

void MainWindow::handleCommand(QString command)
{
    if (command == "LYRIC_CONFIG_RELOAD")
    {
        emit needReloadSetting();
        return;
    }

    if (command == "LYRIC_CONFIG_SAVE_SELF")
    {
        Setting s;
        s.put(KEY_FONT_SIZE, QString("%1").arg(defaultFont.pointSize()).toStdString());
        s.put(KEY_WIDTH, QString("%1").arg(width()).toStdString());
        s.put(KEY_HEIGHT, QString("%1").arg(height()).toStdString());
        s.put(KEY_POS_X, QString("%1").arg(x()).toStdString());
        s.put(KEY_POS_Y, QString("%1").arg(y()).toStdString());
        s.store();
        this->lyric->setText("Saved!");
        this->lyric->tick();
        return;
    }

    if (command.startsWith("LYRIC_CONFIG_PREVIEW")) {
        this->lyric->setText(command.remove(0, 21));
        this->lyric->tick();
        return;
    }

    QStringList commands = command.split(" ");
    if (commands.isEmpty()) return;

    if (commands[0] == "LYRIC_CONFIG_FRAME_LESS" && commands.size() == 2)
    {
        setWindowFlag(Qt::FramelessWindowHint, commands[1] == "1");
        setVisible(true);
        return;
    }

    if (commands[0] == "LYRIC_CONFIG_STAY_ON_TOP" && commands.size() == 2)
    {
        setWindowFlag(Qt::WindowStaysOnBottomHint, commands[1] == "1");
        setVisible(true);
        return;
    }
}



void MainWindow::receiveLyric()
{
    QByteArray buffer;
    buffer.resize(server->pendingDatagramSize());
    server->readDatagram(buffer.data(), buffer.size());
    QString incoming = QString::fromStdString(buffer.toStdString());

    // handle command
    if (incoming.startsWith("LYRIC_CONFIG"))
    {
        DEBUG("Recived config signal.");
        emit reciveCommand(incoming);
        return;
    }

    // set lyric
    if (incoming != currentLyric)
    {
        currentLyric = incoming;
        lyric->setText(currentLyric);
        index = 0;
    }
    lyric->tick();
}


void MainWindow::closeEvent(QCloseEvent* event)
{
    application->exit(0);
}
