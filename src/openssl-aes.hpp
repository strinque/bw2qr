#pragma once
#include <cstring>
#include <vector>
#include <utility>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "openssl-base64.hpp"

namespace aes
{
  // hash std::string using SHA-256 algorithm
  inline const std::vector<unsigned char> hash_sha256(const std::string& data)
  {
    // initialize SHA-256 context
    const EVP_MD* md = EVP_sha256();
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, nullptr);
    EVP_DigestUpdate(ctx, data.c_str(), data.length());

    // execute the hashing process
    constexpr std::size_t hash_size = AES_BLOCK_SIZE * 2;
    std::vector<unsigned char> hash(EVP_MAX_MD_SIZE, 0);
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, hash.data(), &len);
    EVP_MD_CTX_free(ctx);
    if (len != hash_size)
      throw std::runtime_error("invalid hash size: " + std::to_string(len) + " (should be " + std::to_string(hash_size) + ")");
    hash.resize(hash_size);
    return hash;
  }

  // generate random IV
  inline const std::vector<unsigned char> generate_iv()
  {
    std::vector<unsigned char> iv(AES_BLOCK_SIZE, 0);
    if (RAND_bytes(iv.data(), iv.size()) != 1)
      throw std::runtime_error("can't generate random IV buf");
    return iv;
  }

  // encrypt data using:
  //  cipher algorithm:         AES-256-CBC
  //  key hash algorithm:       SHA-256
  //  data padding:             NONE
  inline const std::string encrypt_256_cbc(const std::string& data, 
                                           const std::string& iv_b64,
                                           const std::string& password)
  {
    // initialize context
    EVP_CIPHER_CTX* ctx = nullptr;
    try
    {
      // create and initialize the context
      ctx = EVP_CIPHER_CTX_new();
      if (!ctx)
        throw std::runtime_error("can't initialize the openssl cipher context");

      // configure cipher context for aes-256-cbc
      const std::vector<unsigned char>& key_buf = hash_sha256(password);
      const std::vector<unsigned char>& iv_buf = base64::decode(iv_b64);
      if ((key_buf.size() != AES_BLOCK_SIZE * 2) ||
          (iv_buf.size() != AES_BLOCK_SIZE))
        throw std::runtime_error("invalid key or iv size");
      if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_buf.data(), iv_buf.data()) != 1)
        throw std::runtime_error("can't configure cipher context for aes-256-cbc with key");

      // encrypt all blocs of 16 bytes of data
      std::string cipher(data.size() + AES_BLOCK_SIZE, 0);
      int len = 0;
      if (EVP_EncryptUpdate(ctx,
                            reinterpret_cast<unsigned char*>(cipher.data()),
                            &len,
                            reinterpret_cast<const unsigned char*>(data.c_str()),
                            data.size()) != 1)
        throw std::runtime_error("can't encrypt data using aes-256-cbc algorithm");
      int cipher_len = len;

      // encrypt the last bloc and finalize encryption
      if (EVP_EncryptFinal_ex(ctx,
                              reinterpret_cast<unsigned char*>(cipher.data()) + len,
                              &len) != 1)
        throw std::runtime_error("can't finalize encryption process");
      cipher_len += len;
      cipher.resize(cipher_len);

      // free cipher context
      EVP_CIPHER_CTX_free(ctx);

      // conver to base64
      return base64::encode(cipher);
    }
    catch (const std::exception& ex)
    {
      // free cipher context
      if (ctx)
        EVP_CIPHER_CTX_free(ctx);
      throw ex;
    }
  }
}
