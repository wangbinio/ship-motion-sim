#pragma once

#include <vector>

#include <QWidget>

#include "common/types.h"

namespace ship_sim {

class TrackViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackViewWidget(QWidget* parent = nullptr);

    void setTrackData(const std::vector<ShipState>& states, const CommandEvents& commands);
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<ShipState> states_;
    CommandEvents commands_;
};

}  // namespace ship_sim
