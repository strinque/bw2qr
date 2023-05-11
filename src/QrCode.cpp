#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <qrcodegen.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "QrCode.h"

using json = nlohmann::ordered_json;

// lambda to create indented string
auto indent = [](const int nb, const std::string& str) -> const std::string {
  return std::regex_replace(str, std::regex(R"((^[ ]*[<]))"), std::string(nb, ' ') + "$1");
};

// implementation of the QrCode functionnalities
class QrCodeImpl final
{
public:
  // constructor/destructor
  QrCodeImpl(const struct entry& entry) :
    m_entry(entry)
  {
  }
  ~QrCodeImpl() = default;

  // generate the html qr-code
  const std::string get_html() const
  {
    std::string html;
    html += indent(0, R"(<div class="qrcode">)" "\n");
    html += indent(2, R"(<div class="frame">)" "\n");
    html += indent(4, get_svg());
    html += indent(4, get_img());
    html += indent(4, get_title());
    html += indent(2, R"(</div>)" "\n");
    html += indent(0, R"(</div>)" "\n");
    return html;
  }

private:
  // generate the svg representing the qrcode
  const std::string get_svg() const
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
    std::string json_str = entry.dump(2);
    json_str = std::regex_replace(json_str, std::regex(R"([ ]{4}[{]\s[ ]{6}([^\n]+)\n[ ]{4}[}])"), "    { $1 }");
    if (json_str.size() > 510)
      throw std::runtime_error(fmt::format("can't convert '{}' - entry size too big: {}", m_entry.name, json_str.size()));

    // force the length of the json string: 510
    json_str = fmt::format("{:<510}", json_str);

    // convert to qrcode using qrcodegen library
    const qrcodegen::QrCode& qr = qrcodegen::QrCode::encodeText(json_str.c_str(), qrcodegen::QrCode::Ecc::HIGH);

    // convert qrcode to svg with 2px border
    return to_svg(qr, 2);
  }

  // generate the logo which will be displayed at the middle of qrcode
  const std::string get_img() const
  {
    const std::string& favicon = get_favicon(std::regex_replace(m_entry.url, std::regex(R"(https://)"), ""));
    return fmt::format("{}\n", "<img />");
  }

  // generate the title which will be displayed at the bottom of qrcode
  const std::string get_title() const
  {
    return fmt::format("<h1>{}</h1>\n", m_entry.name.substr(0, 16));
  }

  // convert the qrcode to a svg
  const std::string to_svg(const qrcodegen::QrCode& qr, const int border) const
  {
    std::string svg;
    svg += fmt::format(R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 {} {}" stroke="none">)",
      qr.getSize() + border * 2,
      qr.getSize() + border * 2);
    svg += R"(  <rect width="100%" height="100%" fill="#FFFFFF"/>)";
    svg += R"(  <path d=")";
    for (int y = 0; y < qr.getSize(); y++)
    {
      for (int x = 0; x < qr.getSize(); x++)
      {
        if (!qr.getModule(x, y))
          svg += fmt::format("M{},{}h1v1h-1z", x + border, y + border);
      }
    }
    svg += R"(" fill="#000000"/>)";
    svg += R"(</svg>)";
    svg = std::regex_replace(svg, std::regex(R"([>])"), ">\n");
    return svg;
  }

  // retrieve the favicon
  const std::string get_favicon(const std::string& url) const
  {
    // download the favicon
    std::string favicon;
    {
      httplib::SSLClient client(url);
      auto res = client.Get("/favicon.ico");
      if (!res ||
          (res->status != 200) ||
          !res->body.size())
          return {};
      favicon = res->body;
    }
    // resize the favicon
    // convert favicon to base64
    return {};
  }

private:
  struct entry m_entry;
};

// separate interface from implementation
QrCode::QrCode(const struct entry& entry) : m_pimpl(std::make_unique<QrCodeImpl>(entry)) {}
QrCode::~QrCode() = default;
const std::string QrCode::get_html() const { return m_pimpl ? m_pimpl->get_html() : ""; }