#include <map>
#include <cmath>
#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <nlohmann/json.hpp>
#pragma warning( push )
#pragma warning( disable: 4005 )
#include <podofo/podofo.h>
#pragma warning( pop )
#include <winpp/console.hpp>
#include <winpp/parser.hpp>
#include <winpp/utf8.hpp>
#include <winpp/progress-bar.hpp>
#include "QrCode.h"
#include "openssl-aes.hpp"

using json = nlohmann::ordered_json;

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "bw2qr";
const std::string PROGRAM_VERSION = "3.0.0";

// default length in characters to align status 
constexpr std::size_t g_status_len = 50;

// qrcode properties
struct qr_entry {
  std::string title;
  std::string data;
  std::string url;
};

/*============================================
| Function definitions
==============================================*/
// lambda function to show colored tags
auto add_tag = [](const fmt::color color, const std::string& text) {
  fmt::print(fmt::format(fmt::fg(color) | fmt::emphasis::bold, "[{}]\n", text));
};

// execute a sequence of actions with tags
void exec(const std::string& str, std::function<void()> fct)
{
  fmt::print(fmt::format(fmt::emphasis::bold, "{:<" + std::to_string(g_status_len) + "}", str + ": "));
  try
  {
    fct();
    add_tag(fmt::color::green, "OK");
  }
  catch (const std::exception& ex)
  {
    add_tag(fmt::color::red, "KO");
    throw ex;
  }
}

// create QR Code (called by threads)
void create_qr_code(std::mutex& mutex,
                    const std::string& password,
                    const std::string& iv_b64,
                    std::map<std::string, struct qr::PngImage>& qr_entries_png,
                    std::queue<struct qr_entry>& qr_entries_data,
                    const std::initializer_list<details::OptionsVal>& qr_stylesheet,
                    std::string& qr_failures,
                    console::progress_bar& progress_bar)
{
  while (true)
  {
    // take one element from the queue - protected by mutex
    struct qr_entry entry;
    {
      std::lock_guard<std::mutex> lck(mutex);
      if (qr_entries_data.empty())
        break;
      entry = qr_entries_data.front();
      qr_entries_data.pop();
    }

    // create the QR Code
    try
    {
      // check that the size of the QR Code data
      //  version:  25
      //  size:     117x117
      //  ecc:      quartile
      //  bytes:    715
      const std::size_t qr_max_size = 715;
      const std::size_t max_size = password.empty() ?
        qr_max_size :
        std::floor(std::floor(qr_max_size / 16.0) * 16 * 3 / 4 / 16.0) * 16 - 1;
      if (entry.data.size() > max_size)
        throw std::runtime_error(fmt::format("entry size too big: {} (should be <= {})", entry.data.size(), max_size));

      // force the length of the json string to maximum size
      //  in order to always have QR Code of the same class/size
      entry.data = fmt::format("{:<" + std::to_string(max_size) + "}", entry.data);

      // encrypt data using aes-256-cbc algorithm
      if (!password.empty())
        entry.data = aes::encrypt_256_cbc(entry.data, iv_b64, password);

      // set QR Code properties and stylesheet
      qr::QrCode qrcode({
        option::qrcode_title(entry.title),
        option::qrcode_data(entry.data),
        option::qrcode_url(entry.url),
        option::qrcode_ecc(qr::ecc::quartile)
        });
      qrcode.set(qr_stylesheet);

      // generate the QR Code image
      const struct qr::PngImage& png = qrcode.get();

      // update QR Code images - protected by mutex
      {
        std::lock_guard<std::mutex> lck(mutex);
        qr_entries_png[entry.title] = png;
      }
    }
    catch(const std::exception& ex)
    {
      std::lock_guard<std::mutex> lck(mutex);
      qr_failures += fmt::format("\nfor entry: \"{}\": {}", entry.title, ex.what());
    }

    // update progress-bar - protected by mutex
    {
      std::lock_guard<std::mutex> lck(mutex);
      progress_bar.tick();
    }
  }
}

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // parse command-line arguments
  std::filesystem::path json_file;
  std::filesystem::path pdf_file;
  std::string password;
  std::size_t qrcode_module_px_size     = 3;
  std::size_t qrcode_border_px_size     = 2;
  std::string qrcode_module_color       = "black";
  std::string qrcode_background_color   = "white";
  std::string frame_border_color        = "#054080";
  std::size_t frame_border_width_size   = 12;
  std::size_t frame_border_height_size  = 65;
  std::size_t frame_border_radius       = 15;
  std::size_t frame_logo_size           = 0;
  std::string frame_font_family         = "Arial-Black";
  std::string frame_font_color          = "white";
  double frame_font_size                = 28.0;
  std::size_t pdf_cols                  = 4;
  std::size_t pdf_rows                  = 5;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("j", "json",                     "path to the bitwarden json file",                                                                          json_file, true)
        .add("p", "pdf",                      "path to the pdf output file",                                                                              pdf_file, true)
        .add("z", "password",                 "set a password to encrypt QR Code data using AES-256-CBC algorithm",                                       password)
        .add("m", "qrcode-module-px-size",    fmt::format("{:<45}(default: {})", "size in pixels of each QR Code module",     qrcode_module_px_size),     qrcode_module_px_size)
        .add("o", "qrcode-border-px-size",    fmt::format("{:<45}(default: {})", "size in pixels of the QR Code border",      qrcode_border_px_size),     qrcode_border_px_size)
        .add("q", "qrcode-module-color",      fmt::format("{:<45}(default: {})", "QR Code module color",                      qrcode_module_color),       qrcode_module_color)
        .add("k", "qrcode-background-color",  fmt::format("{:<45}(default: {})", "QR Code background color",                  qrcode_background_color),   qrcode_background_color)
        .add("a", "frame-border-color",       fmt::format("{:<45}(default: {})", "color of the frame",                        frame_border_color),        frame_border_color)
        .add("w", "frame-border-width-size",  fmt::format("{:<45}(default: {})", "size in pixels of the frame border width",  frame_border_width_size),   frame_border_width_size)
        .add("e", "frame-border-height-size", fmt::format("{:<45}(default: {})", "size in pixels of the frame border height", frame_border_height_size),  frame_border_height_size)
        .add("r", "frame-border-radius",      fmt::format("{:<45}(default: {})", "size in pixels of the frame border radius", frame_border_radius),       frame_border_radius)
        .add("l", "frame-logo-size",          fmt::format("{:<45}(default: {})", "size in pixels of the logo",                frame_logo_size),           frame_logo_size)
        .add("f", "frame-font-family",        fmt::format("{:<45}(default: {})", "font family of the QR Code name",           frame_font_family),         frame_font_family)
        .add("c", "frame-font-color",         fmt::format("{:<45}(default: {})", "font color of the QR Code name",            frame_font_color),          frame_font_color)
        .add("s", "frame-font-size",          fmt::format("{:<45}(default: {})", "size in pixels of the QR Code name font",   frame_font_size),           frame_font_size)
        .add("x", "pdf-cols",                 fmt::format("{:<45}(default: {})", "number of columns of QR Codes in pdf",      pdf_cols),                  pdf_cols)
        .add("y", "pdf-rows",                 fmt::format("{:<45}(default: {})", "number of rows of QR Codes in pdf",         pdf_rows),                  pdf_rows);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }

  try
  {
    // check arguments validity
    if (!std::filesystem::exists(json_file) || json_file.extension().string() != ".json")
      throw std::runtime_error(fmt::format("invalid bitwarden json file: \"{}\"", json_file.u8string()));
    if (pdf_file.empty() || pdf_file.extension().string() != ".pdf")
      throw std::runtime_error(fmt::format("invalid output filename: \"{}\"", pdf_file.u8string()));

    // parse bitwarden json file
    std::queue<struct qr_entry> qr_entries_data;
    exec("parse bitwarden json file", [&]() {
      // open json file for read
      std::ifstream file(json_file);
      if (!file.good())
        throw std::runtime_error(fmt::format("can't open file: \"{}\"", json_file.u8string()));

      // lambda to read string data from json object
      auto get_field = [](const json& obj, const std::string& field_name) -> const std::string {
        if (!obj.contains(field_name) ||
            !obj[field_name].is_string() ||
            obj[field_name].empty())
          return {};
        return obj[field_name].get<std::string>();
      };

      // parse json file
      const auto& db = json::parse(file);
      if (!db.contains("items") || !db["items"].is_array())
        throw std::runtime_error("invalid json file format");
      for (const auto& item : db["items"])
      {
        // check item format
        if ((!item.contains("name") || !item["name"].is_string()) ||
            (!item.contains("type") || !item["type"].is_number()) ||
            (!item.contains("favorite") || !item["favorite"].is_boolean()))
          throw std::runtime_error("invalid json file format");

        // skip non login or non-favorite item
        if (item["type"].get<int>() != 1 || !item["favorite"].get<bool>())
          continue;

        // check login format
        if (!item.contains("login") || !item["login"].is_object())
          throw std::runtime_error("invalid json file format");

        // create json object based on entry
        json entry;
        entry["login"] = json::object();
        entry["login"]["username"] = get_field(item["login"], "username");
        entry["login"]["password"] = get_field(item["login"], "password");
        entry["login"]["totp"] = get_field(item["login"], "totp");
        entry["fields"] = json::array();
        if (item.contains("fields"))
        {
          for (const auto& field : item["fields"])
          {
            if (!get_field(field, "name").empty())
            {
              json obj;
              obj[get_field(field, "name")] = get_field(field, "value");
              entry["fields"].push_back(obj);
            }
          }
        }

        // create qrcode entry
        const std::string& title = get_field(item, "name");
        const std::string& data = std::regex_replace(entry.dump(2), std::regex(R"([ ]{4}[{]\s[ ]{6}([^\n]+)\n[ ]{4}[}])"), "    { $1 }");
        const std::string& url = (item["login"].contains("uris") && item["login"]["uris"].size()) ? 
          get_field(item["login"]["uris"].at(0), "uri") : "";

        // add to queue of qrcodes
        qr_entries_data.push({ utf8::to_utf8(title), utf8::to_utf8(data), url });
      }
      });
    if (qr_entries_data.empty())
      throw std::runtime_error("no \"favorite\" entry found");

    // generate a random base64 std::string IV
    std::string iv_b64;
    std::string iv_hex;
    if (!password.empty())
    {
      exec("generate a random IV", [&]() {
        const std::vector<unsigned char>& iv = aes::generate_iv();
        iv_b64 = base64::encode(std::string(iv.begin(), iv.end()));
        for (const auto& byte : iv)
          iv_hex += fmt::format("{:02x}", byte);
        });
    }

    // generate all QR Codes for entries - store png images
    std::map<std::string, struct qr::PngImage> qr_entries_png;
    {
      console::progress_bar progress_bar("generate all entries QR Codes:", qr_entries_data.size());

      // create QR Code stylesheet
      const std::initializer_list<details::OptionsVal> qr_stylesheet = { 
        option::qrcode_module_px_size(qrcode_module_px_size),
        option::qrcode_border_px_size(qrcode_border_px_size),
        option::qrcode_module_color(qrcode_module_color),
        option::qrcode_background_color(qrcode_background_color),
        option::frame_border_color(frame_border_color),
        option::frame_border_width_size(frame_border_width_size),
        option::frame_border_height_size(frame_border_height_size),
        option::frame_border_radius(frame_border_radius),
        option::frame_logo_size(frame_logo_size),
        option::frame_font_family(frame_font_family),
        option::frame_font_color(frame_font_color),
        option::frame_font_size(frame_font_size)
      };

      // start threads
      std::mutex mutex;
      std::string qr_failures;
      const std::size_t max_cpu = static_cast<std::size_t>(std::thread::hardware_concurrency());
      const std::size_t nb_threads = std::min(qr_entries_data.size(), max_cpu);
      std::vector<std::thread> threads(nb_threads);
      for (auto& t : threads)
        t = std::thread(create_qr_code,
                        std::ref(mutex),
                        std::ref(password),
                        std::ref(iv_b64),
                        std::ref(qr_entries_png),
                        std::ref(qr_entries_data),
                        std::ref(qr_stylesheet),
                        std::ref(qr_failures),
                        std::ref(progress_bar));

      // wait for threads completion
      for (auto& t : threads)
        if (t.joinable())
          t.join();

      // check if QR Code convertion has failed
      if (!qr_failures.empty())
        throw std::runtime_error(qr_failures);
    }

    // generate all footers QR Codes - store png images
    std::vector<struct qr::PngImage> qr_footers_png;
    if (!password.empty())
    {
      exec("generate all footers QR Codes", [&]() {
        // create QR Code stylesheet
        const std::initializer_list<details::OptionsVal> qr_stylesheet = {
          option::qrcode_module_px_size(qrcode_module_px_size),
          option::qrcode_module_color(qrcode_module_color),
          option::qrcode_background_color(qrcode_background_color),
          option::frame_border_width_size(10),
          option::frame_border_height_size(35),
          option::frame_border_radius(frame_border_radius),
          option::frame_font_family(frame_font_family),
          option::frame_font_color(frame_font_color),
          option::frame_font_size(20)
        };

        // lambda to create a footer qrcode
        auto create_footer_qrcode = [=](const std::string& name,
                                        const std::string& data,
                                        const std::size_t border_px_size,
                                        const std::string& color) -> struct qr::PngImage {
          qr::QrCode qrcode({
            option::qrcode_title(name),
            option::qrcode_data(data),
            option::qrcode_ecc(qr::ecc::medium),
            option::qrcode_border_px_size(border_px_size),
            option::frame_border_color(color)
          });
          qrcode.set(qr_stylesheet);
          return qrcode.get();
        };

        // add the IV and URL QR Codes to the footers - try to get the same QR Code size by playing with qrcode_border_px_size
        qr_footers_png.push_back(create_footer_qrcode("iv b64",   iv_b64,                                     4, "#7F0000"));
        qr_footers_png.push_back(create_footer_qrcode("decrypt",  "https://cryptii.com/pipes/aes-encryption", 2, "#00137F"));
        qr_footers_png.push_back(create_footer_qrcode("iv hex",   iv_hex,                                     2, "#7F0000"));
        });
    }

    // write all QR Codes to pdf file - resolution: 150dpi
    exec("write all QR Codes to PDF file", [&]() {
      // skip invalid parameters
      if (qr_entries_png.empty())
        throw std::runtime_error("no entry QR Codes to generate");
      if (!pdf_cols || !pdf_rows)
        throw std::runtime_error(fmt::format("invalid number of rows: {} or columns: {}", pdf_rows, pdf_cols));

      // create the pdf document and disable debugging informations
      PoDoFo::PdfError::EnableDebug(false);
      PoDoFo::PdfMemDocument pdf;

      // create the A4 pdf pages
      const std::size_t qr_per_pages = pdf_cols * pdf_rows;
      const std::size_t nb_pages = std::ceil(static_cast<double>(qr_entries_png.size()) / qr_per_pages);
      std::size_t page_width = 0;
      std::size_t page_height = 0;
      for (int i = 0; i < nb_pages; ++i)
      {
        PoDoFo::PdfPage* page = pdf.CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::ePdfPageSize_A4));
        if (!page)
          throw std::runtime_error("can't create pdf page");
        page_width = page->GetPageSize().GetWidth();
        page_height = page->GetPageSize().GetHeight();
      }
      if (!page_width || !page_height)
        throw std::runtime_error(fmt::format("invalid pdf page width: {} or height: {}", page_width, page_height));

      // get the size of entry and footer QR Codes images
      const double scale = 72.0 / 300.0 * 1.30;
      const std::size_t qr_entry_width = qr_entries_png.begin()->second.width * scale;
      const std::size_t qr_entry_height = qr_entries_png.begin()->second.height * scale;
      const std::size_t qr_footer_width = qr_footers_png.empty() ? 0 : qr_footers_png.begin()->width * scale;
      const std::size_t qr_footer_height = qr_footers_png.empty() ? 0 : qr_footers_png.begin()->height * scale;

      // check that everything fits in the pdf page
      if ((qr_entry_width * pdf_cols) > page_width)
        throw std::runtime_error(fmt::format("can't place '{}' QR Codes of {}px width within: {}px of A4 page", pdf_cols, qr_entry_width, page_width));
      if ((qr_entry_height * pdf_rows + qr_footer_height) > page_height)
        throw std::runtime_error(fmt::format("can't place '{}' QR Codes of {}px height + {}px height within: {}px of A4 page", pdf_rows, qr_entry_height, qr_footer_height, page_height));

      // calc margin size
      const std::size_t margin_entry_width = (page_width - (pdf_cols * qr_entry_width)) / (pdf_cols + 1);
      const std::size_t margin_entry_height = (page_height - (pdf_rows * qr_entry_height + qr_footer_height)) / (pdf_rows + 1 + (qr_footers_png.empty() ? 0 : 1));
      const std::size_t margin_footer_width = (page_width - (qr_footers_png.size() * qr_footer_width)) / (qr_footers_png.size() + 1);

      // lambda to draw png images in pdf
      auto draw_png = [](PoDoFo::PdfMemDocument& pdf,
                         PoDoFo::PdfPage* page,
                         const struct qr::PngImage& png,
                         const std::size_t px,
                         const std::size_t py,
                         const double scale) -> void {
        PoDoFo::PdfPainter painter;
        painter.SetPage(page);
        PoDoFo::PdfImage img(&pdf);
        img.LoadFromPngData(reinterpret_cast<const unsigned char*>(png.data.c_str()), png.data.size());
        painter.DrawImage(px, py, &img, scale, scale);
        painter.FinishPage();
      };

      // add all QR Codes png images to A4 pdf pages
      std::size_t qr_idx = 0;
      for (const auto& [k, v] : qr_entries_png)
      {
        // select the proper pdf page
        const std::size_t current_page = qr_idx / qr_per_pages;
        PoDoFo::PdfPage* page = pdf.GetPage(current_page);
        if (!page)
          throw std::runtime_error(fmt::format("can't access pdf page: {}", current_page));
        const std::size_t idx_x = (qr_idx % qr_per_pages) % pdf_cols;
        const std::size_t idx_y = (qr_idx % qr_per_pages) / pdf_cols;
        const double px = ((idx_x + 1) * margin_entry_width) + (idx_x * qr_entry_width);
        const double py = page_height - ((idx_y + 1) * (margin_entry_height + qr_entry_height));
        draw_png(pdf, page, v, px, py, scale);
        ++qr_idx;
      }

      // draw footers QR Codes on the bottom of the pdf
      for (int i = 0; i < nb_pages; ++i)
      {
        // select the proper pdf page
        PoDoFo::PdfPage* page = pdf.GetPage(i);
        if (!page)
          throw std::runtime_error(fmt::format("can't access pdf page: {}", i));
        std::size_t f_idx = 0;
        for (const auto& f : qr_footers_png)
        {
          const double px = ((f_idx + 1) * margin_footer_width) + (f_idx * qr_footer_width);
          const double py = page_height - ((pdf_rows * (qr_entry_height + margin_entry_height)) + qr_footer_height + margin_entry_height);
          draw_png(pdf, page, f, px, py, scale);
          ++f_idx;
        }
      }

      // write pdf to disk
      if (std::filesystem::exists(pdf_file))
      {
        std::ofstream file(pdf_file);
        if (!file.is_open())
          throw std::runtime_error(fmt::format("can't write to file: \"{}\" - already open?", pdf_file.u8string()));
      }
      pdf.Write(pdf_file.string().c_str());
      });

    return 0;
  }
  catch (const std::exception& ex)
  {
    fmt::print("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error:"),
      ex.what());
    return -1;
  }
}