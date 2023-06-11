#pragma once
#include <map>
#include <string>
#include <memory>
#include "QrCodeOpts.h"

namespace qr
{
  struct PngImage
  {
    std::size_t width = 0;
    std::size_t height = 0;
    std::string data;
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
    QrCode(const std::initializer_list<details::OptionsVal>& opts);
    ~QrCode();

    // set a list of options
    void set(const std::initializer_list<details::OptionsVal>& opts);

    // generate a QR Code in a PNG image
    const struct PngImage get() const;

  private:
    // pointer to internal implementation
    std::unique_ptr<QrCodeImpl> m_pimpl;
  };
}