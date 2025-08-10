
#include "AppInstance.h"
#include <QApplication>

#ifdef Q_OS_WIN
#include "mainwindow.h"
#elifdef Q_OS_ANDROID
#include "androidmainwindow.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
extern "C" {
#include "Source/Libs/ffmpeg/build/arm64-v8a/include/libavformat/avformat.h"
#include "Source/Libs/ffmpeg/build/arm64-v8a/include/libavcodec/avcodec.h"
}
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AppInstance *w = AppInstance::getInstance();

#ifdef Q_OS_WIN

    mainWindow* win = new mainWindow(w);
    win->show();
#elifdef  Q_OS_ANDROID

    androidMainWindow* win = new androidMainWindow();
    win->show();

    const char* filename = "/storage/emulated/0/Music/Primorose.mp3";

    AVFormatContext* fmt_ctx = nullptr;

    int ret = avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qDebug() << "Open file error:" << errbuf;
    }

    qDebug() << "FFmpeg open file succes";

    avformat_close_input(&fmt_ctx);

    //QQmlApplicationEngine engine;
    //engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    //if (engine.rootObjects().isEmpty())
      //  return -1;

#endif

    return a.exec();
}
