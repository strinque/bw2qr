#include <string>
#include <filesystem>
#include <functional>
#include <signal.h>
#include <stdbool.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <winpp/console.hpp>
#include <winpp/parser.hpp>

/*============================================
| Declaration
==============================================*/
// program version
const std::string PROGRAM_NAME = "xxx";
const std::string PROGRAM_VERSION = "1.0.0";

// default length in characters to align status 
constexpr std::size_t g_status_len = 50;

/*============================================
| Function definitions
==============================================*/
// define the function to be called when ctrl-c is sent to process
void exit_program(int signum)
{
  fmt::print("\nevent: ctrl-c called => stopping program\n");
}

// lambda function to show colored tags
auto add_tag = [](const fmt::color color, const std::string& text) {
  spdlog::debug(fmt::format(fmt::fg(color) | fmt::emphasis::bold, "[{}]\n", text));
};

// execute a sequence of actions with tags
void exec(const std::string& str, std::function<void()> fct)
{
  spdlog::debug(fmt::format(fmt::emphasis::bold, "{:<" + std::to_string(g_status_len) + "}", str + ": "));
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

// initialize logger - spdlog
bool init_logger(const std::filesystem::path& file)
{
  try
  {
    // initialize console output
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("%v");

    std::shared_ptr<spdlog::logger> logger;
    if (!file.empty())
    {
      // initialize file output
      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file.string(), true);
      file_sink->set_level(spdlog::level::info);
      file_sink->set_pattern("%v");
      logger = std::make_shared<spdlog::logger>(spdlog::logger("multi_sink", { console_sink, file_sink }));
    }
    else
      logger = std::make_shared<spdlog::logger>(spdlog::logger("multi_sink", { console_sink }));

    // set default logger
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    // disable end-of-line
    auto custom_formatter = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, std::string(""));
    logger->set_formatter(std::move(custom_formatter));

    // activate flush on level: info
    logger->flush_on(spdlog::level::info);

    return true;
  }
  catch (...)
  {
    return false;
  }
}

int main(int argc, char** argv)
{
  // initialize Windows console
  console::init();

  // register signal handler
  signal(SIGINT, exit_program);

  // parse command-line arguments
  std::filesystem::path log_file;
  console::parser parser(PROGRAM_NAME, PROGRAM_VERSION);
  parser.add("l", "log", "save the updated list of directories to a log file", log_file);
  if (!parser.parse(argc, argv))
  {
    parser.print_usage();
    return -1;
  }

  // initialize logger
  if (!init_logger(log_file))
  {
    fmt::print("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error:"),
      fmt::format("can't create the log file: \"{}\"", log_file.u8string()));
    return -1;
  }

  try
  {
    exec("execute something", [&]() {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    });
    return 0;
  }
  catch (const std::exception& ex)
  {
    spdlog::debug("{} {}\n",
      fmt::format(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error:"),
      ex.what());
    return -1;
  }
}