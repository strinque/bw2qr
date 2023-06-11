#pragma once
#include <string>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace base64
{
  // encode in base64 a std::string using openssl
  inline std::string encode(const std::string& data)
  {
    // convert in base64
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);

    // extract data from buffer to std::string
    BUF_MEM* buf = nullptr;
    BIO_get_mem_ptr(bio, &buf);
    std::string base64_str(buf->data, buf->length);

    // free BIOs memory
    BIO_free_all(bio);
    return base64_str;
  }

  // decode a base64 std::string into a std::vector<unsigned char> using openssl
  std::vector<unsigned char> decode(const std::string& data)
  {
    // convert from base64
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new_mem_buf(data.c_str(), data.size());
    bio = BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // decode base64
    const int buf_len = 1024;
    char buffer[buf_len];
    std::vector<unsigned char> buf;
    while (true)
    {
      const int bytes = BIO_read(bio, buffer, buf_len);
      if (bytes > 0)
        buf.insert(buf.end(), buffer, buffer + bytes);
      else
        break;
    }

    // free BIOs memory
    BIO_free_all(bio);
    return buf;
  }
}