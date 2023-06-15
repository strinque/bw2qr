#pragma once
#include <string>
#include <regex>
#include <chrono>
#include <map>
#include <stdbool.h>
#include <httplib.h>

namespace favicon
{
  // handle url component
  struct url
  {
    bool is_valid() const { return !base.empty(); }
    bool decode(const std::string& url)
    {
      std::regex regex(R"(^(http[s]*:\/\/)?(www.)?([^:\/]+)[:]*([0-9]+)?([\/]*.*))");
      std::smatch sm;
      if (!std::regex_search(url, sm, regex))
        return false;
      try
      {
        const std::string& b = sm.str(3);
        const std::string& r = sm.str(5).empty() ? "/" : sm.str(5);
        const std::size_t p = sm.str(4).empty() ? 443u : std::stoi(sm.str(4).c_str());
        base = b;
        request = std::regex_replace(r, std::regex(R"([&]amp;)"), "&");
        ssl_port = p;
        return true;
      }
      catch (...)
      {
        return false;
      }
    }
    std::string base;
    std::string request;
    std::size_t ssl_port = 443u;
  };

  // try to download data directly or with redirection if necessary
  inline std::string download_data(const struct url& url,
                                   const std::chrono::seconds& timeout = std::chrono::seconds(5))
  {
    // check url validity
    if (!url.is_valid())
      return {};

    // try to download data directly
    std::string html;
    std::size_t status;
    {
      httplib::SSLClient client(url.base, url.ssl_port);
      client.set_connection_timeout(timeout);
      client.set_read_timeout(timeout);
      auto res = client.Get(url.request);
      if (!res ||
          !res->body.size() ||
          ((res->status != 200) && (res->status != 301) && (res->status != 302)))
        return {};
      html = res->body;
      status = res->status;
    }

    // try to download the redirected data
    if ((status == 301) || (status == 302))
    {
      // retrieve the redirected link
      const std::regex regex(R"(<[aA] [hH][rR][eE][fF]=["]([^"]+)["].*>)");
      std::smatch sm;
      if (!std::regex_search(html, sm, regex))
        return {};
      struct url redirected_url;
      if (!redirected_url.decode(sm.str(1)) || !redirected_url.is_valid())
        return {};

      httplib::SSLClient client(redirected_url.base, redirected_url.ssl_port);
      client.set_connection_timeout(timeout);
      client.set_read_timeout(timeout);
      auto res = client.Get(redirected_url.request);
      if (!res ||
          !res->body.size() ||
          (res->status != 200))
        return {};
      html = res->body;
    }
    return html;
  }

  // try to download icon by decoding html
  inline bool download_with_generic_api(const std::string& url,
                                        const std::size_t logo_size,
                                        std::string& icon_content)
  {
    try
    {
      // decode url properties
      struct url url_props;
      if (!url_props.decode(url) || !url_props.is_valid())
        return false;

      // download webpage using generic api
      const std::string& page = download_data(url_props);
      if (page.empty())
        return false;

      // find all the icons url in html head
      std::map<std::size_t, std::string> icons;
      {
        const std::regex regex(R"(<link rel=["][^"]+["] (type=["][^"]+["] )?(sizes=["]([0-9]+)x[0-9]+["] )?href=["]([^"]+[.]png)["])");
        for (auto sit = std::sregex_iterator(page.cbegin(), page.cend(), regex); sit != std::sregex_iterator(); ++sit)
        {
          if(sit->str(3).empty())
            icons[0u] = sit->str(4);
          else
            icons[std::stoi(sit->str(3).c_str())] = sit->str(4);
        }
      }
      if (icons.empty())
        return false;

      // detect best icon url
      if (icons.find(logo_size) != icons.end())
        url_props.request = "/" + icons.find(logo_size)->second;
      else
        url_props.request = "/" + icons.rbegin()->second;

      // download favicon
      const std::string& data = download_data(url_props);
      if (data.empty())
        return false;
      icon_content = data;
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  // try to download icon using google-api
  inline bool download_with_google_api(const std::string& url,
                                       const std::size_t logo_size,
                                       std::string& icon_content)
  {
    try
    {
      // create favicon request using google-api
      struct url url_props;
      if (!url_props.decode(url) || !url_props.is_valid())
        return false;
      const std::string& google_request = fmt::format("/s2/favicons?domain={}&sz={}", url_props.base, logo_size);

      // download favicon using google-api
      const std::string& data = download_data({ "www.google.com", google_request, 443u });
      if (data.empty())
        return false;
      icon_content = data;
      return true;
    }
    catch (...)
    {
      return false;
    }
  }
}