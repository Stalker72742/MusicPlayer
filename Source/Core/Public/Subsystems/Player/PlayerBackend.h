//
// Created by Stalker7274 on 08.09.2025.
//

#ifndef MUSICPLAYER_IPLAYERBACKEND_H
#define MUSICPLAYER_IPLAYERBACKEND_H
#include <QObject>

class QString;

class playerBackend : public QObject {
  Q_OBJECT

public:
  explicit playerBackend(QObject* parent);

  virtual void play() = 0;
  virtual void pause();
  virtual void unpause();
  virtual void setSource(QString Path);
  virtual void setVolume(float volume);
};

#endif // MUSICPLAYER_IPLAYERBACKEND_H
