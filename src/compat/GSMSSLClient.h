
#ifndef _GSM_SSL_CLIENT_H_INCLUDED
#define _GSM_SSL_CLIENT_H_INCLUDED

#include "GSMClient.h"
#include "utility/GSMRootCerts.h"
class GSMSSLClient : public GSMClient {

public:
    GSMSSLClient(bool synch = true);
    virtual ~GSMSSLClient();

    virtual int ready();

    virtual int connect(IPAddress ip, uint16_t port);
    virtual int connect(const char* host, uint16_t port);
    virtual void setSignedCertificate(const uint8_t* cert, const char* name, size_t size);
    virtual void setPrivateKey(const uint8_t* key, const char* name, size_t size);
    virtual void useSignedCertificate(const char* name);
    virtual void usePrivateKey(const char* name);
    virtual void setTrustedRoot(const char* name);
    virtual void setUserRoots(const GSMRootCert * userRoots, size_t size);
    virtual void eraseTrustedRoot();

private:
    static bool _rootCertsLoaded;
    int _certIndex;
    int _state;
    const GSMRootCert * _gsmRoots;
    int _sizeRoot;

};

#endif
