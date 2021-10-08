#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <gskcms.h>
#include <gskssl.h>
#include <gsktypes.h>

int base64urlToBase64(char *s, int bufSize) {
  int i = 0;
  unsigned int c;
  int rc = 0;

  while (((c = s[i]) != '\0') && (i < bufSize)) {
    switch (c) {
    case '-':
      s[i] = '+';
      break;
    case '_':
      s[i] = '/';
      break;
    }
    i++;
  }
  switch (i % 4) {
    case 2:
      if (i >= bufSize - 2) {
        rc = -2;
        break;
      } else {
        s[i++] = '=';
        rc++;
        /* fall through */
      }
    case 3:
      if (i >= bufSize - 1) {
        rc = -1;
        break;
      } else {
        s[i++] = '=';
        rc++;
        /* fall through */
      }
    case 0:
      s[i++] = '\0';
      break;
    default:
      rc = -3;
  }
  return rc;
}

char *decodeSignature(char *signature) {
  size_t len = strlen(signature);
  size_t size = len * 2 + 1;
  char *decoded = malloc(size);
  strcpy(decoded, signature);
  int rc = base64urlToBase64(decoded, size);
  return decoded;
}

int create_public_key(x509_public_key_info *publicKey) {
  gsk_status status;
  char modulus[4096] = "305HKB30ZvmscZHv0IKdPm0a9SxSrN9wWJEYo9QuSocQbYPQkP1QSt4r8oGkbD82gzDOphEogTnpJ2AyzI7FsvoftAOCLJct6E3GptXHfD1oKCWtHuEYdVFc-ZM9n4pSVErhfmyzbqo89tuzm9HEP148h68kDK-zPbW4QQC6mAjVdZuRXyZkxTa2ypB6lTCyce1glmK7yyJc09LKYUCfjwiVEL7K5xmbIWVK5wkEWSxoBh6tdRQgkeeoGbN3vIEcsc2CuARryhmOnDg1RUWq-wJ-BYZWB-Dq_6o70QZv8x7bZwLtdYXnv3saE3mvrmT6Ss99LMIkS8_iivsfjOpVUw";
  char exponent[1024] = "AQAB";

  if (base64urlToBase64(modulus, sizeof(modulus)) < 0) {
    fprintf (stdout, "failed to base64urlToBase64 modulus\n");
    return -1;
  }
  gsk_buffer base64ModulusBuffer = {
    .data = (void*)modulus,
    .length = strlen(modulus)
  };
  char modulusBuf[4096];
  gsk_buffer modulusBuffer = {
    .data = (void*)modulusBuf,
    .length = sizeof(modulusBuf)
  };

  status = gsk_decode_base64(&base64ModulusBuffer, &modulusBuffer);
  if (status != 0) {
    fprintf (stdout, "failed to base64 decode modulus status %d - %s\n", status, gsk_strerror(status));
    return status;
  }

  if (base64urlToBase64(exponent, sizeof(exponent)) < 0) {
    fprintf (stdout, "failed to base64urlToBase64 exponent\n");
    return -1;
  }
  gsk_buffer base64ExponentBuffer = {
    .data = (void*)exponent,
    .length = strlen(exponent)
  };
  char exponentBuf[4096];
  gsk_buffer exponentBuffer = {
    .data = (void*)exponentBuf,
    .length = sizeof(exponentBuf)
  };

  status = gsk_decode_base64(&base64ExponentBuffer, &exponentBuffer);
  if (status != 0) {
    fprintf (stdout, "failed to base64 decode exponent status %d - %s\n", status, gsk_strerror(status));
    return status;
  }
  status = gsk_construct_public_key_rsa (&modulusBuffer, &exponentBuffer, publicKey);  
  if (status != 0) {
    fprintf (stdout, "failed to construct public key with status %d - %s\n", status, gsk_strerror(status));
    return status;
  }
  fprintf (stdout, "public key constructed\n");
  return status;
}

int verify_signature(x509_public_key_info *publicKey) {
  gsk_status status;
  x509_algorithm_type alg = x509_alg_sha256WithRsaEncryption;
#pragma convert("UTF8")
  const char *data = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ0czMxMDUiLCJsdHBhIjoicjNEQmFyRlNlUEtLR0E1eGRaMk5aY1lLeXpRZjJrK2pXNGFPeGFQMEN1dFd3Y04waFdmV0tuRFNFajEvVHJtNDdQclEvcWVxeEVydHh3VVdOWWhRN2NNdWV6OWhJQ0RGMTBXbkpBY3FoMXhSQ1dsaGZzb3ZGTzZpR21rbDNVdGp0eE1iOXFoVWNmMWN2VTZSVUdaRDd2YW9Gd2hET2QvbFBPN3VhYnZlSm11UU1tc05XMS80MHo4NEp1M3JFMTdkek1CelFSVmlicjdzQ0xhbEl0Qm05YS9WNUM5UHhpSmV5cDQxcDc1TDV5V0djbXoxaXNHak50RnJnT2ozVmNZZmZRWW85MjJydHFLREZZUnZqbmo0dWVQNGRsYk5RK2s2aWhUc1JHWE8yS3VldkMyZUJrR2pHcyt4YldNdUFLaXgiLCJpYXQiOjE2MzM2Njk3OTMsImV4cCI6MTYzMzY5ODU5MywiaXNzIjoiQVBJTUwiLCJqdGkiOiJkMTUxZDIxMi1iZmI3LTQ5ZjgtODVkMi0zMjEwNGNkZTdlNmMifQ";
#pragma convert(pop)
  char signature[] = "2LqNn3GD8KSMXHHt21UahNMhM4XX4njQSv7tqx_9itff3N2i-1EDMaZjR59mEy2woFytQwryTVtsdlCVLgMFROgEGBO729TLNyDj26VX7kjE2Z8bn6G9UjkmMsCF4bAVFP1vm6Jr1GnJ02L21BQ-msX2ysl1QndVnG1gARp8JMN5NSyl3l46zXY1q6JsYsM5QtCAqCHQg2BjSUFLOwEZm5sokc89pUNlFA2A3dqeJDf86bPHLFLO-Gs7ixySiMqaBGLvB256zvEzdilOn2DPFs0-aoBOcVTgwGGVcRAYr9VBCtSOUQmrj4OTugZ0jr5GPBeewYrqczJQ0nk0Hw2c2A";
  gsk_buffer dataBuffer = {
    .data = (void*)data,
    .length = strlen(data)
  };
  char signatureCharBuffer[2048];
  gsk_buffer signatureBuffer = {
    .data = (void*)signatureCharBuffer,
    .length = sizeof(signatureCharBuffer)
  };
  char *decoded = decodeSignature(signature);

  gsk_buffer encodedSignatureBuffer = {
    .data = (void*)decoded,
    .length = strlen(decoded)
  };
  status = gsk_decode_base64(&encodedSignatureBuffer, &signatureBuffer);
  if (status != 0) {
    fprintf (stdout, "failed to decode 64 signature status %d - %s\n", status, gsk_strerror(status));
    return status;
  }
  status = gsk_verify_data_signature(alg, publicKey, 0, &dataBuffer, &signatureBuffer);
  if (status != 0) {
    fprintf (stdout, "failed to verify signature with status %d - %s\n", status, gsk_strerror(status));
    return status;
  }
  fprintf (stdout, "signature verified\n");
  return status;
}

int main(int argc, char *argv[]) {
  x509_public_key_info publicKey;
  if (create_public_key(&publicKey)) {
    return 0;
  }
  if (verify_signature(&publicKey)) {
    return 0;
  }
  return 0;
}
