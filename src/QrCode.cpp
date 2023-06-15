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
#include <ZXing/ReadBarcode.h>
#include "QrCode.h"
#include "Icon.hpp"
#include "type_mgk.h"
#include "jbigkit/jbig.h"

using json = nlohmann::ordered_json;

namespace qr
{
  // graphicsmagick library helper class
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

    // convert from std::string to Magick::Color
    static Magick::Color GetColor(const std::string& color)
    {
      try
      {
        return Magick::Color(color);
      }
      catch (const std::exception& ex)
      {
        throw std::runtime_error("can't convert color: \"" + color + "\"");
      }
    }

    // convert the data from Magick::Image to std::string buffer
    static std::string ToString(const Magick::Image& img)
    {
      Magick::Blob blob;
      Magick::Image& i = const_cast<Magick::Image&>(img);
      i.write(&blob);
      std::string str;
      str.assign(static_cast<const char*>(blob.data()), blob.length());
      return str;
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

      // check that mandatory option are set
      if (!m_options.hasArg(details::option_id::qrcode_data))
        throw std::runtime_error("missing mandatory argument: qrcode-data");
      if (!m_options.hasArg(details::option_id::qrcode_ecc))
        throw std::runtime_error("missing mandatory argument: qrcode-ecc");
    }
    ~QrCodeImpl() = default;

    // set a list of options
    void set(const std::initializer_list<details::OptionsVal>& opts)
    {
      m_options.setArgs(opts);
    }

    // generate png image of qrcode in std::string
    const struct PngImage get() const
    {
      // create QR Code using qrcodegen library
      const std::string& qrcode_data = m_options.getArg<std::string>(details::option_id::qrcode_data);
      const qr::ecc& qrcode_ecc = m_options.getArg<qr::ecc>(details::option_id::qrcode_ecc);
      qrcodegen::QrCode::Ecc ecc = qrcodegen::QrCode::Ecc::HIGH;
      switch (qrcode_ecc)
      {
      case qr::ecc::low:      ecc = qrcodegen::QrCode::Ecc::LOW;      break;
      case qr::ecc::medium:   ecc = qrcodegen::QrCode::Ecc::MEDIUM;   break;
      case qr::ecc::quartile: ecc = qrcodegen::QrCode::Ecc::QUARTILE; break;
      case qr::ecc::high:     ecc = qrcodegen::QrCode::Ecc::HIGH;     break;
      default: break;
      }
      const qrcodegen::QrCode& qr = qrcodegen::QrCode::encodeText(qrcode_data.c_str(), ecc);

      // create all png images
      const Magick::Image& qrcode = get_qrcode_png(qr);
      const Magick::Image& logo = get_logo_png();
      const Magick::Image& frame = get_frame_png(qrcode.columns(), qrcode.rows());
      const Magick::Image& text = get_text_png(frame.columns());

      // assemble all png images with logo - try to decode with ZXing
      const std::string& title = m_options.getArg<std::string>(details::option_id::qrcode_title);
      const std::size_t frame_border_width_size = m_options.getArg<std::size_t>(details::option_id::frame_border_width_size);
      const std::size_t frame_border_height_size = title.empty() ? 0 : m_options.getArg<std::size_t>(details::option_id::frame_border_height_size);
      if (logo.isValid())
      {
        Magick::Image with_logo(Magick::Geometry(frame.columns(), frame.rows()), Magick::Color("transparent"));
        with_logo.composite(frame, 0, 0, MagickLib::OverCompositeOp);
        with_logo.composite(qrcode, frame_border_width_size, frame_border_width_size, MagickLib::OverCompositeOp);
        with_logo.composite(logo, frame_border_width_size + (qrcode.rows() - logo.rows()) / 2, frame_border_width_size + (qrcode.rows() - logo.rows()) / 2, MagickLib::OverCompositeOp);
        with_logo.composite(text, (frame.columns() - text.columns()) / 2, frame_border_width_size + qrcode.rows() + (frame_border_height_size - text.rows()) / 2, MagickLib::OverCompositeOp);
        with_logo.magick("PNG");
        if (decode_qr_code(with_logo) == qrcode_data)
          return { with_logo.columns(), with_logo.rows(), GraphicsMagick::ToString(with_logo) };
      }
      // assemble all png images without logo - try to decode with ZXing
      {
        Magick::Image without_logo(Magick::Geometry(frame.columns(), frame.rows()), Magick::Color("transparent"));
        without_logo.composite(frame, 0, 0, MagickLib::OverCompositeOp);
        without_logo.composite(qrcode, frame_border_width_size, frame_border_width_size, MagickLib::OverCompositeOp);
        without_logo.composite(text, (frame.columns() - text.columns()) / 2, frame_border_width_size + qrcode.rows() + (frame_border_height_size - text.rows()) / 2, MagickLib::OverCompositeOp);
        without_logo.magick("PNG");
        if (decode_qr_code(without_logo) == qrcode_data)
          return { without_logo.columns(), without_logo.rows(), GraphicsMagick::ToString(without_logo) };
        else
          throw std::runtime_error("can't decode QR Code image properly using ZXing");
      }
    }

  private:
    // create a png image based on the QR Code
    const Magick::Image get_qrcode_png(const qrcodegen::QrCode& qr) const
    {
      // retrieve parameters
      const std::string& background_color = m_options.getArg<std::string>(details::option_id::qrcode_background_color);
      const std::string& module_color = m_options.getArg<std::string>(details::option_id::qrcode_module_color);
      const std::size_t module_px_size = m_options.getArg<std::size_t>(details::option_id::qrcode_module_px_size);
      const std::size_t border_px_size = m_options.getArg<std::size_t>(details::option_id::qrcode_border_px_size);

      // initialize the image data with a white background - RGB format
      std::vector<uint8_t> img_data(3.0 * qr.getSize() * qr.getSize(), 255);

      // iterate over each module in the QR Code
      for (int y = 0; y < qr.getSize(); ++y)
      {
        for (int x = 0; x < qr.getSize(); ++x) 
        {
          // determine the color of the current module
          const Magick::Color& px_color = qr.getModule(x, y) ? 
            GraphicsMagick::GetColor(module_color) : 
            GraphicsMagick::GetColor(background_color);

          // set the color of the pixels in the image data for the current module
          const int idx       = (y * qr.getSize() + x) * 3;
          img_data[idx]       = px_color.redQuantum();
          img_data[idx + 1.0] = px_color.greenQuantum();
          img_data[idx + 2.0] = px_color.blueQuantum();
        }
      }

      // transform vector in png image of px_per_block size
      Magick::Image png(qr.getSize(), qr.getSize(), "RGB", Magick::CharPixel, img_data.data());
      if (module_px_size != 1)
        png.resize(Magick::Geometry(qr.getSize() * module_px_size, qr.getSize() * module_px_size), MagickLib::FilterTypes::BoxFilter);

      // create QR Code with borders and with border-radius
      const std::size_t img_size = png.rows() + (border_px_size * module_px_size * 2);
      Magick::Image qrcode(Magick::Geometry(img_size, img_size), Magick::Color("transparent"));
      qrcode.antiAlias(true);
      qrcode.draw({ 
        Magick::DrawableFillColor(GraphicsMagick::GetColor(background_color)),
        Magick::DrawableRoundRectangle(0, 0, img_size - 1, img_size - 1, 10, 10)
        });
      qrcode.composite(png, border_px_size * module_px_size, border_px_size * module_px_size, MagickLib::OverCompositeOp);
      qrcode.magick("PNG");
      return qrcode;
    }

    // retrieve the favicon as a png image
    const Magick::Image get_logo_png() const
    {
      try
      {
        // retrieve parameters
        const std::string& url = m_options.getArg<std::string>(details::option_id::qrcode_url);
        const std::size_t logo_size = m_options.getArg<std::size_t>(details::option_id::frame_logo_size);

        // skip empty url logo
        if (!logo_size || url.empty())
          return {};

        // download the favicon - 3s timeout
        std::string favicon_ico;
        {
          const std::regex regex(R"((?:http[s]*://)?([^/]+))");
          std::smatch sm;
          if (!std::regex_search(url, sm, regex))
            return {};
          httplib::SSLClient client(sm.str(1));
          client.set_connection_timeout(std::chrono::seconds(3));
          client.set_read_timeout(std::chrono::seconds(3));
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

        // create a mask of a rounded icon with white background
        Magick::Image mask(Magick::Geometry(logo_size, logo_size), Magick::Color("transparent"));
        mask.antiAlias(true);
        mask.draw({
          Magick::DrawableFillColor(GraphicsMagick::GetColor(Magick::Color("white"))),
          Magick::DrawableRoundRectangle(0, 0, logo_size - 1, logo_size - 1, 10, 10)
          });
        mask.magick("PNG");

        // apply the mask on the icon
        Magick::Image icon_logo(Magick::Geometry(logo_size, logo_size), Magick::Color("transparent"));
        icon_logo.composite(mask, 0, 0, MagickLib::OverCompositeOp);
        icon_logo.composite(icon_image, 0, 0, MagickLib::InCompositeOp);
        icon_logo.magick("PNG");

        // create the logo by assembling the mask with the transparent rounded icon
        Magick::Image logo(Magick::Geometry(logo_size, logo_size), Magick::Color("transparent"));
        logo.composite(mask, 0, 0, MagickLib::OverCompositeOp);
        logo.composite(icon_logo, 0, 0, MagickLib::OverCompositeOp);
        logo.magick("PNG");
        return logo;
      }
      catch (const std::exception& ex)
      {
        return {};
      }
    }

    // create QR Code text
    const Magick::Image get_text_png(const std::size_t width) const
    {
      // retrieve parameters
      const std::string& title = m_options.getArg<std::string>(details::option_id::qrcode_title);
      const std::string& background_color = m_options.getArg<std::string>(details::option_id::frame_border_color);
      const std::string& font_family = m_options.getArg<std::string>(details::option_id::frame_font_family);
      const std::string& font_color = m_options.getArg<std::string>(details::option_id::frame_font_color);
      const double font_size = m_options.getArg<double>(details::option_id::frame_font_size);

      // skip empty text
      if (!font_size || title.empty() || !width)
        return {};

      // determine the maximum text-size within this frame width
      Magick::TypeMetric metrics;
      std::string str = title;
      while(!str.empty())
      {
        Magick::Image text;
        text.font(font_family);
        text.fontPointsize(font_size);
        text.fontTypeMetrics(str, &metrics);
        if (metrics.textWidth() > width)
          str.resize(str.size() - 1);
        else
          break;
      }
      if (str.empty())
        return {};

      // write text in this bounding box
      Magick::Image text(Magick::Geometry(metrics.textWidth(), static_cast<int>(font_size)), Magick::Color("transparent"));
      text.antiAlias(true);
      text.draw({
        Magick::DrawableFont(font_family),
        Magick::DrawableFillColor(GraphicsMagick::GetColor(font_color)),
        Magick::DrawablePointSize(font_size),
        Magick::DrawableTextAntialias(true),
        Magick::DrawableText(0, font_size + metrics.descent() + 2, str.c_str())
        });
      text.magick("PNG");
      return text;
    }

    // create Qr Code Frame
    const Magick::Image get_frame_png(const std::size_t qr_width, const std::size_t qr_height) const
    {
      // retrieve parameters
      const std::string& title = m_options.getArg<std::string>(details::option_id::qrcode_title);
      const std::string& background_color = m_options.getArg<std::string>(details::option_id::qrcode_background_color);
      const std::string& frame_color = m_options.getArg<std::string>(details::option_id::frame_border_color);
      const std::size_t frame_border_width_size = m_options.getArg<std::size_t>(details::option_id::frame_border_width_size);
      const std::size_t frame_border_height_size = title.empty() ? 0 : m_options.getArg<std::size_t>(details::option_id::frame_border_height_size);
      const std::size_t frame_border_radius = m_options.getArg<std::size_t>(details::option_id::frame_border_radius);

      // create the frame
      const std::size_t width = qr_width + frame_border_width_size * 2.0;
      const std::size_t height = qr_height + frame_border_width_size + frame_border_height_size;
      Magick::Image frame(Magick::Geometry(width, height), Magick::Color("transparent"));
      frame.antiAlias(true);
      frame.draw({
        Magick::DrawableFillColor(GraphicsMagick::GetColor(frame_color)),
        Magick::DrawableRoundRectangle(0, 0, width - 1, height - 1, frame_border_radius, frame_border_radius)
        });
      frame.magick("PNG");
      return frame;
    }

    // try to decode the QR Code with ZXing
    const std::string decode_qr_code(const Magick::Image& img) const
    {
      try
      {
        // convert Magick::Image PNG image to RGB std::vector
        const int width = img.columns();
        const int height = img.rows();
        std::vector<unsigned char> data(3.0 * width * height, 0);
        Magick::Image& i = const_cast<Magick::Image&>(img);
        i.write(0, 0, width, height, "RGB", Magick::CharPixel, &data[0]);

        // decode QR Code using ZXing library
        ZXing::DecodeHints hints;
        hints.setTryHarder(true);
        hints.setFormats(ZXing::BarcodeFormat::QRCode);
        ZXing::ImageView image{ data.data(), width, height, ZXing::ImageFormat::RGB };
        ZXing::Result res = ZXing::ReadBarcode(image, hints);
        if (!res.isValid())
          return {};
        return res.text();
      }
      catch (const std::exception& ex)
      {
        return {};
      }
    }

  private:
    details::Options m_options;
  };

  // separate interface from implementation
  QrCode::QrCode(const std::initializer_list<details::OptionsVal>& opts) : m_pimpl(std::make_unique<QrCodeImpl>(opts)) {}
  QrCode::~QrCode() = default;
  void QrCode::set(const std::initializer_list<details::OptionsVal>& opts) { if (m_pimpl) m_pimpl->set(opts); }
  const struct PngImage QrCode::get() const { return m_pimpl ? m_pimpl->get() : struct PngImage(); }
}