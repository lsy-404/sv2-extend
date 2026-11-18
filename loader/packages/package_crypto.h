#pragma once
// AES-GCM for sealed package logic. A package reads its own ciphertext resource
// and decrypts it in memory once it has proven its own entitlement; plaintext
// never reaches disk. Keys and buffers are raw bytes. The GCM shape (96-bit
// nonce, 128-bit tag, key length selects the variant) is shared; the primitive
// itself is Windows CNG (BCrypt) or Apple CommonCrypto.
#include <string>

namespace package_crypto {
constexpr size_t payload_limit=1u<<20;
inline bool variant(const std::string &algorithm,const std::string &key){
 if(algorithm=="aes-128-gcm")return key.size()==16;
 if(algorithm=="aes-192-gcm")return key.size()==24;
 if(algorithm=="aes-256-gcm")return key.size()==32;
 return false;
}
inline bool shape(const std::string &algorithm,const std::string &key,const std::string &iv,size_t payload,std::string &error){
 if(!variant(algorithm,key)){error="ALGORITHM";return false;}
 if(iv.size()!=12){error="IV_SIZE";return false;}
 if(payload>payload_limit){error="PAYLOAD_SIZE";return false;}
 return true;
}
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif
namespace package_crypto {
struct Key{
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE handle=nullptr;
 ~Key(){if(handle)BCryptDestroyKey(handle);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
 bool open(const std::string &key,std::string &error){
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)!=STATUS_SUCCESS){error="PROVIDER";return false;}
  if(BCryptSetProperty(algorithm,BCRYPT_CHAINING_MODE,(PUCHAR)BCRYPT_CHAIN_MODE_GCM,sizeof(BCRYPT_CHAIN_MODE_GCM),0)!=STATUS_SUCCESS){error="MODE";return false;}
  if(BCryptGenerateSymmetricKey(algorithm,&handle,nullptr,0,(PUCHAR)key.data(),(ULONG)key.size(),0)!=STATUS_SUCCESS){error="KEY";return false;}
  return true;
 }
};
inline void authenticate(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO &info,const std::string &iv,const std::string &aad,PUCHAR tag,ULONG tag_length){
 BCRYPT_INIT_AUTH_MODE_INFO(info);info.pbNonce=(PUCHAR)iv.data();info.cbNonce=(ULONG)iv.size();
 if(!aad.empty()){info.pbAuthData=(PUCHAR)aad.data();info.cbAuthData=(ULONG)aad.size();}
 info.pbTag=tag;info.cbTag=tag_length;
}
inline bool decrypt(const std::string &algorithm,const std::string &key,const std::string &iv,const std::string &cipher,const std::string &tag,const std::string &aad,std::string &out,std::string &error){
 if(!shape(algorithm,key,iv,cipher.size(),error))return false;
 if(tag.size()!=16){error="TAG_SIZE";return false;}
 Key material;if(!material.open(key,error))return false;
 BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;authenticate(info,iv,aad,(PUCHAR)tag.data(),(ULONG)tag.size());
 out.assign(cipher.size(),'\0');ULONG produced=0;
 NTSTATUS status=BCryptDecrypt(material.handle,(PUCHAR)cipher.data(),(ULONG)cipher.size(),&info,nullptr,0,cipher.empty()?nullptr:(PUCHAR)&out[0],(ULONG)out.size(),&produced,0);
 if(status!=STATUS_SUCCESS){out.clear();error="AUTH";return false;}
 out.resize(produced);return true;
}
inline bool encrypt(const std::string &algorithm,const std::string &key,const std::string &iv,const std::string &plain,const std::string &aad,std::string &out,std::string &tag,std::string &error){
 if(!shape(algorithm,key,iv,plain.size(),error))return false;
 Key material;if(!material.open(key,error))return false;
 tag.assign(16,'\0');BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;authenticate(info,iv,aad,(PUCHAR)&tag[0],(ULONG)tag.size());
 out.assign(plain.size(),'\0');ULONG produced=0;
 NTSTATUS status=BCryptEncrypt(material.handle,(PUCHAR)plain.data(),(ULONG)plain.size(),&info,nullptr,0,plain.empty()?nullptr:(PUCHAR)&out[0],(ULONG)out.size(),&produced,0);
 if(status!=STATUS_SUCCESS){out.clear();tag.clear();error="CIPHER";return false;}
 out.resize(produced);return true;
}
}
#elif defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
namespace package_crypto {
inline bool decrypt(const std::string &algorithm,const std::string &key,const std::string &iv,const std::string &cipher,const std::string &tag,const std::string &aad,std::string &out,std::string &error){
 if(!shape(algorithm,key,iv,cipher.size(),error))return false;
 if(tag.size()!=16){error="TAG_SIZE";return false;}
 out.assign(cipher.size(),'\0');
 CCCryptorStatus status=CCCryptorGCMOneshotDecrypt(kCCAlgorithmAES,key.data(),key.size(),iv.data(),iv.size(),aad.empty()?nullptr:aad.data(),aad.size(),cipher.empty()?nullptr:cipher.data(),cipher.size(),out.empty()?nullptr:&out[0],tag.data(),tag.size());
 if(status!=kCCSuccess){out.clear();error="AUTH";return false;}
 return true;
}
inline bool encrypt(const std::string &algorithm,const std::string &key,const std::string &iv,const std::string &plain,const std::string &aad,std::string &out,std::string &tag,std::string &error){
 if(!shape(algorithm,key,iv,plain.size(),error))return false;
 out.assign(plain.size(),'\0');tag.assign(16,'\0');
 CCCryptorStatus status=CCCryptorGCMOneshotEncrypt(kCCAlgorithmAES,key.data(),key.size(),iv.data(),iv.size(),aad.empty()?nullptr:aad.data(),aad.size(),plain.empty()?nullptr:plain.data(),plain.size(),out.empty()?nullptr:&out[0],&tag[0],tag.size());
 if(status!=kCCSuccess){out.clear();tag.clear();error="CIPHER";return false;}
 return true;
}
}
#else
#error "package_crypto has no AES-GCM backend for this platform"
#endif
