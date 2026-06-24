//
// Created by Stalker7274 on 21.11.2025.
//

#ifndef SOUNDLINK_SETTINGDATA_H
#define SOUNDLINK_SETTINGDATA_H

#include <QIODevice>
#include <QString>
#include "SettingType.h"

struct SettingData
{
    QString SettingName;
    ESettingType SettingType;

    QByteArray Value;

    SettingData()
    {
        SettingName = "";
        SettingType = ESettingType::None;
        Value = {};
    }

    SettingData(const QString& InSettingName, const ESettingType InSettingType, const QByteArray& InSettingValue)
    {
        SettingName = InSettingName;
        SettingType = InSettingType;
        Value = InSettingValue;
    }

    template <typename V>
    bool GetValue(V& OutValue)
    {
        if (Value.isEmpty())
            return false;

        QDataStream stream(Value);
        stream >> OutValue;

        return stream.status() == QDataStream::Ok;
    }

    template <typename V>
    void SetValue(V InValue)
    {
        Value.clear();
        QDataStream stream(&Value, QIODevice::WriteOnly);
        stream << InValue;
    }

    friend QDataStream& operator<<(QDataStream& stream, const SettingData& data)
    {
        stream << data.SettingName;
        stream << static_cast<int>(data.SettingType);
        stream << data.Value;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, SettingData& data)
    {
        int settingType;
        stream >> data.SettingName;
        stream >> settingType;
        data.SettingType = static_cast<ESettingType>(settingType);
        stream >> data.Value;
        return stream;
    }
};

#endif // SOUNDLINK_SETTINGDATA_H
