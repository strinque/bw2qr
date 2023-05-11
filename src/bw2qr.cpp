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
#include "html_template.h"
#include "QrCode.h"

using json = nlohmann::ordered_json;

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "bw2qr";
const std::string PROGRAM_VERSION = "1.1.0";

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

// lambda to create indented string
auto indent = [](const int nb, const std::string& str) -> const std::string {
  return std::regex_replace(str, std::regex(R"((^[ ]*[<]))"), std::string(nb, ' ') + "$1");
};

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // parse command-line arguments
  std::filesystem::path json_file;
  std::filesystem::path pdf_file;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("j", "json", "path to the bitwarden json file", json_file, true)
        .add("p", "pdf", "path to the pdf output file", pdf_file, true);
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

    // generate all qrcodes for entries
    std::vector<std::string> qrcodes;
    exec("generate all qrcodes", [&]() {
      for (const auto& entry : entries)
        qrcodes.push_back(QrCode(entry).get_html());
      });

    // generate html file
    exec("generate html file", [&]() {
      // load html template
      std::string html = html_template;

      // construct all html A4 pages - 12 qrcodes per page
      std::string data;
      constexpr int qr_per_page = 12;
      const int nb_pages = static_cast<int>(std::ceil(static_cast<float>(qrcodes.size()) / qr_per_page));
      if (!nb_pages)
        throw std::runtime_error("no qrcode marked as 'favorite' as been found");
      data += indent(4, R"(<div class="document">)" "\n");
      for (int p = 0; p < nb_pages; ++p)
      {
        data += indent(6, R"(<div class="page">)" "\n");
        for (int q = 0; q < qr_per_page; q++)
        {
          const int qr_idx = p * qr_per_page + q;
          if (qr_idx < qrcodes.size())
            data += indent(8, qrcodes.at(qr_idx));
          else
            data += indent(8, R"(<div class="qrcode" />)" "\n");
        }
        data += indent(6, R"(</div>)" "\n");
      }
      data += indent(4, R"(</div>)" "\n");

      // inject pages into html
      html = std::regex_replace(html, std::regex(R"(([ ]+<div class="document" />))"), data);

      // write to file
      std::filesystem::path html_file = pdf_file;
      html_file.replace_extension(".html");
      std::ofstream file(html_file, std::ios::binary);
      file << html;
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