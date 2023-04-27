#include <string>
#include <filesystem>
#include <functional>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <winpp/console.hpp>
#include <winpp/parser.hpp>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "bw2qr";
const std::string PROGRAM_VERSION = "1.0.0";

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
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }

  try
  {
    exec("execute something", [&]() {
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