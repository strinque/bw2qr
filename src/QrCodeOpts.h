#pragma once
#include <map>
#include <algorithm>
#include <string>
#include <variant>
#include <initializer_list>
#include <stdint.h>
#include <stdbool.h>

namespace qr
{
  enum class ecc
  {
    low,      // 7%
    medium,   // 15%
    quartile, // 25%
    high      // 30%
  };
}

namespace details
{
  // list of option id
  enum class option_id
  {
    qrcode_title,
    qrcode_data,
    qrcode_url,
    qrcode_ecc,
    qrcode_module_px_size,
    qrcode_border_px_size,
    qrcode_module_color,
    qrcode_background_color,
    frame_border_color,
    frame_border_width_size,
    frame_border_height_size,
    frame_border_radius,
    frame_logo_size,
    frame_font_family,
    frame_font_color,
    frame_font_size
  };
  const std::map<option_id, std::string> option_name = 
  {
    {option_id::qrcode_title,             "qrcode-title"},
    {option_id::qrcode_data,              "qrcode-data"},
    {option_id::qrcode_url,               "qrcode-url"},
    {option_id::qrcode_ecc,               "qrcode-ecc"},
    {option_id::qrcode_module_px_size,    "qrcode-module-px-size"},
    {option_id::qrcode_border_px_size,    "qrcode-border-px-size"},
    {option_id::qrcode_module_color,      "qrcode-module-color"},
    {option_id::qrcode_background_color,  "qrcode-background-color"},
    {option_id::frame_border_color,       "frame-border-color"},
    {option_id::frame_border_width_size,  "frame-border-width-size"},
    {option_id::frame_border_height_size, "frame-border-height-size"},
    {option_id::frame_border_radius,      "frame-border-radius"},
    {option_id::frame_logo_size,          "frame-logo-size"},
    {option_id::frame_font_family,        "frame-font-family"},
    {option_id::frame_font_color,         "frame-font-color"},
    {option_id::frame_font_size,          "frame-font-size"}
  };

  // template used to check the data type validity
  template<option_id ID, typename T1>
  struct option_data
  {
    template<typename T2>
    option_data(T2 a) :
      id(ID),
      arg(a)
    {
      static_assert(std::is_convertible_v<T1, T2>, "invalid data type for this option");
    }
    template<>
    option_data(const char* a) :
      id(ID),
      arg(a)
    {
      static_assert(std::is_convertible_v<T1, std::string>, "invalid data type for this option");
    }
    option_id id;
    T1 arg;
  };
}

namespace option
{
  // define options and their data type (to check format type afterwards)
  using qrcode_title              = details::option_data<details::option_id::qrcode_title,              std::string>;
  using qrcode_data               = details::option_data<details::option_id::qrcode_data,               std::string>;
  using qrcode_url                = details::option_data<details::option_id::qrcode_url,                std::string>;
  using qrcode_ecc                = details::option_data<details::option_id::qrcode_ecc,                qr::ecc>;
  using qrcode_module_px_size     = details::option_data<details::option_id::qrcode_module_px_size,     std::size_t>;
  using qrcode_border_px_size     = details::option_data<details::option_id::qrcode_border_px_size,     std::size_t>;
  using qrcode_module_color       = details::option_data<details::option_id::qrcode_module_color,       std::string>;
  using qrcode_background_color   = details::option_data<details::option_id::qrcode_background_color,   std::string>;
  using frame_border_color        = details::option_data<details::option_id::frame_border_color,        std::string>;
  using frame_border_width_size   = details::option_data<details::option_id::frame_border_width_size,   std::size_t>;
  using frame_border_height_size  = details::option_data<details::option_id::frame_border_height_size,  std::size_t>;
  using frame_border_radius       = details::option_data<details::option_id::frame_border_radius,       std::size_t>;
  using frame_logo_size           = details::option_data<details::option_id::frame_logo_size,           std::size_t>;
  using frame_font_family         = details::option_data<details::option_id::frame_font_family,         std::string>;
  using frame_font_color          = details::option_data<details::option_id::frame_font_color,          std::string>;
  using frame_font_size           = details::option_data<details::option_id::frame_font_size,           double>;
}

namespace details
{
  // variant which contains all the different options available
  using OptionsVal = 
    std::variant<
      option::qrcode_title,
      option::qrcode_data,
      option::qrcode_url,
      option::qrcode_ecc,
      option::qrcode_module_px_size,
      option::qrcode_border_px_size,
      option::qrcode_module_color,
      option::qrcode_background_color,
      option::frame_border_color,
      option::frame_border_width_size,
      option::frame_border_height_size,
      option::frame_border_radius,
      option::frame_logo_size,
      option::frame_font_family,
      option::frame_font_color,
      option::frame_font_size
    >;

  // variant which contains all the different options data types
  using OptionsType = std::variant<std::string, std::size_t, double, qr::ecc>;

  // store all the different options
  class Options final
  {
  public:
    Options(const std::initializer_list<OptionsVal>& opts) :
      m_opts()
    {
      setArgs(opts);
    }
    ~Options() = default;

    // set a list of options
    void setArgs(const std::initializer_list<OptionsVal>& opts)
    {
      for (const auto& o : opts)
      {
        if      (std::holds_alternative<option::qrcode_title>(o))             setArg(option_id::qrcode_title,             std::get<option::qrcode_title>(o).arg);
        else if (std::holds_alternative<option::qrcode_data>(o))              setArg(option_id::qrcode_data,              std::get<option::qrcode_data>(o).arg);
        else if (std::holds_alternative<option::qrcode_url>(o))               setArg(option_id::qrcode_url,               std::get<option::qrcode_url>(o).arg);
        else if (std::holds_alternative<option::qrcode_ecc>(o))               setArg(option_id::qrcode_ecc,               std::get<option::qrcode_ecc>(o).arg);
        else if (std::holds_alternative<option::qrcode_module_px_size>(o))    setArg(option_id::qrcode_module_px_size,    std::get<option::qrcode_module_px_size>(o).arg);
        else if (std::holds_alternative<option::qrcode_border_px_size>(o))    setArg(option_id::qrcode_border_px_size,    std::get<option::qrcode_border_px_size>(o).arg);
        else if (std::holds_alternative<option::qrcode_module_color>(o))      setArg(option_id::qrcode_module_color,      std::get<option::qrcode_module_color>(o).arg);
        else if (std::holds_alternative<option::qrcode_background_color>(o))  setArg(option_id::qrcode_background_color,  std::get<option::qrcode_background_color>(o).arg);
        else if (std::holds_alternative<option::frame_border_color>(o))       setArg(option_id::frame_border_color,       std::get<option::frame_border_color>(o).arg);
        else if (std::holds_alternative<option::frame_border_width_size>(o))  setArg(option_id::frame_border_width_size,  std::get<option::frame_border_width_size>(o).arg);
        else if (std::holds_alternative<option::frame_border_height_size>(o)) setArg(option_id::frame_border_height_size, std::get<option::frame_border_height_size>(o).arg);
        else if (std::holds_alternative<option::frame_border_radius>(o))      setArg(option_id::frame_border_radius,      std::get<option::frame_border_radius>(o).arg);
        else if (std::holds_alternative<option::frame_logo_size>(o))          setArg(option_id::frame_logo_size,          std::get<option::frame_logo_size>(o).arg);
        else if (std::holds_alternative<option::frame_font_family>(o))        setArg(option_id::frame_font_family,        std::get<option::frame_font_family>(o).arg);
        else if (std::holds_alternative<option::frame_font_color>(o))         setArg(option_id::frame_font_color,         std::get<option::frame_font_color>(o).arg);
        else if (std::holds_alternative<option::frame_font_size>(o))          setArg(option_id::frame_font_size,          std::get<option::frame_font_size>(o).arg);
        else throw std::runtime_error("invalid option given");
      }
    }

    // get the option value from the map
    template<typename T>
    const T getArg(const option_id id, const T& default_value = {}) const
    {
      const auto it = m_opts.find(id);
      if (it == m_opts.end() || !std::holds_alternative<T>(it->second))
        return default_value;
      return std::get<T>(it->second);
    }

    // check if the option has been set
    bool hasArg(const option_id id) const { return (m_opts.find(id) != m_opts.end()); }

    // check if the options have been set
    bool hasArgs(const std::vector<option_id>& ids,
                 std::vector<option_id>& missing_ids = std::vector<option_id>()) const
    {
      std::for_each(ids.begin(), ids.end(), [&](const option_id id) {
        if (!hasArg(id))
          missing_ids.push_back(id);
        });
      return missing_ids.empty();
    }

  private:
    // set the option value in the map
    void setArg(const option_id id, OptionsType value) { m_opts[id] = value; }

  private:
    std::map<option_id, OptionsType> m_opts;
  };
}