//
// Created by Stalker7274 on 27.03.2025.
//

#ifndef YTSEARCHERSUB_H
#define YTSEARCHERSUB_H

#include <QProcess>


class ytSearcherSub : public QObject{
    Q_OBJECT
public:
    explicit ytSearcherSub(QObject *parent = nullptr);

    ~ytSearcherSub() override;

    void search(const QString &Text);

    void download(QUrl url);

private slots:

    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:

    void parseResults(const QByteArray &data);

    QProcess *process;


};
#endif //YTSEARCHERSUB_H