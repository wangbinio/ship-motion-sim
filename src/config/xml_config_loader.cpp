#include "config/xml_config_loader.h"

#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "common/math_utils.h"

namespace ship_sim
{

namespace
{

double parseRequiredDouble(const QDomElement& element, const char* attribute_name)
{
    bool ok = false;
    const double value = element.attribute(QString::fromLatin1(attribute_name)).toDouble(&ok);
    if (!ok)
    {
        throw std::runtime_error("Invalid or missing attribute '" + std::string(attribute_name) + "' on element '"
                                 + element.tagName().toStdString() + "'");
    }
    return value;
}

double parseOptionalDouble(const QDomElement& element, const char* attribute_name, const double default_value)
{
    const QString text = element.attribute(QString::fromLatin1(attribute_name));
    if (text.isEmpty())
    {
        return default_value;
    }

    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok)
    {
        throw std::runtime_error("Invalid attribute '" + std::string(attribute_name) + "' on element '"
                                 + element.tagName().toStdString() + "'");
    }
    return value;
}

int parseRequiredInt(const QDomElement& element, const char* attribute_name)
{
    bool ok = false;
    const int value = element.attribute(QString::fromLatin1(attribute_name)).toInt(&ok);
    if (!ok)
    {
        throw std::runtime_error("Invalid or missing attribute '" + std::string(attribute_name) + "' on element '"
                                 + element.tagName().toStdString() + "'");
    }
    return value;
}

std::string parseRequiredString(const QDomElement& element, const char* attribute_name)
{
    const QString value = element.attribute(QString::fromLatin1(attribute_name));
    if (value.isEmpty())
    {
        throw std::runtime_error("Missing attribute '" + std::string(attribute_name) + "' on element '"
                                 + element.tagName().toStdString() + "'");
    }
    return value.toStdString();
}

bool parseOptionalBool(const QDomElement& element, const char* attribute_name, const bool default_value)
{
    const QString value = element.attribute(QString::fromLatin1(attribute_name));
    if (value.isEmpty())
    {
        return default_value;
    }
    if (value == QStringLiteral("true") || value == QStringLiteral("1"))
    {
        return true;
    }
    if (value == QStringLiteral("false") || value == QStringLiteral("0"))
    {
        return false;
    }
    throw std::runtime_error("Invalid boolean attribute '" + std::string(attribute_name) + "' on element '"
                             + element.tagName().toStdString() + "'");
}

QDomElement requireChild(const QDomElement& parent, const char* tag_name)
{
    const QDomElement child = parent.firstChildElement(QString::fromLatin1(tag_name));
    if (child.isNull())
    {
        throw std::runtime_error("Missing element '" + std::string(tag_name) + "' under '"
                                 + parent.tagName().toStdString() + "'");
    }
    return child;
}

void validateSimulationConfig(const SimulationConfig& config)
{
    if (config.dt_s <= 0.0)
    {
        throw std::runtime_error("simulation.dt_s must be positive");
    }
    if (config.duration_s < 0.0)
    {
        throw std::runtime_error("simulation.duration_s must be non-negative");
    }
    if (config.earth_radius_m <= 0.0)
    {
        throw std::runtime_error("simulation.earth_radius_m must be positive");
    }
    if (config.nomoto_T_s <= 0.0)
    {
        throw std::runtime_error("model.nomoto_T_s must be positive");
    }
    if (config.rudder_full_effect_speed_mps <= 0.0)
    {
        throw std::runtime_error("model.rudder_full_effect_speed_mps must be positive");
    }
    if (config.speed_tau_s <= 0.0)
    {
        throw std::runtime_error("model.speed_tau_s must be positive");
    }
    if (config.rudder_limit_deg <= 0.0)
    {
        throw std::runtime_error("model.rudder_limit_deg must be positive");
    }
}

void validateInitialState(const InitialState& initial_state)
{
    const double lat0_rad = math::degToRad(initial_state.lat_deg);
    if (std::abs(std::cos(lat0_rad)) < 1e-6)
    {
        throw std::runtime_error("initialState.lat_deg is too close to poles for local tangent-plane conversion");
    }
}

void validateDefinitions(const SessionConfig& config)
{
    if (config.engine_panels.empty())
    {
        throw std::runtime_error("enginePanels must not be empty");
    }
    if (config.engine_orders.empty())
    {
        throw std::runtime_error("engineOrders must not be empty");
    }
    if (config.rudder_presets.empty())
    {
        throw std::runtime_error("rudderPresets must not be empty");
    }

    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> panel_ids;
    for (const auto& panel: config.engine_panels)
    {
        if (!ids.insert(panel.id).second)
        {
            throw std::runtime_error("Duplicate id detected: " + panel.id);
        }
        panel_ids.insert(panel.id);
    }
    for (const auto& order: config.engine_orders)
    {
        if (!ids.insert(order.id).second)
        {
            throw std::runtime_error("Duplicate id detected: " + order.id);
        }
        if (panel_ids.find(order.panel_id) == panel_ids.end())
        {
            throw std::runtime_error("Engine order references unknown panel_id: " + order.panel_id);
        }
    }
    for (const auto& preset: config.rudder_presets)
    {
        if (!ids.insert(preset.id).second)
        {
            throw std::runtime_error("Duplicate id detected: " + preset.id);
        }
    }
}

QDomElement appendChildElement(QDomDocument& doc, QDomElement& parent, const char* tag_name)
{
    QDomElement child = doc.createElement(QString::fromLatin1(tag_name));
    parent.appendChild(child);
    return child;
}

} // namespace

SessionConfig XmlConfigLoader::loadFromFile(const std::string& path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    QDomDocument document;
    QString error_message;
    int error_line = 0;
    int error_column = 0;
    if (!document.setContent(&file, &error_message, &error_line, &error_column))
    {
        throw std::runtime_error("Failed to parse XML config at line " + std::to_string(error_line) + ", column "
                                 + std::to_string(error_column) + ": " + error_message.toStdString());
    }

    const QDomElement root = document.documentElement();
    if (root.isNull() || root.tagName() != QStringLiteral("shipMotionConfig"))
    {
        throw std::runtime_error("Root element must be 'shipMotionConfig'");
    }

    SessionConfig config;

    const QDomElement simulation = requireChild(root, "simulation");
    config.simulation.dt_s = parseRequiredDouble(simulation, "dt_s");
    config.simulation.duration_s = parseRequiredDouble(simulation, "duration_s");
    config.simulation.earth_radius_m = parseRequiredDouble(simulation, "earth_radius_m");

    const QDomElement model = requireChild(root, "model");
    config.simulation.nomoto_T_s = parseRequiredDouble(model, "nomoto_T_s");
    config.simulation.nomoto_K = parseRequiredDouble(model, "nomoto_K");
    config.simulation.rudder_full_effect_speed_mps = parseOptionalDouble(model, "rudder_full_effect_speed_mps", 1.0);
    config.simulation.speed_tau_s = parseRequiredDouble(model, "speed_tau_s");
    config.simulation.rudder_limit_deg = parseRequiredDouble(model, "rudder_limit_deg");

    const QDomElement initial_state = requireChild(root, "initialState");
    config.initial_state.lat_deg = parseRequiredDouble(initial_state, "lat_deg");
    config.initial_state.lon_deg = parseRequiredDouble(initial_state, "lon_deg");
    config.initial_state.heading_deg = parseRequiredDouble(initial_state, "heading_deg");
    config.initial_state.speed_mps = parseRequiredDouble(initial_state, "speed_mps");

    const QDomElement analysis = requireChild(root, "analysis");
    config.default_output_dir = parseRequiredString(analysis, "default_output_dir");
    config.auto_plot = parseOptionalBool(analysis, "auto_plot", true);
    config.plot_script_path = parseRequiredString(analysis, "plot_script");

    const QDomElement engine_panels = requireChild(root, "enginePanels");
    for (QDomElement panel = engine_panels.firstChildElement(QStringLiteral("panel")); !panel.isNull();
         panel = panel.nextSiblingElement(QStringLiteral("panel")))
    {
        config.engine_panels.push_back(EnginePanelDefinition{
          parseRequiredString(panel, "id"),
          parseRequiredString(panel, "label"),
          parseRequiredInt(panel, "display_order"),
        });
    }

    const QDomElement engine_orders = requireChild(root, "engineOrders");
    for (QDomElement order = engine_orders.firstChildElement(QStringLiteral("order")); !order.isNull();
         order = order.nextSiblingElement(QStringLiteral("order")))
    {
        config.engine_orders.push_back(EngineOrderDefinition{
          parseRequiredString(order, "id"),
          parseRequiredString(order, "label"),
          parseRequiredString(order, "panel_id"),
          parseRequiredDouble(order, "target_speed_mps"),
          parseRequiredInt(order, "display_order"),
        });
    }

    const QDomElement rudder_presets = requireChild(root, "rudderPresets");
    for (QDomElement preset = rudder_presets.firstChildElement(QStringLiteral("preset")); !preset.isNull();
         preset = preset.nextSiblingElement(QStringLiteral("preset")))
    {
        config.rudder_presets.push_back(RudderPresetDefinition{
          parseRequiredString(preset, "id"),
          parseRequiredString(preset, "label"),
          parseRequiredDouble(preset, "angle_deg"),
          parseRequiredInt(preset, "display_order"),
        });
    }

    validateSimulationConfig(config.simulation);
    validateInitialState(config.initial_state);
    validateDefinitions(config);
    return config;
}

void XmlConfigWriter::saveToFile(const SessionConfig& config, const std::string& path)
{
    validateSimulationConfig(config.simulation);
    validateInitialState(config.initial_state);
    validateDefinitions(config);

    QDomDocument document(QStringLiteral("shipMotionConfig"));
    QDomProcessingInstruction instruction =
      document.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
    document.appendChild(instruction);

    QDomElement root = document.createElement(QStringLiteral("shipMotionConfig"));
    root.setAttribute(QStringLiteral("version"), QStringLiteral("2"));
    document.appendChild(root);

    QDomElement simulation = appendChildElement(document, root, "simulation");
    simulation.setAttribute(QStringLiteral("dt_s"), config.simulation.dt_s);
    simulation.setAttribute(QStringLiteral("duration_s"), config.simulation.duration_s);
    simulation.setAttribute(QStringLiteral("earth_radius_m"), config.simulation.earth_radius_m);

    QDomElement model = appendChildElement(document, root, "model");
    model.setAttribute(QStringLiteral("nomoto_T_s"), config.simulation.nomoto_T_s);
    model.setAttribute(QStringLiteral("nomoto_K"), config.simulation.nomoto_K);
    model.setAttribute(QStringLiteral("rudder_full_effect_speed_mps"), config.simulation.rudder_full_effect_speed_mps);
    model.setAttribute(QStringLiteral("speed_tau_s"), config.simulation.speed_tau_s);
    model.setAttribute(QStringLiteral("rudder_limit_deg"), config.simulation.rudder_limit_deg);

    QDomElement initial_state = appendChildElement(document, root, "initialState");
    initial_state.setAttribute(QStringLiteral("lat_deg"), config.initial_state.lat_deg);
    initial_state.setAttribute(QStringLiteral("lon_deg"), config.initial_state.lon_deg);
    initial_state.setAttribute(QStringLiteral("heading_deg"), config.initial_state.heading_deg);
    initial_state.setAttribute(QStringLiteral("speed_mps"), config.initial_state.speed_mps);

    QDomElement analysis = appendChildElement(document, root, "analysis");
    analysis.setAttribute(QStringLiteral("default_output_dir"), QString::fromStdString(config.default_output_dir));
    analysis.setAttribute(QStringLiteral("auto_plot"),
                          config.auto_plot ? QStringLiteral("true") : QStringLiteral("false"));
    analysis.setAttribute(QStringLiteral("plot_script"), QString::fromStdString(config.plot_script_path));

    QDomElement engine_panels = appendChildElement(document, root, "enginePanels");
    for (const auto& panel: config.engine_panels)
    {
        QDomElement panel_element = appendChildElement(document, engine_panels, "panel");
        panel_element.setAttribute(QStringLiteral("id"), QString::fromStdString(panel.id));
        panel_element.setAttribute(QStringLiteral("label"), QString::fromStdString(panel.label));
        panel_element.setAttribute(QStringLiteral("display_order"), panel.display_order);
    }

    QDomElement engine_orders = appendChildElement(document, root, "engineOrders");
    for (const auto& order: config.engine_orders)
    {
        QDomElement order_element = appendChildElement(document, engine_orders, "order");
        order_element.setAttribute(QStringLiteral("id"), QString::fromStdString(order.id));
        order_element.setAttribute(QStringLiteral("label"), QString::fromStdString(order.label));
        order_element.setAttribute(QStringLiteral("panel_id"), QString::fromStdString(order.panel_id));
        order_element.setAttribute(QStringLiteral("target_speed_mps"), order.target_speed_mps);
        order_element.setAttribute(QStringLiteral("display_order"), order.display_order);
    }

    QDomElement rudder_presets = appendChildElement(document, root, "rudderPresets");
    for (const auto& preset: config.rudder_presets)
    {
        QDomElement preset_element = appendChildElement(document, rudder_presets, "preset");
        preset_element.setAttribute(QStringLiteral("id"), QString::fromStdString(preset.id));
        preset_element.setAttribute(QStringLiteral("label"), QString::fromStdString(preset.label));
        preset_element.setAttribute(QStringLiteral("angle_deg"), preset.angle_deg);
        preset_element.setAttribute(QStringLiteral("display_order"), preset.display_order);
    }

    const std::filesystem::path output_path(path);
    if (output_path.has_parent_path())
    {
        std::filesystem::create_directories(output_path.parent_path());
    }

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        throw std::runtime_error("Failed to open config file for writing: " + path);
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << document.toString(2);
}

} // namespace ship_sim
