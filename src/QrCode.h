#pragma once
#include <map>
#include <string>
#include <memory>
#include "QrCodeOpts.h"

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

  // generate png image of qrcode in std::string
  const std::string get() const;

private:
  // pointer to internal implementation
  std::unique_ptr<QrCodeImpl> m_pimpl;
};