#pragma once

#include <QObject>
#include <QStringList>

// What Qt Multimedia can read as video on this computer (its FFmpeg backend): the "video to GIF" feature starts from
// here. Asked lazily: building the list loads the multimedia plugin, which the program only needs when a video is
// involved.
class VideoSupport : public QObject {
    Q_OBJECT
    // A video can be decoded at all.
    Q_PROPERTY(bool available READ available CONSTANT)
    // File extensions of the containers that can be decoded (lower case, no dot): mp4, m4v, mov, webm, mkv, avi...
    Q_PROPERTY(QStringList extensions READ extensions CONSTANT)

public:
    explicit VideoSupport(QObject *parent = nullptr) : QObject(parent) {}

    // Whether the file name has the extension of a video (mp4, mov, mkv...): only a look at the name, it does not load
    // the multimedia plugin; whether this computer can read that video is what extensions() tells.
    static bool looksLikeVideo(const QString &path);

    bool available() const { return !extensions().isEmpty(); }
    QStringList extensions() const;

private:
    mutable QStringList m_extensions;
    mutable bool m_known = false;
};
