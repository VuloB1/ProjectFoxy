#pragma once

#include <QObject>
#include <QString>

// The eyedropper of the colour readout: whether Alt is held, the eyedropper mouse cursor, a colour as text
// ("#RRGGBB") and copying it to the clipboard. The page (qml/ImageCanvas.qml) decides when to use each.
class ColorPicker : public QObject {
    Q_OBJECT

public:
    explicit ColorPicker(QObject *parent = nullptr) : QObject(parent) {}
    ~ColorPicker() override;

    // Alt is held right now (polled: a key press alone sends no mouse event the page could react to).
    Q_INVOKABLE bool altDown() const;
    // Swaps the mouse cursor for an eyedropper (the tip is the hot spot) and back.
    Q_INVOKABLE void setCursor(bool eyedropper);
    // "#RRGGBB", or "#RRGGBBAA" when it is not opaque.
    Q_INVOKABLE QString hex(int r, int g, int b, int a = 255) const;
    // Puts `text` on the clipboard.
    Q_INVOKABLE void copyText(const QString &text) const;

private:
    bool m_cursorOn = false;
};
