//
// Created by Stalker7274 on 27.03.2025.
//

#ifndef YTSEARCHERSUB_H
#define YTSEARCHERSUB_H

#include <QProcess>
#include <QJsonObject>


enum ESearchResultsState {

    startParsing,
    pending,
    finishParsing
};


class ytSearcherSub : public QObject{
    Q_OBJECT
public:
    explicit ytSearcherSub(QObject *parent = nullptr);

    ~ytSearcherSub() override;

    void search(const QString &Text);
    void download(QUrl url, QString out);

signals:

    void searchResults(QJsonObject json, ESearchResultsState searchState);

private slots:

    void searchFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:

    void parseSearchResults(const QByteArray &data);

    QProcess *process;


};
#endif //YTSEARCHERSUB_H
