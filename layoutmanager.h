#ifndef LAYOUTMANAGER_H
#define LAYOUTMANAGER_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QList>
#include <QRect>
#include <QDebug>

class LayoutManager : public QObject
{
    Q_OBJECT

public:
    enum LayoutMode {
        Mode_Control = 0,
        Mode_Fullscreen,
        Mode_Count
    };

    struct WidgetPosition {
        QString widgetName;
        QRect geometry;  // 百分比 (0-100)
        bool visible;
        int zOrder;

        WidgetPosition() : visible(true), zOrder(0) {}
    };

    struct LayoutDefinition {
        LayoutMode mode;
        QString description;
        QList<WidgetPosition> positions;
        LayoutDefinition() : mode(Mode_Control) {}
    };

public:
    explicit LayoutManager(QWidget* container, QObject* parent = nullptr);
    ~LayoutManager();

    void registerWidget(QWidget* widget, const QString& name);
    void unregisterWidget(const QString& name);

    bool addLayout(LayoutMode mode, const LayoutDefinition& layout);
    bool switchToMode(LayoutMode mode);
    LayoutMode currentMode() const { return m_currentMode; }

    void applyCurrentLayout();
    void onContainerResized();

private:
    void applyLayout(const LayoutDefinition& layout);

private:
    QWidget* m_container;
    LayoutMode m_currentMode;
    QMap<QString, QWidget*> m_widgets;
    QMap<LayoutMode, LayoutDefinition> m_layouts;
    QSize m_lastSize;
};

#endif // LAYOUTMANAGER_H
