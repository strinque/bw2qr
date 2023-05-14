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
      if (!std::filesystem::exists(type))
      {
        std::ofstream file(type, std::ios::binary);
        if (!file)
          throw std::runtime_error("can't initialize GraphicsMagick");
        file.write(type_mgk, sizeof(type_mgk));
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

// lambda to create indented string
auto indent = [](const int nb, const std::string& str) -> const std::string {
  return std::regex_replace(str, std::regex(R"((^[ ]*[<]))"), std::string(nb, ' ') + "$1");
};

// implementation of the QR Code functionnalities
class QrCodeImpl final
{
public:
  // constructor/destructor
  QrCodeImpl(const struct entry& entry) : m_entry(entry)
  {
    // initialize graphicsmagick only once
    GraphicsMagick::Initialize();
  }
  ~QrCodeImpl() = default;

  // generate png image of qrcode in std::string
  const std::string get(const std::string& frame_color = "#000000",
                        const std::string& qrcode_color = "#000000",
                        const uint8_t px_per_block = 3, 
                        const uint8_t px_border = 2,
                        const bool with_logo = true) const
  {
    auto save = [](const std::filesystem::path& path, Magick::Image img) -> void {
      std::ofstream file(path, std::ios::binary);
      Magick::Blob blob;
      img.magick("png");
      img.write(&blob);
      file.write(static_cast<const char*>(blob.data()), blob.length());
    };


    // generate the json text for this entry
    //  json string will always have the same size
    const std::string& json = get_json();

    // create QR Code using qrcodegen library
    //  using HIGH error correction level (ecc >= 30%)
    const qrcodegen::QrCode& qr = qrcodegen::QrCode::encodeText(json.c_str(), qrcodegen::QrCode::Ecc::HIGH);

    // convert hex color to Magick::Color
    auto get_color = [](const std::string& color) -> const Magick::Color {
      std::regex regex(R"([#]([0-9]{2})([0-9]{2})([0-9]{2}))");
      std::smatch sm;
      if (!std::regex_match(color, sm, regex))
        throw std::runtime_error("can't decode hexadecimal color");
      return Magick::Color(std::stoi(sm.str(1), nullptr, 16),
                           std::stoi(sm.str(2), nullptr, 16),
                           std::stoi(sm.str(3), nullptr, 16));
    };

    // create QR Code png image
    const Magick::Image& qrcode = get_qrcode(qr, get_color(qrcode_color), px_per_block, px_border);
    save("qrcode.png", qrcode);

    // create QR Code logo image of 48px size
    Magick::Image logo;
    if (with_logo)
      logo = get_logo(m_entry.url, 48);
    save("logo.png", logo);

    // create QR Code text
    const Magick::Image& text = get_text(m_entry.name.substr(0, 16), "Arial-Black", Magick::Color("white"), 24);
    save("text.png", text);

    // create Qr Code frame
//    const Magick::Image& frame = get_frame(qrcode.size(), frame_color);

    exit(0);
    return {};
  }

private:
  // generate the json text for this entry
  const std::string get_json() const
  {
    // create json object based on entry
    json entry;
    entry["login"] = json::object();
    entry["login"]["username"] = m_entry.username;
    entry["login"]["password"] = m_entry.password;
    entry["login"]["totp"] = m_entry.totp;
    entry["fields"] = json::array();
    for (const auto& [k, v] : m_entry.fields)
    {
      json field = json::object();
      field[k] = v;
      entry["fields"].push_back(field);
    }

    // serialize json to std::string
    std::string json = entry.dump(2);
    json = std::regex_replace(json, std::regex(R"([ ]{4}[{]\s[ ]{6}([^\n]+)\n[ ]{4}[}])"), "    { $1 }");
    if (json.size() > 510)
      throw std::runtime_error(fmt::format("can't convert '{}' - entry size too big: {}", m_entry.name, json.size()));

    // force the length of the json string: 510
    // in order to always have QR Code of the same class/size
    json = fmt::format("{:<510}", json);
    return json;
  }

  // create a png image based on the QR Code
  const Magick::Image get_qrcode(const qrcodegen::QrCode& qr,
                                 const Magick::Color& color,
                                 const std::size_t px_per_block,
                                 const std::size_t px_border) const
  {
    // initialize the image data with a white background - RGB format
    std::vector<uint8_t> img_data(qr.getSize() * qr.getSize() * 3, 255);

    // iterate over each module in the QR Code
    for (int y = 0; y < qr.getSize(); ++y)
    {
      for (int x = 0; x < qr.getSize(); ++x) 
      {
        // determine the color of the current module (black or white)
        const Magick::Color& px_color = qr.getModule(x, y) ? Magick::Color("white") : color;

        // set the color of the pixels in the image data for the current module
        const int idx     = (y * qr.getSize() + x) * 3;
        img_data[idx]     = px_color.redQuantum();
        img_data[idx + 1] = px_color.greenQuantum();
        img_data[idx + 2] = px_color.blueQuantum();
      }
    }

    // transform vector in png image of px_per_block size per module with a border
    const std::size_t img_size = (qr.getSize() + (px_border * 2)) * px_per_block;
    Magick::Image qrcode_full(Magick::Geometry(img_size, img_size), Magick::Color("white"));
    Magick::Image qrcode(qr.getSize(), qr.getSize(), "RGB", Magick::CharPixel, img_data.data());
    if (px_per_block != 1)
      qrcode.resize(Magick::Geometry(qr.getSize() * px_per_block, qr.getSize() * px_per_block), MagickLib::FilterTypes::BoxFilter);
    qrcode_full.composite(qrcode, px_border * px_per_block, px_border * px_per_block);
    qrcode_full.magick("PNG");
    return qrcode_full;
  }

  // retrieve the favicon as a png image
  const Magick::Image get_logo(const std::string& url, const uint8_t logo_size) const
  {
    try
    {
      // download the favicon
      std::string favicon_ico;
      {
        const std::string& website = std::regex_replace(url, std::regex(R"(https://)"), "");
        httplib::SSLClient client(website);
        auto res = client.Get("/favicon.ico");
        if (!res ||
            (res->status != 200) ||
            !res->body.size())
            return {};
        favicon_ico = res->body;
      }

      // extract biggest icon from favicon
      const auto& icons = icon::get_icons(favicon_ico);
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
      logo.fillColor(Magick::Color("white"));
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
                               const std::string& font,
                               const Magick::Color& font_color,
                               const double font_size) const
  {
    // write text in box
    Magick::Image text(Magick::Geometry(300, 100), Magick::Color("grey"));
    Magick::DrawableList props = {
      Magick::DrawableFont(font),
      Magick::DrawableTextUnderColor("grey"),
      Magick::DrawableFillColor(font_color),
      Magick::DrawablePointSize(font_size),
      Magick::DrawableTextAntialias(true),
      Magick::DrawableText(300/2, 100/2, title.c_str())
    };
    text.draw(props);
    text.magick("PNG");
    return text;
  }

private:
  struct entry m_entry;
};

// separate interface from implementation
QrCode::QrCode(const struct entry& entry) : m_pimpl(std::make_unique<QrCodeImpl>(entry)) {}
QrCode::~QrCode() = default;
const std::string QrCode::get() const { return m_pimpl ? m_pimpl->get() : ""; }