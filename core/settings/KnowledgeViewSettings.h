#pragma once
#include <QObject>
class KnowledgeViewSettings : public QObject {
    Q_OBJECT
public:
    static KnowledgeViewSettings &shared();
    int zoomPercent() const;
    void setZoomPercent(int percent);
signals:
    void zoomChanged(int percent);
private:
    KnowledgeViewSettings() = default;
};
