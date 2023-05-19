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
#include <winpp/progress-bar.hpp>
#include "QrCode.h"

using json = nlohmann::ordered_json;

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "bw2qr";
const std::string PROGRAM_VERSION = "2.3.0";

// default length in characters to align status 
constexpr std::size_t g_status_len = 50;

// entry definition with all extracted fields
struct entry {
  std::string name;
  std::string username;
  std::string password;
  std::string totp;
  std::string url;
  std::map<std::string, std::string> fields;
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
                    std::queue<struct entry>& entries,
                    std::map<std::string, struct PngImage>& qr_images,
                    const std::initializer_list<details::OptionsVal>& qr_stylesheet,
                    std::string& qr_failures,
                    console::progress_bar& progress_bar)
{
  while (true)
  {
    // take one element from the queue - protected by mutex
    struct entry entry;
    {
      std::lock_guard<std::mutex> lck(mutex);
      if (entries.empty())
        break;
      entry = entries.front();
      entries.pop();
    }

    // create the QR Code
    try
    {
      // set the QR Code properties
      QrCode qrcode({
        option::qrcode_name(entry.name),
        option::qrcode_username(entry.username),
        option::qrcode_password(entry.password),
        option::qrcode_totp(entry.totp),
        option::qrcode_url(entry.url),
        option::qrcode_fields(entry.fields)
        });
      qrcode.set(qr_stylesheet);

      // generate the QR Code image
      const struct PngImage& png = qrcode.get();

      // update QR Code images - protected by mutex
      {
        std::lock_guard<std::mutex> lck(mutex);
        qr_images[entry.name] = png;
      }
    }
    catch(const std::exception& ex)
    {
      std::lock_guard<std::mutex> lck(mutex);
      qr_failures += fmt::format("\nfor entry: \"{}\": {}", entry.name, ex.what());
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
  std::size_t qrcode_module_px_size     = 3;
  std::size_t qrcode_border_px_size     = 3;
  std::string qrcode_module_color       = "black";
  std::string qrcode_background_color   = "white";
  std::string frame_border_color        = "#054080";
  std::size_t frame_border_width_size   = 12;
  std::size_t frame_border_height_size  = 65;
  std::size_t frame_border_radius       = 15;
  std::size_t frame_logo_size           = 48;
  std::string frame_font_family         = "Arial-Black";
  std::string frame_font_color          = "white";
  double frame_font_size                = 28.0;
  std::size_t pdf_cols                  = 4;
  std::size_t pdf_rows                  = 5;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("j", "json",                     "path to the bitwarden json file",                                                                          json_file, true)
        .add("p", "pdf",                      "path to the pdf output file",                                                                              pdf_file, true)
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
    std::map<std::string, struct entry> qr_entries;
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

        // extract fields from item
        struct entry entry;
        entry.name = get_field(item, "name");
        entry.username = get_field(item["login"], "username");
        entry.password = get_field(item["login"], "password");
        entry.totp = get_field(item["login"], "totp");
        if (item["login"].contains("uris") && item["login"]["uris"].size())
          entry.url = get_field(item["login"]["uris"].at(0), "uri");

        if (item.contains("fields"))
          for (const auto& field : item["fields"])
            if (!get_field(field, "name").empty())
              entry.fields[get_field(field, "name")] = get_field(field, "value");

        // add to map of entries
        qr_entries[entry.name] = entry;
      }
      });
    if (qr_entries.empty())
      throw std::runtime_error("no \"favorite\" entry found");

    // generate all QR Codes for entries - store png images
    std::map<std::string, struct PngImage> qr_images;
    {      
      console::progress_bar progress_bar("generate all QR Codes:", qr_entries.size());

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

      // create queue of entries
      std::queue<struct entry> entries;
      for(const auto& [k, v]: qr_entries)
        entries.push(v);

      // list of QR Code failures
      std::string qr_failures;

      // start threads
      std::mutex mutex;
      const std::size_t max_cpu = static_cast<std::size_t>(std::thread::hardware_concurrency());
      const std::size_t nb_threads = std::min(qr_entries.size(), max_cpu);
      std::vector<std::thread> threads(nb_threads);
      for (auto& t : threads)
        t = std::thread(create_qr_code,
                        std::ref(mutex),
                        std::ref(entries),
                        std::ref(qr_images),
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

    // write QR Codes to pdf file
    exec("write QR Codes to PDF file", [&]() {
      PoDoFo::PdfError::EnableDebug(false);
      PoDoFo::PdfMemDocument pdf;

      // create the A4 pdf pages
      const std::size_t qr_per_pages = pdf_cols * pdf_rows;
      const std::size_t nb_pages = std::ceil(static_cast<double>(qr_images.size()) / qr_per_pages);
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

      // get the size of QR Codes images
      std::size_t img_width = 0;
      std::size_t img_height = 0;
      const double scale = 72.0 / 300.0;
      for (const auto& [k, v] : qr_images)
      {
        img_width = v.width * scale;
        img_height = v.height * scale;
        break;
      }

      // calc margin size
      const std::size_t margin_width = (page_width - (pdf_cols * img_width)) / (pdf_cols + 1);
      const std::size_t margin_height = (page_height - (pdf_rows * img_height)) / (pdf_rows + 1);

      // add QR Codes png images to A4 pdf pages
      std::size_t qr_idx = 0;
      for (const auto& [k, v]: qr_images)
      {
        // calc QR Codes position
        const std::size_t qx = (qr_idx % qr_per_pages) % pdf_cols;
        const std::size_t qy = (qr_idx % qr_per_pages) / pdf_cols;
        const double px = ((qx + 1) * margin_width + (qx * img_width));
        const double py = page_height - ((qy + 1) * (margin_height + img_height));

        // draw QR Codes on pdf
        const std::size_t current_page = qr_idx / qr_per_pages;
        PoDoFo::PdfPage* page = pdf.GetPage(current_page);
        if (!page)
          throw std::runtime_error(fmt::format("can't access pdf page: {}", current_page));
        PoDoFo::PdfImage img(&pdf);
        img.LoadFromPngData(reinterpret_cast<const unsigned char*>(v.data.c_str()), v.data.size());
        PoDoFo::PdfPainter painter;
        painter.SetPage(page);
        painter.DrawImage(px,
                          py,
                          &img,
                          scale,
                          scale);
        painter.FinishPage();
        ++qr_idx;
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