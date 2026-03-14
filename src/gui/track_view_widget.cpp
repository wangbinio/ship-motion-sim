#include "gui/track_view_widget.h"

#include <algorithm>
#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace ship_sim {

namespace {

struct Bounds {
    double min_x {};
    double max_x {};
    double min_y {};
    double max_y {};
};

Bounds computeBounds(const std::vector<ShipState>& states) {
    Bounds bounds {
        states.front().lon_deg,
        states.front().lon_deg,
        states.front().lat_deg,
        states.front().lat_deg,
    };
    for (const auto& state : states) {
        bounds.min_x = std::min(bounds.min_x, state.lon_deg);
        bounds.max_x = std::max(bounds.max_x, state.lon_deg);
        bounds.min_y = std::min(bounds.min_y, state.lat_deg);
        bounds.max_y = std::max(bounds.max_y, state.lat_deg);
    }
    if (bounds.max_x - bounds.min_x < 1e-6) {
        bounds.min_x -= 0.0005;
        bounds.max_x += 0.0005;
    }
    if (bounds.max_y - bounds.min_y < 1e-6) {
        bounds.min_y -= 0.0005;
        bounds.max_y += 0.0005;
    }
    return bounds;
}

QPointF mapToCanvas(const ShipState& state, const Bounds& bounds, const QRectF& canvas) {
    const double x_ratio = (state.lon_deg - bounds.min_x) / (bounds.max_x - bounds.min_x);
    const double y_ratio = (state.lat_deg - bounds.min_y) / (bounds.max_y - bounds.min_y);
    return QPointF(
        canvas.left() + x_ratio * canvas.width(),
        canvas.bottom() - y_ratio * canvas.height());
}

}  // namespace

TrackViewWidget::TrackViewWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(true);
}

void TrackViewWidget::setTrackData(const std::vector<ShipState>& states, const CommandEvents& commands) {
    states_ = states;
    commands_ = commands;
    update();
}

QSize TrackViewWidget::minimumSizeHint() const {
    return QSize(480, 320);
}

void TrackViewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#dbeafe"));

    const QRectF canvas = rect().adjusted(24, 24, -24, -24);
    painter.setPen(QPen(QColor("#93c5fd"), 1, Qt::DashLine));
    for (int i = 0; i <= 5; ++i) {
        const double x = canvas.left() + canvas.width() * static_cast<double>(i) / 5.0;
        const double y = canvas.top() + canvas.height() * static_cast<double>(i) / 5.0;
        painter.drawLine(QPointF(x, canvas.top()), QPointF(x, canvas.bottom()));
        painter.drawLine(QPointF(canvas.left(), y), QPointF(canvas.right(), y));
    }

    painter.setPen(QPen(QColor("#1f2937"), 1));
    painter.drawRect(canvas);

    if (states_.empty()) {
        painter.setPen(QPen(QColor("#1f2937"), 1));
        painter.drawText(rect(), Qt::AlignCenter, tr("No track data"));
        return;
    }

    const Bounds bounds = computeBounds(states_);
    QPainterPath path;
    path.moveTo(mapToCanvas(states_.front(), bounds, canvas));
    for (std::size_t index = 1; index < states_.size(); ++index) {
        path.lineTo(mapToCanvas(states_[index], bounds, canvas));
    }

    painter.setPen(QPen(QColor("#0f766e"), 2));
    painter.drawPath(path);

    const QPointF start_point = mapToCanvas(states_.front(), bounds, canvas);
    const QPointF end_point = mapToCanvas(states_.back(), bounds, canvas);
    painter.setBrush(QColor("#16a34a"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(start_point, 5, 5);
    painter.setBrush(QColor("#dc2626"));
    painter.drawEllipse(end_point, 6, 6);

    const ShipState& current_state = states_.back();
    const QPointF heading_origin = mapToCanvas(current_state, bounds, canvas);
    const double heading_rad = current_state.heading_deg * 3.14159265358979323846 / 180.0;
    const QPointF heading_tip(
        heading_origin.x() + 18.0 * std::cos(heading_rad),
        heading_origin.y() - 18.0 * std::sin(heading_rad));
    painter.setPen(QPen(QColor("#111827"), 2));
    painter.drawLine(heading_origin, heading_tip);

    painter.setPen(QPen(QColor("#2563eb"), 1));
    painter.setBrush(QColor("#2563eb"));
    for (const auto& command : commands_) {
        auto it = std::lower_bound(
            states_.begin(),
            states_.end(),
            command.time_s,
            [](const ShipState& state, const double time_s) { return state.time_s < time_s; });
        if (it == states_.end()) {
            continue;
        }
        painter.drawEllipse(mapToCanvas(*it, bounds, canvas), 3, 3);
    }
}

}  // namespace ship_sim
