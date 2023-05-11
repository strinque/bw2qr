#pragma once
#include <map>
#include <string>
#include <memory>

// entry definition with all extracted fields
struct entry {
  std::string name;
  std::string username;
  std::string password;
  std::string totp;
  std::string url;
  std::map<std::string, std::string> fields;
};

class QrCodeImpl;
class QrCode final
{
  // delete copy/assignement operators
  QrCode(const QrCode&) = delete;
  QrCode& operator=(const QrCode&) = delete;
  QrCode(QrCode&&) = delete;
  QrCode& operator=(QrCode&&) = delete;

public:
  // constructor/destructor
  QrCode(const struct entry& entry);
  ~QrCode();

  // generate html qr-code
  const std::string get_html() const;

private:
  // pointer to internal implementation
  std::unique_ptr<QrCodeImpl> m_pimpl;
};