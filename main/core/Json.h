#pragma once

// =============================================================================
//  SiliFlow · core/Json.h
//  对 nlohmann/json 的薄封装：统一 wxString <-> UTF-8 转换，并提供一批
//  「缺字段就给默认值」的取值助手，避免各处重复写 find()/is_number() 判断。
//  纯头文件实现。
// =============================================================================

#include <nlohmann/json.hpp>
#include <wx/colour.h>
#include <wx/string.h>

#include "Types.h"

#include <cstdio>
#include <string>

namespace sf
{

using Json = nlohmann::json;

// -----------------------------------------------------------------------------
//  字符串转换：JSON 内部一律 UTF-8，跨边界只在这两个函数里转。
// -----------------------------------------------------------------------------
inline std::string ToUtf8(const wxString& s) { return std::string(s.ToUTF8()); }
inline wxString FromUtf8(const std::string& s) { return wxString::FromUTF8(s.c_str(), s.size()); }

// -----------------------------------------------------------------------------
//  取值助手：键不存在或类型不符时返回默认值，永不抛异常。
// -----------------------------------------------------------------------------
inline bool Has(const Json& j, const char* key)
{
    return j.is_object() && j.contains(key) && !j[key].is_null();
}

inline int GetInt(const Json& j, const char* key, int def = 0)
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_float()) return static_cast<int>(v.get<double>());
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return def;
}

inline double GetDouble(const Json& j, const char* key, double def = 0.0)
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    if (v.is_number()) return v.get<double>();
    return def;
}

inline bool GetBool(const Json& j, const char* key, bool def = false)
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number_integer()) return v.get<int>() != 0;
    return def;
}

inline wxString GetStr(const Json& j, const char* key, const wxString& def = wxString())
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    if (v.is_string()) return FromUtf8(v.get<std::string>());
    return def;
}

/// 读 {"x":..,"y":..}
inline Point GetPoint(const Json& j, const char* key, const Point& def = Point(0, 0))
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    return Point(GetInt(v, "x", def.x), GetInt(v, "y", def.y));
}

/// 读 {"x":..,"y":..,"w":..,"h":..}
inline Rect GetRect(const Json& j, const char* key, const Rect& def = Rect(0, 0, 0, 0))
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    return Rect(GetInt(v, "x", def.x), GetInt(v, "y", def.y), GetInt(v, "w", def.width),
                GetInt(v, "h", def.height));
}

/// 写 {"x":..,"y":..}
inline void PutPoint(Json& j, const char* key, const Point& p)
{
    j[key] = Json{{"x", p.x}, {"y", p.y}};
}

/// 写 {"x":..,"y":..,"w":..,"h":..}
inline void PutRect(Json& j, const char* key, const Rect& r)
{
    j[key] = Json{{"x", r.x}, {"y", r.y}, {"w", r.width}, {"h", r.height}};
}

// -----------------------------------------------------------------------------
//  颜色：JSON 里用 "#RRGGBB" / "#RRGGBBAA"，也容忍 "RRGGBB"。
// -----------------------------------------------------------------------------
inline wxColour GetColour(const Json& j, const char* key, const wxColour& def = *wxBLACK)
{
    if (!Has(j, key)) return def;
    const Json& v = j[key];
    if (!v.is_string()) return def;

    wxString s = FromUtf8(v.get<std::string>());
    s.Trim(true).Trim(false);
    if (s.StartsWith("#")) s = s.Mid(1);

    if (s.length() != 6 && s.length() != 8) return def;

    long value = 0;
    if (!s.ToLong(&value, 16)) return def;

    const unsigned char a = (s.length() == 8) ? static_cast<unsigned char>(value & 0xFF) : 0xFF;
    const unsigned char r = (s.length() == 8) ? static_cast<unsigned char>((value >> 24) & 0xFF)
                                              : static_cast<unsigned char>((value >> 16) & 0xFF);
    const unsigned char g = (s.length() == 8) ? static_cast<unsigned char>((value >> 16) & 0xFF)
                                              : static_cast<unsigned char>((value >> 8) & 0xFF);
    const unsigned char b = (s.length() == 8) ? static_cast<unsigned char>((value >> 8) & 0xFF)
                                              : static_cast<unsigned char>(value & 0xFF);
    return wxColour(r, g, b, a);
}

inline std::string ColourToHex(const wxColour& c)
{
    char buf[10];
    if (c.Alpha() == 0xFF)
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.Red(), c.Green(), c.Blue());
    else
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.Red(), c.Green(), c.Blue(), c.Alpha());
    return std::string(buf);
}

}  // namespace sf
