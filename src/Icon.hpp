#pragma once
#include <map>
#include <vector>
#include <sstream>
#include <string>
#include <stdint.h>

namespace icon
{
  struct icon_header
  {
    uint16_t reserved = 0;
    uint16_t type = 0;
    uint16_t nb_icons = 0;
  };

  struct icon_properties
  {
    uint8_t width = 0;
    uint8_t height = 0;
    uint8_t nb_colors = 0;
    uint8_t reserved = 0;
    uint16_t color_planes = 0;
    uint16_t bits_per_pixel = 0;
    uint32_t size = 0;
    uint32_t offset = 0;
  };

  // read .ico header
  inline struct icon_header read_header(const std::string& icon_data)
  {
    struct icon_header hdr;
    if (icon_data.size() < sizeof(struct icon_header))
      throw std::runtime_error("invalid icon size");
    std::stringstream ss;
    ss << icon_data;
    ss.read(reinterpret_cast<char*>(&hdr), sizeof(struct icon_header));
    if (!hdr.nb_icons || hdr.type != 1)
      throw std::runtime_error("invalid icon format");
    return hdr;
  }

  // read icon properties
  inline std::vector<struct icon_properties> read_properties(const std::string& icon_data, const uint8_t nb_icons)
  {
    if (icon_data.size() < (sizeof(struct icon_header) + (nb_icons * sizeof(struct icon_properties))))
      throw std::runtime_error("invalid icon size");
    std::stringstream ss;
    ss << icon_data;
    ss.seekg(sizeof(struct icon_header), std::ios::beg);
    std::vector<struct icon_properties> icons;
    for (int i = 0; i < nb_icons; ++i)
    {
      struct icon_properties props;
      ss.read(reinterpret_cast<char*>(&props), sizeof(struct icon_properties));
      icons.push_back(props);
    }
    return icons;
  }

  // get the icon size
  inline uint8_t get_size(const std::string& icon_data)
  {
    const struct icon_header& hdr = read_header(icon_data);
    const std::vector<struct icon_properties>& props = read_properties(icon_data, hdr.nb_icons);
    if (props.size() != 1)
      throw std::runtime_error("invalid number of icon in file");
    return props.begin()->width;
  }

  // get all the icons data
  inline const std::map<std::size_t, std::string> get_icons(const std::string& icons_data)
  {
    // read all icons inside the .ico file
    std::map<std::size_t, std::string> icons;
    const struct icon_header& hdr = read_header(icons_data);
    const std::vector<struct icon_properties>& props = read_properties(icons_data, hdr.nb_icons);
    for (const auto& p : props)
    {
      // read icon data and recreate an icon
      if (icons_data.size() < (p.offset + p.size))
        throw std::runtime_error("invalid icon size");
      std::stringstream icon;
      const std::size_t o_offset = sizeof(struct icon_header) + sizeof(struct icon_properties);
      const struct icon_header o_hdr { 0, 1, 1 };
      const struct icon_properties o_props { p.width, p.height, p.nb_colors, p.reserved, p.color_planes, p.bits_per_pixel, p.size, o_offset };
      icon.write(reinterpret_cast<const char*>(&o_hdr), sizeof(struct icon_header));
      icon.write(reinterpret_cast<const char*>(&o_props), sizeof(struct icon_properties));
      icon.write(&icons_data[p.offset], p.size);
      icons[p.width] = icon.str();
    }
    return icons;
  }
}