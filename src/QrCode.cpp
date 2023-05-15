#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <regex>
#include <filesystem>
#include <stdint.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <qrcodegen.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <Magick++.h>
#include "QrCode.h"
#include "Icon.hpp"
#include "type_mgk.h"
#include "jbigkit/jbig.h"

using json = nlohmann::ordered_json;

// initialize graphicsmagick only once
class GraphicsMagick final
{
public:
  // constructor/destructor
  GraphicsMagick() = default;
  ~GraphicsMagick() = default;

  // initialize graphicsmagick - protected by mutex to avoid multiple initializations
  static void Initialize()
  {
    std::lock_guard<std::mutex> lck(m_mutex);
    if (!m_initialized)
    {
      const std::filesystem::path dir = std::filesystem::temp_directory_path();
      const std::filesystem::path type = dir / "type.mgk";

      auto is_valid = [](const std::filesystem::path& path, const std::string& data) -> bool {
        if (!std::filesystem::exists(path))
          return false;
        std::ifstream file(path, std::ios::binary);
        if (!file)
          return false;
        file.seekg(0, std::ios::end);
        const std::size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string str(size, 0);
        file.read(&str[0], size);
        return str == data;
      };

      if (!is_valid(type, std::string(type_mgk)))
      {
        std::ofstream file(type, std::ios::binary);
        if (!file)
          throw std::runtime_error("can't initialize GraphicsMagick");
        file.write(type_mgk, sizeof(type_mgk) - 1);
      }
      Magick::InitializeMagick(dir.string().c_str());
      m_initialized = true;
    }
  }

private:
  static bool m_initialized;
  static std::mutex m_mutex;
};
bool GraphicsMagick::m_initialized = false;
std::mutex GraphicsMagick::m_mutex;

// implementation of the QR Code functionnalities
class QrCodeImpl final
{
public:
  // constructor/destructor
  QrCodeImpl(const std::initializer_list<details::OptionsVal>& opts) : m_options(opts)
  {
    // initialize graphicsmagick only once
    GraphicsMagick::Initialize();

    // check that all mandatory options are set
    std::vector<details::option_id> missing_ids;
    if (!m_options.hasArgs(
      {
        details::option_id::qrcode_name,
        details::option_id::qrcode_username,
        details::option_id::qrcode_password,
        details::option_id::qrcode_totp,
        details::option_id::qrcode_url,
        details::option_id::qrcode_fields
      },
      missing_ids))
    {
      std::string err = "missing mandatory argument:\n";
      err += "[ ";
      for(const auto& ids: missing_ids)
        err += fmt::format("{}{} ", 
          details::option_name.at(ids), 
          (&ids != &missing_ids.back()) ? "," : "");
      err += "]\n";
      throw std::runtime_error(err);
    }
  }
  ~QrCodeImpl() = default;

  // generate png image of qrcode in std::string
  const std::string get() const
  {
// toremove
    auto save = [](const std::filesystem::path& path, Magick::Image img) -> void {
      std::ofstream file(path, std::ios::binary);
      Magick::Blob blob;
      img.magick("png");
      img.write(&blob);
      file.write(static_cast<const char*>(blob.data()), blob.length());
    };

    // generate the json text for this entry
    //  json string will always have the same size
    const std::string& json = get_json(
      m_options.getArg<std::string>(details::option_id::qrcode_name),
      m_options.getArg<std::string>(details::option_id::qrcode_username),
      m_options.getArg<std::string>(details::option_id::qrcode_password),
      m_options.getArg<std::string>(details::option_id::qrcode_totp),
      m_options.getArg<std::map<std::string, std::string>>(details::option_id::qrcode_fields)
    );

    // create QR Code using qrcodegen library
    //  using HIGH error correction level (ecc >= 30%)
    const qrcodegen::QrCode& qr = qrcodegen::QrCode::encodeText(json.c_str(), qrcodegen::QrCode::Ecc::HIGH);

    // create QR Code png image
    const Magick::Image& qrcode = get_qrcode(
      qr, 
      m_options.getArg<std::string>(details::option_id::qrcode_background_color, "white"),
      m_options.getArg<std::string>(details::option_id::qrcode_module_color, "black"),
      m_options.getArg<uint8_t>(details::option_id::qrcode_module_px_size, 3),
      m_options.getArg<uint8_t>(details::option_id::qrcode_border_px_size, 2));
    save("qrcode.png", qrcode);

    // create QR Code logo image of 48px size
    Magick::Image logo;
    if (m_options.getArg<bool>(details::option_id::frame_logo_status, false))
    {
      logo = get_logo(
        m_options.getArg<std::string>(details::option_id::qrcode_url),
        m_options.getArg<std::string>(details::option_id::qrcode_background_color, "white"),
        m_options.getArg<uint8_t>(details::option_id::frame_logo_size, 48));
      save("logo.png", logo);
    }

    // create QR Code text
    const Magick::Image& text = get_text(
      m_options.getArg<std::string>(details::option_id::qrcode_name),
      m_options.getArg<std::string>(details::option_id::frame_border_color, "grey"),
      m_options.getArg<std::string>(details::option_id::frame_font_family, "Arial-Black"),
      m_options.getArg<std::string>(details::option_id::frame_font_color, "white"),
      m_options.getArg<double>(details::option_id::frame_font_size, 24));
    save("text.png", text);

    // create Qr Code frame
//    const Magick::Image& frame = get_frame(qrcode.size(), frame_color);

    exit(0);
    return {};
  }

private:
  // convert from std::string to Magick::Color
  const Magick::Color get_color(const std::string& str) const
  {
    try
    {
      return Magick::Color(str);
    }
    catch (const std::exception& ex)
    {
      throw std::runtime_error("can't convert color: \"" + str + "\"");
    }
  }

  // generate the json text for this entry
  const std::string get_json(const std::string& name,
                             const std::string& username,
                             const std::string& password,
                             const std::string& totp,
                             const std::map<std::string, std::string>& fields) const
  {
    // create json object based on entry
    json entry;
    entry["login"] = json::object();
    entry["login"]["username"] = username;
    entry["login"]["password"] = password;
    entry["login"]["totp"] = totp;
    entry["fields"] = json::array();
    for (const auto& [k, v] : fields)
    {
      json field = json::object();
      field[k] = v;
      entry["fields"].push_back(field);
    }

    // serialize json to std::string
    std::string json = entry.dump(2);
    json = std::regex_replace(json, std::regex(R"([ ]{4}[{]\s[ ]{6}([^\n]+)\n[ ]{4}[}])"), "    { $1 }");
    if (json.size() > 510)
      throw std::runtime_error(fmt::format("can't convert '{}' - entry size too big: {}", name, json.size()));

    // force the length of the json string: 510
    // in order to always have QR Code of the same class/size
    json = fmt::format("{:<510}", json);
    return json;
  }

  // create a png image based on the QR Code
  const Magick::Image get_qrcode(const qrcodegen::QrCode& qr,
                                 const std::string& module_color,
                                 const std::string& background_color,
                                 const std::size_t module_px_size,
                                 const std::size_t border_px_size) const
  {
    // initialize the image data with a white background - RGB format
    std::vector<uint8_t> img_data(qr.getSize() * qr.getSize() * 3, 255);

    // initialize module colors
    const Magick::Color module_on = get_color(module_color);
    const Magick::Color module_off = get_color(background_color);

    // iterate over each module in the QR Code
    for (int y = 0; y < qr.getSize(); ++y)
    {
      for (int x = 0; x < qr.getSize(); ++x) 
      {
        // determine the color of the current module
        const Magick::Color& px_color = qr.getModule(x, y) ? module_off : module_on;

        // set the color of the pixels in the image data for the current module
        const int idx     = (y * qr.getSize() + x) * 3;
        img_data[idx]     = px_color.redQuantum();
        img_data[idx + 1] = px_color.greenQuantum();
        img_data[idx + 2] = px_color.blueQuantum();
      }
    }

    // transform vector in png image of px_per_block size per module with a border
    const std::size_t img_size = (qr.getSize() + (border_px_size * 2)) * module_px_size;
    Magick::Image qrcode_full(Magick::Geometry(img_size, img_size), module_on);
    Magick::Image qrcode(qr.getSize(), qr.getSize(), "RGB", Magick::CharPixel, img_data.data());
    if (module_px_size != 1)
      qrcode.resize(Magick::Geometry(qr.getSize() * module_px_size, qr.getSize() * module_px_size), MagickLib::FilterTypes::BoxFilter);
    qrcode_full.composite(qrcode, border_px_size * module_px_size, border_px_size * module_px_size);
    qrcode_full.magick("PNG");
    return qrcode_full;
  }

  // retrieve the favicon as a png image
  const Magick::Image get_logo(const std::string& url, 
                               const std::string& background_color,
                               const uint8_t logo_size) const
  {
    try
    {
      // skip empty url logo
      if (url.empty())
        return {};

      // download the favicon
      std::string favicon_ico;
      {
        const std::regex regex(R"((?:http[s]*://)?([^/]+))");
        std::smatch sm;
        if (!std::regex_search(url, sm, regex))
          return {};
        httplib::SSLClient client(sm.str(1));
        auto res = client.Get("/favicon.ico");
        if (!res ||
            (res->status != 200) ||
            !res->body.size())
            return {};
        favicon_ico = res->body;
      }

      // extract biggest icon from favicon
      const std::map<std::size_t, std::string>& icons = icon::get_icons(favicon_ico);
      if (icons.empty())
        return {};
      std::string big_icon;
      if (icons.find(logo_size) != icons.end())
        big_icon = icons.find(logo_size)->second;
      else
        big_icon = icons.rbegin()->second;

      // load the favicon data and resize it
      Magick::Blob blob_in(big_icon.c_str(), big_icon.size());
      Magick::Image icon_image;
      icon_image.magick("ICO");
      icon_image.read(blob_in);
      if (icon::get_size(big_icon) != logo_size)
        icon_image.resize(Magick::Geometry(logo_size, logo_size), Magick::FilterTypes::LanczosFilter);

      // create logo
      Magick::Image logo(Magick::Geometry(logo_size, logo_size), Magick::Color("transparent"));
      logo.matte(true);
      logo.fillColor(get_color(background_color));
      logo.draw(Magick::DrawableRoundRectangle(0, 0, logo_size-1, logo_size-1, 10, 10));
      logo.composite(icon_image, 0, 0, Magick::OverCompositeOp);
      logo.magick("PNG");
      return logo;
    }
    catch (const std::exception& ex)
    {
      return {};
    }
  }

  // create QR Code text
  const Magick::Image get_text(const std::string& title,
                               const std::string& background_color,
                               const std::string& font_family,
                               const std::string& font_color,
                               const double font_size) const
  {
    // set max size of title to 16 chars
    const std::string& str = title.substr(0, 16);

    // determine the text size
    Magick::Geometry bounding_box;
    {
      Magick::Image text;
      text.font(font_family);
      text.fontPointsize(font_size);
      Magick::TypeMetric metrics;
      text.fontTypeMetrics(str.c_str(), &metrics);
      bounding_box.width(metrics.textWidth());
      bounding_box.height((metrics.textHeight() / 2) + 2);
    }

    // write text in this bouding box
    Magick::Image text(bounding_box, get_color(background_color));
    Magick::DrawableList props = {
      Magick::DrawableFont(font_family),
      Magick::DrawableTextUnderColor(get_color(background_color)),
      Magick::DrawableFillColor(get_color(font_color)),
      Magick::DrawablePointSize(font_size),
      Magick::DrawableTextAntialias(true),
      Magick::DrawableText(0, bounding_box.height() - 1, str.c_str())
    };
    text.draw(props);
    text.magick("PNG");
    return text;
  }

private:
  details::Options m_options;
};

// separate interface from implementation
QrCode::QrCode(const std::initializer_list<details::OptionsVal>& opts) : m_pimpl(std::make_unique<QrCodeImpl>(opts)) {}
QrCode::~QrCode() = default;
const std::string QrCode::get() const { return m_pimpl ? m_pimpl->get() : ""; }