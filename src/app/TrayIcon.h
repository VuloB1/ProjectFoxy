#pragma once

#include <QObject>
#include <QString>

class QQuickWindow;

// Windows only: an icon in the notification area. Left click (or double click) asks for the window back; the right
// click menu has "open" and "quit". Elsewhere `available` is false and nothing is shown.
class TrayIcon : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString openText MEMBER m_openText)
    Q_PROPERTY(QString quitText MEMBER m_quitText)
    Q_PROPERTY(QString tip MEMBER m_tip)

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    bool available() const;
    bool visible() const { return m_visible; }
    void setVisible(bool on);
    // Gives the pages the program is not using back to Windows (what Task Manager counts); they come back on use.
    Q_INVOKABLE void trimMemory();
    // Off, the window's graphics (Direct3D device, textures) are freed while it is hidden and rebuilt when it is shown.
    Q_INVOKABLE void setGraphicsPersistent(QQuickWindow *window, bool on);

    struct Native;

signals:
    void visibleChanged();
    void activated();
    void quitRequested();

private:
    Native *m_native = nullptr;
    bool m_visible = false;
    QString m_openText = QStringLiteral("Open");
    QString m_quitText = QStringLiteral("Quit");
    QString m_tip = QStringLiteral("Project Foxy");
};
