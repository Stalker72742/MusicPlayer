//
// Created by Stalker7274 on 08.09.2025.
//

#ifndef MUSICPLAYER_MULTIMEDIABACKEND_H
#define MUSICPLAYER_MULTIMEDIABACKEND_H
#include "PlayerBackend.h"

class MultimediaBackend : public playerBackend {
  Q_OBJECT
public:

  virtual void play() override;

};

#endif // MUSICPLAYER_MULTIMEDIABACKEND_H
