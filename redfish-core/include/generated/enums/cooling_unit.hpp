#pragma once
#include <nlohmann/json.hpp>

namespace cooling_unit
{
// clang-format off

enum class CoolingEquipmentType{
    Invalid,
    CDU,
    HeatExchanger,
    ImmersionUnit,
};

enum class CoolingUnitMode{
    Invalid,
    Enabled,
    Disabled,
};

NLOHMANN_JSON_SERIALIZE_ENUM(CoolingEquipmentType, {
    {CoolingEquipmentType::Invalid, "Invalid"},
    {CoolingEquipmentType::CDU, "CDU"},
    {CoolingEquipmentType::HeatExchanger, "HeatExchanger"},
    {CoolingEquipmentType::ImmersionUnit, "ImmersionUnit"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(CoolingUnitMode, {
    {CoolingUnitMode::Invalid, "Invalid"},
    {CoolingUnitMode::Enabled, "Enabled"},
    {CoolingUnitMode::Disabled, "Disabled"},
});

}
// clang-format on
