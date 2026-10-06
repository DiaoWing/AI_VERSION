#include "layoutmanager.h"

LayoutManager::LayoutManager(QWidget* container, QObject* parent)
    : QObject(parent)
    , m_container(container)
    , m_currentMode(Mode_Control)
{
    if (!m_container) {
        qWarning() << "LayoutManager: Container is null!";
        return;
    }

    m_lastSize = m_container->size();
    qDebug() << "LayoutManager created, container size:" << m_lastSize;
}

LayoutManager::~LayoutManager()
{
}

void LayoutManager::registerWidget(QWidget* widget, const QString& name)
{
    if (!widget || name.isEmpty()) {
        qWarning() << "Invalid widget or name";
        return;
    }

    if (m_widgets.contains(name)) {
        qWarning() << "Widget already registered:" << name;
        return;
    }

    m_widgets[name] = widget;
    widget->setParent(m_container);

    // 关键：设置为可调整大小
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    qDebug() << "Registered widget:" << name << "size:" << widget->size();
}

void LayoutManager::unregisterWidget(const QString& name)
{
    m_widgets.remove(name);
}

bool LayoutManager::addLayout(LayoutMode mode, const LayoutDefinition& layout)
{
    if (mode < 0 || mode >= Mode_Count) {
        qWarning() << "Invalid layout mode";
        return false;
    }

    m_layouts[mode] = layout;
    qDebug() << "Added layout:" << layout.description << "with" << layout.positions.size() << "widgets";
    return true;
}

bool LayoutManager::switchToMode(LayoutMode mode)
{
    if (mode < 0 || mode >= Mode_Count) {
        qWarning() << "Invalid mode";
        return false;
    }

    if (!m_layouts.contains(mode)) {
        qWarning() << "Layout not found for mode:" << mode;
        return false;
    }

    m_currentMode = mode;
    applyCurrentLayout();
    return true;
}

void LayoutManager::applyCurrentLayout()
{
    if (!m_layouts.contains(m_currentMode)) {
        qWarning() << "No layout for current mode";
        return;
    }

    const LayoutDefinition& layout = m_layouts[m_currentMode];
    applyLayout(layout);
}

void LayoutManager::applyLayout(const LayoutDefinition& layout)
{
    if (!m_container) {
        qWarning() << "Container is null";
        return;
    }

    QSize containerSize = m_container->size();
    if (containerSize.isEmpty()) {
        qWarning() << "Container size is empty, using default 1920x1080";
        containerSize = QSize(1920, 1080);
    }

    qDebug() << "=== Applying Layout: " << layout.description;
    qDebug() << "Container size:" << containerSize;
    qDebug() << "Widget count:" << layout.positions.size();

    // 先隐藏所有控件
    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }

    // 应用每个控件的位置
    for (const WidgetPosition& pos : layout.positions) {
        if (!m_widgets.contains(pos.widgetName)) {
            qWarning() << "Widget not found:" << pos.widgetName;
            continue;
        }

        QWidget* widget = m_widgets[pos.widgetName];
        if (!widget) continue;

        // 计算实际像素位置 (百分比)
        int x = pos.geometry.x() * containerSize.width() / 100;
        int y = pos.geometry.y() * containerSize.height() / 100;
        int w = pos.geometry.width() * containerSize.width() / 100;
        int h = pos.geometry.height() * containerSize.height() / 100;

        QRect rect(x, y, w, h);

        qDebug() << "Widget:" << pos.widgetName
                 << "Percent:" << pos.geometry
                 << "Pixel:" << rect
                 << "Visible:" << pos.visible;

        // 设置位置
        widget->setGeometry(rect);
        widget->setVisible(pos.visible);

        if (pos.zOrder > 0) {
            widget->raise();
        }
    }

    m_container->update();
    qDebug() << "=== Layout applied successfully";
}

void LayoutManager::onContainerResized()
{
    if (!m_container) return;

    QSize newSize = m_container->size();
    if (newSize == m_lastSize) return;

    m_lastSize = newSize;
    qDebug() << "Container resized to:" << newSize;

    applyCurrentLayout();
}
