#include <map>
#include <cmath>
#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <functional>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <nlohmann/json.hpp>
#include <winpp/console.hpp>
#include <winpp/parser.hpp>
#include "QrCode.h"

using json = nlohmann::ordered_json;

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "bw2qr";
const std::string PROGRAM_VERSION = "2.1.0";

// default length in characters to align status 
constexpr std::size_t g_status_len = 50;

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
  std::string frame_border_color        = "#485778";
  std::size_t frame_border_width_size   = 12;
  std::size_t frame_border_height_size  = 65;
  std::size_t frame_border_radius       = 15;
  std::size_t frame_logo_size           = 48;
  std::string frame_font_family         = "Arial-Black";
  std::string frame_font_color          = "white";
  double frame_font_size                = 28.0;
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
        .add("s", "frame-font-size",          fmt::format("{:<45}(default: {})", "size in pixels of the QR Code name font",   frame_font_size),           frame_font_size);
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

    // entry definition with all extracted fields
    struct entry {
      std::string name;
      std::string username;
      std::string password;
      std::string totp;
      std::string url;
      std::map<std::string, std::string> fields;
    };

    // parse bitwarden json file
    std::vector<struct entry> entries;
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

        // convert struct item to json string
        entries.push_back(entry);
      }
      });

    // generate all QR Codes for entries - store png images
    std::vector<std::string> qrcodes;
    exec("generate all qrcodes", [&]() {
      for (const auto& entry : entries)
        qrcodes.push_back(QrCode(
          {
            // set the QR Codes data
            option::qrcode_name(entry.name),
            option::qrcode_username(entry.username),
            option::qrcode_password(entry.password),
            option::qrcode_totp(entry.totp),
            option::qrcode_url(entry.url),
            option::qrcode_fields(entry.fields),

            // set the QR Code stylesheets
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
          }).get());
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