
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <vector>
#include <string>
using namespace std;

#include "hci_server.h"
#include "hid_server.h"

enum {
    HID_SDP_PSM = 0x0001,
    HID_CONTROL_PSM = 0x0011,
    HID_INTERRUPT_PSM = 0x0013,
};

class SDPParse;
class InputDevice {
public:

    enum State {
        CLOSED,
        CONNECT,
        READING_NAME,
        READING_SDP,
        AUTHENTICATING,
        OPENING,
        OPEN
    };

    InputDevice();
    ~InputDevice();

    State _state;
    bdaddr_t _bdaddr;
    int _dev_class;
    string _name;

    SDPParse* _sdp_handler;
    string _hid_descriptor;

    bool _reconnect;
    int _sdp;
    int _control;
    int _interrupt;

    const char* name();
    void connect();
    void authentication_complete(int status);
    void connection_request();
    void connection_complete(int status);
    void remote_name_response(const char* name);
    void disconnection_complete();
    void socket_changed(int socket, int state);

    void start_sdp();
    void update_sdp();
};

//==================================================================
//==================================================================
// clunky sdp

/* AttrID name lookup table */
typedef struct {
    int   attr_id;
    const char* name;
} sdp_attr_id_nam_lookup_table_t;

#define SDP_ATTR_ID_SERVICE_RECORD_HANDLE              0x0000
#define SDP_ATTR_ID_SERVICE_CLASS_ID_LIST              0x0001
#define SDP_ATTR_ID_SERVICE_RECORD_STATE               0x0002
#define SDP_ATTR_ID_SERVICE_SERVICE_ID                 0x0003
#define SDP_ATTR_ID_PROTOCOL_DESCRIPTOR_LIST           0x0004
#define SDP_ATTR_ID_BROWSE_GROUP_LIST                  0x0005
#define SDP_ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST    0x0006
#define SDP_ATTR_ID_SERVICE_INFO_TIME_TO_LIVE          0x0007
#define SDP_ATTR_ID_SERVICE_AVAILABILITY               0x0008
#define SDP_ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST  0x0009
#define SDP_ATTR_ID_DOCUMENTATION_URL                  0x000A
#define SDP_ATTR_ID_CLIENT_EXECUTABLE_URL              0x000B
#define SDP_ATTR_ID_ICON_URL                           0x000C
#define SDP_ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS     0x000D
#define SDP_ATTR_ID_SERVICE_NAME                       0x0100
#define SDP_ATTR_ID_SERVICE_DESCRIPTION                0x0101
#define SDP_ATTR_ID_PROVIDER_NAME                      0x0102
#define SDP_ATTR_ID_VERSION_NUMBER_LIST                0x0200
#define SDP_ATTR_ID_GROUP_ID                           0x0200
#define SDP_ATTR_ID_SERVICE_DATABASE_STATE             0x0201
#define SDP_ATTR_ID_SERVICE_VERSION                    0x0300

#define SDP_ATTR_ID_EXTERNAL_NETWORK                   0x0301 /* Cordless Telephony */
#define SDP_ATTR_ID_SUPPORTED_DATA_STORES_LIST         0x0301 /* Synchronization */
#define SDP_ATTR_ID_REMOTE_AUDIO_VOLUME_CONTROL        0x0302 /* GAP */
#define SDP_ATTR_ID_SUPPORTED_FORMATS_LIST             0x0303 /* OBEX Object Push */
#define SDP_ATTR_ID_FAX_CLASS_1_SUPPORT                0x0302 /* Fax */
#define SDP_ATTR_ID_FAX_CLASS_2_0_SUPPORT              0x0303
#define SDP_ATTR_ID_FAX_CLASS_2_SUPPORT                0x0304
#define SDP_ATTR_ID_AUDIO_FEEDBACK_SUPPORT             0x0305
#define SDP_ATTR_ID_SECURITY_DESCRIPTION               0x030a /* PAN */
#define SDP_ATTR_ID_NET_ACCESS_TYPE                    0x030b /* PAN */
#define SDP_ATTR_ID_MAX_NET_ACCESS_RATE                0x030c /* PAN */
#define SDP_ATTR_ID_IPV4_SUBNET                        0x030d /* PAN */
#define SDP_ATTR_ID_IPV6_SUBNET                        0x030e /* PAN */

#define SDP_ATTR_ID_SUPPORTED_CAPABILITIES             0x0310 /* Imaging */
#define SDP_ATTR_ID_SUPPORTED_FEATURES                 0x0311 /* Imaging and Hansfree */
#define SDP_ATTR_ID_SUPPORTED_FUNCTIONS                0x0312 /* Imaging */
#define SDP_ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY        0x0313 /* Imaging */
#define SDP_ATTR_ID_SUPPORTED_REPOSITORIES             0x0314 /* PBAP */

#define SDP_ATTR_HID_DEVICE_RELEASE_NUMBER    0x0200
#define SDP_ATTR_HID_PARSER_VERSION        0x0201
#define SDP_ATTR_HID_DEVICE_SUBCLASS        0x0202
#define SDP_ATTR_HID_COUNTRY_CODE        0x0203
#define SDP_ATTR_HID_VIRTUAL_CABLE        0x0204
#define SDP_ATTR_HID_RECONNECT_INITIATE        0x0205
#define SDP_ATTR_HID_DESCRIPTOR_LIST        0x0206
#define SDP_ATTR_HID_LANG_ID_BASE_LIST        0x0207

#define SDP_ATTR_HID_SDP_DISABLE        0x0208
#define SDP_ATTR_HID_BATTERY_POWER        0x0209
#define SDP_ATTR_HID_REMOTE_WAKEUP        0x020a
#define SDP_ATTR_HID_PROFILE_VERSION        0x020b
#define SDP_ATTR_HID_SUPERVISION_TIMEOUT    0x020c
#define SDP_ATTR_HID_NORMALLY_CONNECTABLE    0x020d
#define SDP_ATTR_HID_BOOT_DEVICE        0x020e


static sdp_attr_id_nam_lookup_table_t sdp_attr_id_nam_lookup_table[] = {
    { SDP_ATTR_HID_DEVICE_RELEASE_NUMBER, "SDP_ATTR_HID_DEVICE_RELEASE_NUMBER"},
    { SDP_ATTR_HID_PARSER_VERSION, "SDP_ATTR_HID_PARSER_VERSION"},
    { SDP_ATTR_HID_DEVICE_SUBCLASS, "SDP_ATTR_HID_DEVICE_SUBCLASS"},
    { SDP_ATTR_HID_COUNTRY_CODE, "SDP_ATTR_HID_COUNTRY_CODE"},
    { SDP_ATTR_HID_VIRTUAL_CABLE, "SDP_ATTR_HID_VIRTUAL_CABLE"},
    { SDP_ATTR_HID_RECONNECT_INITIATE, "SDP_ATTR_HID_RECONNECT_INITIATE"},
    { SDP_ATTR_HID_DESCRIPTOR_LIST, "SDP_ATTR_HID_DESCRIPTOR_LIST"},
    { SDP_ATTR_HID_LANG_ID_BASE_LIST, "SDP_ATTR_HID_LANG_ID_BASE_LIST"},

    { SDP_ATTR_HID_SDP_DISABLE, "SDP_ATTR_HID_SDP_DISABLE"},
    { SDP_ATTR_HID_BATTERY_POWER, "SDP_ATTR_HID_BATTERY_POWER"},
    { SDP_ATTR_HID_REMOTE_WAKEUP, "SDP_ATTR_HID_REMOTE_WAKEUP"},
    { SDP_ATTR_HID_SDP_DISABLE, "SDP_ATTR_HID_SDP_DISABLE"},
    { SDP_ATTR_HID_PROFILE_VERSION, "SDP_ATTR_HID_PROFILE_VERSION"},
    { SDP_ATTR_HID_SUPERVISION_TIMEOUT, "SDP_ATTR_HID_SUPERVISION_TIMEOUT"},
    { SDP_ATTR_HID_NORMALLY_CONNECTABLE, "SDP_ATTR_HID_NORMALLY_CONNECTABLE"},
    { SDP_ATTR_HID_BOOT_DEVICE, "SDP_ATTR_HID_BOOT_DEVICE"},

    { SDP_ATTR_ID_SERVICE_RECORD_HANDLE,             "SrvRecHndl"         },
    { SDP_ATTR_ID_SERVICE_CLASS_ID_LIST,             "SrvClassIDList"     },
    { SDP_ATTR_ID_SERVICE_RECORD_STATE,              "SrvRecState"        },
    { SDP_ATTR_ID_SERVICE_SERVICE_ID,                "SrvID"              },
    { SDP_ATTR_ID_PROTOCOL_DESCRIPTOR_LIST,          "ProtocolDescList"   },
    { SDP_ATTR_ID_BROWSE_GROUP_LIST,                 "BrwGrpList"         },
    { SDP_ATTR_ID_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,   "LangBaseAttrIDList" },
    { SDP_ATTR_ID_SERVICE_INFO_TIME_TO_LIVE,         "SrvInfoTimeToLive"  },
    { SDP_ATTR_ID_SERVICE_AVAILABILITY,              "SrvAvail"           },
    { SDP_ATTR_ID_BLUETOOTH_PROFILE_DESCRIPTOR_LIST, "BTProfileDescList"  },
    { SDP_ATTR_ID_DOCUMENTATION_URL,                 "DocURL"             },
    { SDP_ATTR_ID_CLIENT_EXECUTABLE_URL,             "ClientExeURL"       },
    { SDP_ATTR_ID_ICON_URL,                          "IconURL"            },
    { SDP_ATTR_ID_ADDITIONAL_PROTOCOL_DESC_LISTS,    "AdditionalProtocolDescLists" },
    { SDP_ATTR_ID_SERVICE_NAME,                      "SrvName"            },
    { SDP_ATTR_ID_SERVICE_DESCRIPTION,               "SrvDesc"            },
    { SDP_ATTR_ID_PROVIDER_NAME,                     "ProviderName"       },
    { SDP_ATTR_ID_VERSION_NUMBER_LIST,               "VersionNumList"     },
    { SDP_ATTR_ID_GROUP_ID,                          "GrpID"              },
    { SDP_ATTR_ID_SERVICE_DATABASE_STATE,            "SrvDBState"         },
    { SDP_ATTR_ID_SERVICE_VERSION,                   "SrvVersion"         },
    { SDP_ATTR_ID_SECURITY_DESCRIPTION,              "SecurityDescription"}, /* PAN */
    { SDP_ATTR_ID_SUPPORTED_DATA_STORES_LIST,        "SuppDataStoresList" }, /* Synchronization */
    { SDP_ATTR_ID_SUPPORTED_FORMATS_LIST,            "SuppFormatsList"    }, /* OBEX Object Push */
    { SDP_ATTR_ID_NET_ACCESS_TYPE,                   "NetAccessType"      }, /* PAN */
    { SDP_ATTR_ID_MAX_NET_ACCESS_RATE,               "MaxNetAccessRate"   }, /* PAN */
    { SDP_ATTR_ID_IPV4_SUBNET,                       "IPv4Subnet"         }, /* PAN */
    { SDP_ATTR_ID_IPV6_SUBNET,                       "IPv6Subnet"         }, /* PAN */
    { SDP_ATTR_ID_SUPPORTED_CAPABILITIES,            "SuppCapabilities"   }, /* Imaging */
    { SDP_ATTR_ID_SUPPORTED_FEATURES,                "SuppFeatures"       }, /* Imaging and Hansfree */
    { SDP_ATTR_ID_SUPPORTED_FUNCTIONS,               "SuppFunctions"      }, /* Imaging */
    { SDP_ATTR_ID_TOTAL_IMAGING_DATA_CAPACITY,       "SuppTotalCapacity"  }, /* Imaging */
    { SDP_ATTR_ID_SUPPORTED_REPOSITORIES,            "SuppRepositories"   }, /* PBAP */
    { -1, NULL }
};

static
const char* get_name(int id)
{
    for (int i = 0; sdp_attr_id_nam_lookup_table[i].name; i++)
        if (sdp_attr_id_nam_lookup_table[i].attr_id == id)
            return sdp_attr_id_nam_lookup_table[i].name;
    return "nope?";
}

class SDPParse {
    int _sdp_sock;
    int _current_id;
    const uint8_t *_data;
    vector<uint8_t> _buf;   // accumulate full search results before parsing

    string _SrvName;
    string _SrvDesc;
    string _ProviderName;
    string _HIDDescriptor;
public:
    SDPParse(int sock) : _sdp_sock(sock)
    {
       if (sock)
           service_search();
    }

    int service_search(const uint8_t* cont = 0, int contlen = 0)
    {
        // 4.7.1 SDP_ServiceSearchAttributeRequest PDU
        const uint8_t _sdp_hid_inquiry[] = {
            0x06,                               // SDP_ServiceSearchAttributeRequest
            0x00,0x01,
            0x00,0x0F,
            0x35,0x03,0x19,0x11,0x24,           // search for 1124 hid
            0xFF,0xFF,                          // max return
            0x35,0x05,0x0A,0x00,0x00,0xFF,0xFF, // AttributeIDList, range from 0000-FFFF
        };
        uint8_t buf[sizeof(_sdp_hid_inquiry) + 1 + 16];
        memcpy(buf,_sdp_hid_inquiry,sizeof(_sdp_hid_inquiry));
        buf[sizeof(_sdp_hid_inquiry)] = contlen;
        buf[4] += contlen;
        if (contlen)
            memcpy(buf+sizeof(_sdp_hid_inquiry)+1,cont,contlen);
        return l2_send(_sdp_sock,buf,sizeof(_sdp_hid_inquiry)+1+contlen);
    }

    int update()
    {
        uint8_t buf[512];
        int len = l2_recv(_sdp_sock,buf,sizeof(buf));
        if (len > 0)
            if (add(buf,len) == 0)
                return 0;   // complete record
        return 1;
    }

    int u8()
    {
        return *_data++;
    }
    int u16()
    {
        return (u8() << 8) | u8();
    }
    int u32()
    {
        return (u16() << 16) | u16();
    }

    #define SDP_DE_NULL   0
    #define SDP_DE_UINT   1
    #define SDP_DE_INT    2
    #define SDP_DE_UUID   3
    #define SDP_DE_STRING 4
    #define SDP_DE_BOOL   5
    #define SDP_DE_SEQ    6
    #define SDP_DE_ALT    7
    #define SDP_DE_URL    8

    int header(uint32_t& n)
    {
        uint8_t hdr = u8();
        uint8_t type = hdr >> 3;
        uint8_t i = hdr & 0x07;
        n = 0;
        switch (i) {
            case 5: n = u8(); break;
            case 6: n = u16(); break;
            case 7: n = u32(); break;
            default:
                n = 1 << i;
        }
        if (type == 0)
            n = 0;  // NULL
        return type;
    }

    void PAD(int n)
    {
        while (n--)
            printf("    ");
    }

    int get_id16()
    {
        uint32_t n;
        int t = header(n);
        if (t != SDP_DE_UINT || n != 2) {
            printf("bad sdp id\n");
            return -1;
        }
        return u16();
    }

    void on_uuid(int n)
    {

    }

    void on_string(const string& s)
    {
        switch (_current_id) {
            case SDP_ATTR_ID_SERVICE_NAME:          _SrvName = s; break;
            case SDP_ATTR_ID_SERVICE_DESCRIPTION:   _SrvDesc = s; break;
            case SDP_ATTR_ID_PROVIDER_NAME:         _ProviderName = s; break;
            case SDP_ATTR_HID_DESCRIPTOR_LIST:      _HIDDescriptor = s; break;
        }
    }

    int item(int len, int level = 0, bool want_id = true)
    {
        PAD(level);
        printf("{\n");
        const uint8_t* end = _data + len;
        uint32_t n,id;
        while (_data < end) {
            if (want_id) {
                _current_id = get_id16();
                if (_current_id == -1)
                    return -1;
                printf("%s %d\n",get_name(_current_id),_current_id);
            }
            int t = header(n);
            switch (t) {
                case SDP_DE_NULL:
                    printf("SDP_DE_NULL\n");
                    break;
                case SDP_DE_SEQ:            // it is a sequence
                    item(n,level+1,false);  // just a list
                    break;
                case SDP_DE_INT:
                case SDP_DE_UINT:
                    switch (n) {
                        case 1:
                            id = u8();
                            PAD(level);
                            printf("SDP_DE_UINT8 %d\n",id);
                            break;
                        case 2:
                            id = u16();
                            PAD(level);
                            printf("SDP_DE_UINT16 %d\n",id);
                            break;
                        case 4:
                            id = u32();
                            PAD(level);
                            printf("SDP_DE_UINT32 %d\n",id);
                            break;
                        default:
                            PAD(level);
                            printf("type:%d skipping %d\n",t,n);
                            _data += n;
                    }
                    break;
                case SDP_DE_STRING:
                {
                    string s((const char*)_data,n);
                    _data += n;
                    on_string(s);
                    PAD(level);
                    printf("SDP_DE_STRING: %s\n",s.c_str());
                }
                    break;
                case SDP_DE_UUID:
                    on_uuid(n);
                    id = -1;
                    switch (n) {
                        case 2: id = u16(); break;
                        case 4: id = u32(); break;
                        default: _data += n;
                    }
                    PAD(level);
                    printf("SDP_DE_UUID 0x%08X\n",id);
                    break;
                case SDP_DE_BOOL:
                    PAD(level);
                    printf("SDP_DE_BOOL ");
                    printf("%s\n",*_data++ ? "true" : "false");
                    break;
                case SDP_DE_URL:
                    printf("SDP_DE_URL %d bytes\n",n);
                    _data += n;
                    break;
                default:
                    PAD(level);
                    printf("unhandled type:%d skipping %d\n",t,n);
                    _data += n;
            }
        }
        PAD(level);
        printf("}\n");
        return 0;
    }

    int list(int len, int level = 0)
    {
        const uint8_t* end = _data + len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t != SDP_DE_SEQ)
                break;
            if (item(n,level) == -1)
                return -1;
        }
        return 0;
    }

    void dump()
    {
        string s = _SrvName + " " + _SrvDesc + " " + _ProviderName;
        printf("%s\n",s.c_str());
        const uint8_t* d = (const uint8_t*)_HIDDescriptor.c_str();
        int n = (int)_HIDDescriptor.length();
        for (int i = 0; i < n; i++)
            printf("%02X,",d[i]);
    }

    void parse(const uint8_t* data, int len)
    {
        _data = data;
        const uint8_t* end = data + len;
        while (_data < end) {
            uint32_t n;
            int t = header(n);  // list
            if (t != SDP_DE_SEQ)
                break;
            if (list(n) == -1)
                break;
        }
    }

    // return hid descriptor if found
    string parse()
    {
        parse(&_buf[0],(int)_buf.size());
        return _HIDDescriptor;
    }

    // packet from sdp socket, accumulate full report before parsing
    int add(const uint8_t* data, int len)
    {
        _data = data;
        uint8_t pid = u8();
        uint16_t tid = u16();
        uint16_t total = u16();
        switch (pid) {
            case 7:     // search attribute resonse
            {
                int count = u16();
                const uint8_t* cont = _data + count;
                size_t pos = _buf.size();
                _buf.resize(pos + count);
                memcpy(&_buf[pos],_data,count);
                if (!cont[0])
                    return 0;   // go all we are going to get: we now have data in _buf
                service_search(cont+1,cont[0]);
                break;
            }
        }
        return 1;   // more to come?
    }
};


InputDevice::InputDevice() : _sdp_handler(0),_reconnect(0) {
    _name[0] = 0;
}

InputDevice::~InputDevice() {
    disconnection_complete();
    delete _sdp_handler;
}

void InputDevice::start_sdp()
{
    delete _sdp_handler;
    _sdp_handler = new SDPParse(_sdp);
}

void InputDevice::update_sdp()
{
    if (_sdp_handler && (_sdp_handler->update() == 0)) {
        _hid_descriptor = _sdp_handler->parse();
        delete _sdp_handler;
        _sdp_handler = 0;
        l2_close(_sdp);
        _sdp = 0;

        // got the sdp from the new device
        // do we want to authenticate?
        hci_authentication_requested(&_bdaddr);
        _state = AUTHENTICATING;
    }
}

// 1. First connection
//      [connect]
//      connection_complete
//      read remote name
//      if NEW
//          read sdp
//      authenticate
//      if NEW
//          get pin
//          store link key
//      open sockets

void InputDevice::connect()
{
    _reconnect = 0;
    _state = CONNECT;
    hci_connect(&_bdaddr);
}

void InputDevice::connection_request()
{
    _reconnect = 1;
    hci_accept_connection_request(&_bdaddr);
}

void InputDevice::connection_complete(int status)
{
    if (_reconnect) {   // start listening to reconnections
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM, true);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM, true);
    }
    hci_remote_name_request(&_bdaddr);
    _state = READING_NAME;
}

void InputDevice::remote_name_response(const char* n)
{
    _name = n;
    printf("remote_name_response:[%s]\n", name());

    uint8_t key[16];
    if (_reconnect || (read_link_key(&_bdaddr,key) == 0))       // do we know this device?
    {
        if (_reconnect)
            _state = OPENING;                   // we already have a link key
        else {
            hci_authentication_requested(&_bdaddr);
            _state = AUTHENTICATING;
        }
    } else {
        _sdp = l2_open(&_bdaddr,HID_SDP_PSM);
        _state = READING_SDP;
    }
}

void InputDevice::authentication_complete(int status)
{
    // entered the wrong kb code...
    if (status)
        printf("pin didn't work, won't create a link key: %d\n", status);

    if (!_reconnect) {
        _control = l2_open(&_bdaddr, HID_CONTROL_PSM);
        _interrupt = l2_open(&_bdaddr, HID_INTERRUPT_PSM);
    }
}

void InputDevice::disconnection_complete()
{
    l2_close(_sdp);     // these are already dead
    l2_close(_control);
    l2_close(_interrupt);

    _reconnect = 0;
    _interrupt = _control = _interrupt = 0;
    _state = CLOSED;
}

const char* _nams[] = {
    "L2CAP_NONE",
    "L2CAP_LISTENING",
    "L2CAP_OPENING",
    "L2CAP_OPEN",
    "L2CAP_CLOSED",
    "L2CAP_DELETED",
};

void InputDevice::socket_changed(int socket, int state)
{
    printf("socket_changed:%d %s\n",socket,_nams[state]);
    if ((socket == _sdp) && (state == L2CAP_OPEN))  // wait for sdp channel to be open before sending query
        start_sdp();
    else if ((socket == _control) && (state == L2CAP_OPEN)) {
        //uint8_t protocol = 0x70;    // ask for boot protocol
        //l2_send(_control,&protocol,1);
    }
}

const char* InputDevice::name()
{
    return _name.empty() ? batostr(_bdaddr) : _name.c_str();
}

class HIDSource {
    string _local_name;
    vector<InputDevice*> _devices;

    InputDevice* get_device(const bdaddr_t* bdaddr)
    {
        for (auto d : _devices)
            if (memcmp(&d->_bdaddr,bdaddr,6) == 0)
                return d;
        return NULL;
    }

    static int hci_cb(HCI_CALLBACK_EVENT evt, const void* data, int len, void *ref)
    {
        return ((HIDSource*)ref)->cb(evt,data,len);
    }

    bool is_input_device(const uint8_t* dev_class)
    {
        int dc = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
        int major = (dc >> 8) & 0x1f;
        int minor = (dc >> 16) >> 2;

        if (major == 5) {
            switch (minor & 15) {
                case 1: return true;  // joystick
                case 2: return true;  // gamepad
                case 3: return true;  // remote
            }
            switch (minor & 0x30) {
                case 0x10: return true; // Keyboard
                case 0x20: return true; // Pointing device
                case 0x30: return true; // Pointing device/keyboard
            }
        }
        if (dc == 0)
            return true;
        return false;
    }

    // add a device if we have not seen it before
    InputDevice* add_device(const bdaddr_t* addr, const uint8_t* dev_class)
    {
        if (dev_class && !is_input_device(dev_class))
            return 0;

        InputDevice* d = get_device(addr);
        if (!d) {
            d = new InputDevice();
            d->_bdaddr = *addr;
            d->_dev_class = (dev_class[0] << 16) | (dev_class[1] << 8) | dev_class[2];
            d->_control = d->_interrupt = d->_sdp = 0;
            d->_state = InputDevice::CLOSED;
            _devices.push_back(d);
            printf("%s:%06X input device added\n",batostr(d->_bdaddr),d->_dev_class);
        }
        return d;
    }

    // inbound (re)connection, is 1 if we want connection
    int connection_request(const connection_request_info& ci)
    {
        auto* d = add_device(&ci.bdaddr,ci.dev_class);
        if (d)
            d->connection_request();
        return 0;
    }

    int cb(HCI_CALLBACK_EVENT evt, const void* data, int len)
    {
        InputDevice* d;
        const auto& cb = *((const cbdata*)data);
        switch (evt) {
            case CALLBACK_READY:
                hci_start_inquiry(5);       // inquire for 5 seconds
                break;

            case CALLBACK_INQUIRY_RESULT:
                add_device(&cb.ii.bdaddr,cb.ii.dev_class);
                break;

            case CALLBACK_INQUIRY_DONE:
                for (auto* id : _devices)
                    id->connect();
                break;

            case CALLBACK_REMOTE_NAME:
                d = get_device(&cb.ri.bdaddr);
                if (d)
                    d->remote_name_response(cb.ri.name);
                break;

            case CALLBACK_CONNECTION_REQUEST:
                d = add_device(&cb.cri.bdaddr,cb.cri.dev_class);
                if (d)
                    d->connection_request();
                break;

            case CALLBACK_DISCONNECTION_COMP:   // reconnect on disconnect?
                d = get_device(&cb.bdaddr);
                if (d)
                    d->disconnection_complete();
                break;

            case CALLBACK_CONNECTION_COMPLETE:
                d = get_device(&cb.ci.bdaddr);
                if (d)
                    d->connection_complete(cb.ci.status);    // device connection is complete, open l2cap sockets
                break;

            case CALLBACK_AUTHENTICATION_COMPLETE:
                d = get_device(&cb.aci.bdaddr);
                if (d)
                    d->authentication_complete(cb.aci.status);
                break;

            case CALLBACK_L2CAP_SOCKET_STATE_CHANGED:
                d = get_device(&cb.ss.bdaddr);
                if (d)
                    d->socket_changed(cb.ss.socket,cb.ss.state);
                break;

            default:
                break;
        }
        return 0;
    }

    public:
    HIDSource(const char* localname) : _local_name(localname)
    {
        hci_init(hci_cb,this);
    }

    void hid_control(InputDevice* d, const uint8_t* data, int len)
    {
        printf("HID CONTROL %02X %d\n",data[0],len);
    }

    // get hid (events). max one per call
    // called from emu thread
    int get(uint8_t* dst, int dst_len)
    {
        for (auto d : _devices) {
            if (d->_interrupt) {
                int len = l2_recv(d->_interrupt,dst,dst_len);
                if (len > 0) {
                    return len;
                }
                if (len < 0) {
                    printf("hid shutting down TODO\n");
                    d->disconnection_complete();
                }
            }
            if (d->_sdp)
                d->update_sdp();
        }
        return 0;
    }
};

static uint32_t msToTicks(int ms)
{
    return ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(ms);
}

HIDServer::HIDServer(const char *local_name, uint32_t repeat_delay_ms, uint32_t repeat_rate_ms)
    : _local_name(local_name)
    , _hid_source(0)
    , _queue(0)
    , _task(0)
    , _repeat_delay_ms(repeat_delay_ms)
    , _repeat_rate_ms(repeat_rate_ms)
{

}

HIDServer::~HIDServer()
{
    end();
}

void HIDServer::begin()
{
    end();
    _hid_source = new HIDSource(_local_name.c_str());

    _queue = xQueueCreate(1, sizeof(HIDKey));
    xTaskCreate(&updateLoop, "HIDServer", HID_SERVER_STACK_SIZE, this, HID_SERVER_PRIO, &_task);
}

#define get_ms()    (unsigned long)


void HIDServer::updateLoop(void *arg)
{
    HIDServer *_this = (HIDServer *)arg;
    HIDKey key = {0};
    HIDKey rep = {0};
    uint64_t start_ts = 0ULL;
    bool do_repeat = false;
    uint32_t repeat_delay = _this->_repeat_delay_ms;

    while (true) {
        hci_update();
        int ret =_this->checkKeys(key);
        if (ret > 0) {
			key.pressed = true;
            rep = key;
            xQueueOverwrite(_this->_queue, &key);
            start_ts = esp_timer_get_time();
            repeat_delay = _this->_repeat_delay_ms;
            do_repeat = true;
        } else if (ret == 0) {
            do_repeat = false;
            start_ts = 0ULL;
			rep.pressed = false;
            xQueueOverwrite(_this->_queue, &rep);
        }

        if (do_repeat && _this->_repeat_rate_ms) {
            int32_t dt = (int32_t)((esp_timer_get_time() - start_ts) / 1000ULL);
            if (dt >= repeat_delay) {
                start_ts = esp_timer_get_time();
                repeat_delay = _this->_repeat_rate_ms;
                xQueueOverwrite(_this->_queue, &rep);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void HIDServer::end()
{
    if (_task) {
        vTaskDelete(_task);
        _task = 0;
    }

    if (_queue) {
        vQueueDelete(_queue);
        _queue = 0;
    }

    delete _hid_source;
    _hid_source = 0;
}

typedef struct
{
    const char *name;
    uint8_t     value;
} keymap_t;

#define EMPTY   {0,0}
#define KEY(v)  {#v, v}

static const keymap_t key_map[256] = {
    [0x00] = KEY(KEY_RESERVED),
    [0x01] = EMPTY,
    [0x02] = EMPTY,
    [0x03] = EMPTY,
    [0x04] = KEY(KEY_A),
    [0x05] = KEY(KEY_B),
    [0x06] = KEY(KEY_C),
    [0x07] = KEY(KEY_D),
    [0x08] = KEY(KEY_E),
    [0x09] = KEY(KEY_F),
    [0x0A] = KEY(KEY_G),
    [0x0B] = KEY(KEY_H),
    [0x0C] = KEY(KEY_I),
    [0x0D] = KEY(KEY_J),
    [0x0E] = KEY(KEY_K),
    [0x0F] = KEY(KEY_L),
    [0x10] = KEY(KEY_M),
    [0x11] = KEY(KEY_N),
    [0x12] = KEY(KEY_O),
    [0x13] = KEY(KEY_P),
    [0x14] = KEY(KEY_Q),
    [0x15] = KEY(KEY_R),
    [0x16] = KEY(KEY_S),
    [0x17] = KEY(KEY_T),
    [0x18] = KEY(KEY_U),
    [0x19] = KEY(KEY_V),
    [0x1A] = KEY(KEY_W),
    [0x1B] = KEY(KEY_X),
    [0x1C] = KEY(KEY_Y),
    [0x1D] = KEY(KEY_Z),
    [0x1E] = KEY(KEY_1),
    [0x1F] = KEY(KEY_2),
    [0x20] = KEY(KEY_3),
    [0x21] = KEY(KEY_4),
    [0x22] = KEY(KEY_5),
    [0x23] = KEY(KEY_6),
    [0x24] = KEY(KEY_7),
    [0x25] = KEY(KEY_8),
    [0x26] = KEY(KEY_9),
    [0x27] = KEY(KEY_0),
    [0x28] = KEY(KEY_ENTER),
    [0x29] = KEY(KEY_ESC),
    [0x2A] = KEY(KEY_BACKSPACE),
    [0x2B] = KEY(KEY_TAB),
    [0x2C] = KEY(KEY_SPACE),
    [0x2D] = KEY(KEY_MINUS),
    [0x2E] = KEY(KEY_EQUAL),
    [0x2F] = KEY(KEY_LEFTBRACE),
    [0x30] = KEY(KEY_RIGHTBRACE),
    [0x31] = KEY(KEY_BACKSLASH),
    [0x32] = KEY(KEY_BACKSLASH),
    [0x33] = KEY(KEY_SEMICOLON),
    [0x34] = KEY(KEY_APOSTROPHE),
    [0x35] = KEY(KEY_GRAVE),
    [0x36] = KEY(KEY_COMMA),
    [0x37] = KEY(KEY_DOT),
    [0x38] = KEY(KEY_SLASH),
    [0x39] = KEY(KEY_CAPSLOCK),
    [0x3A] = KEY(KEY_F1),
    [0x3B] = KEY(KEY_F2),
    [0x3C] = KEY(KEY_F3),
    [0x3D] = KEY(KEY_F4),
    [0x3E] = KEY(KEY_F5),
    [0x3F] = KEY(KEY_F6),
    [0x40] = KEY(KEY_F7),
    [0x41] = KEY(KEY_F8),
    [0x42] = KEY(KEY_F9),
    [0x43] = KEY(KEY_F10),
    [0x44] = KEY(KEY_F11),
    [0x45] = KEY(KEY_F12),
    [0x46] = KEY(KEY_SYSRQ),
    [0x47] = KEY(KEY_SCROLLLOCK),
    [0x48] = KEY(KEY_PAUSE),
    [0x49] = KEY(KEY_INSERT),
    [0x4A] = KEY(KEY_HOME),
    [0x4B] = KEY(KEY_PAGEUP),
    [0x4C] = KEY(KEY_DELETE),
    [0x4D] = KEY(KEY_END),
    [0x4E] = KEY(KEY_PAGEDOWN),
    [0x4F] = KEY(KEY_RIGHT),
    [0x50] = KEY(KEY_LEFT),
    [0x51] = KEY(KEY_DOWN),
    [0x52] = KEY(KEY_UP),
    [0x53] = KEY(KEY_NUMLOCK),
    [0x54] = KEY(KEY_KPSLASH),
    [0x55] = KEY(KEY_KPASTERISK),
    [0x56] = KEY(KEY_KPMINUS),
    [0x57] = KEY(KEY_KPPLUS),
    [0x58] = KEY(KEY_KPENTER),
    [0x59] = KEY(KEY_KP1),
    [0x5A] = KEY(KEY_KP2),
    [0x5B] = KEY(KEY_KP3),
    [0x5C] = KEY(KEY_KP4),
    [0x5D] = KEY(KEY_KP5),
    [0x5E] = KEY(KEY_KP6),
    [0x5F] = KEY(KEY_KP7),
    [0x60] = KEY(KEY_KP8),
    [0x61] = KEY(KEY_KP9),
    [0x62] = KEY(KEY_KP0),
    [0x63] = KEY(KEY_KPDOT),
    [0x64] = KEY(KEY_102ND),
    [0x65] = KEY(KEY_COMPOSE),
    [0x66] = KEY(KEY_POWER),
    [0x67] = KEY(KEY_KPEQUAL),
    [0x68] = KEY(KEY_F13),
    [0x69] = KEY(KEY_F14),
    [0x6A] = KEY(KEY_F15),
    [0x6B] = KEY(KEY_F16),
    [0x6C] = KEY(KEY_F17),
    [0x6D] = KEY(KEY_F18),
    [0x6E] = KEY(KEY_F19),
    [0x6F] = KEY(KEY_F20),
    [0x70] = KEY(KEY_F21),
    [0x71] = KEY(KEY_F22),
    [0x72] = KEY(KEY_F23),
    [0x73] = KEY(KEY_F24),
    [0x74] = KEY(KEY_OPEN),
    [0x75] = KEY(KEY_HELP),
    [0x76] = KEY(KEY_PROPS),
    [0x77] = KEY(KEY_FRONT),
    [0x78] = EMPTY,
    [0x79] = KEY(KEY_AGAIN),
    [0x7A] = KEY(KEY_UNDO),
    [0x7B] = KEY(KEY_CUT),
    [0x7C] = KEY(KEY_COPY),
    [0x7D] = KEY(KEY_PASTE),
    [0x7E] = EMPTY,
    [0x7F] = EMPTY,
    [0x80] = EMPTY,
    [0x81] = EMPTY,
    [0x82] = EMPTY,
    [0x83] = EMPTY,
    [0x84] = EMPTY,
    [0x85] = KEY(KEY_KPCOMMA),
    [0x86] = EMPTY,
    [0x87] = KEY(KEY_RO),
    [0x88] = KEY(KEY_KATAKANAHIRAGANA),
    [0x89] = KEY(KEY_YEN),
    [0x8A] = KEY(KEY_HENKAN),
    [0x8B] = KEY(KEY_MUHENKAN),
    [0x8C] = KEY(KEY_KPJPCOMMA),
    [0x8D] = EMPTY,
    [0x8E] = EMPTY,
    [0x8F] = EMPTY,
    [0x90] = KEY(KEY_HANGEUL),
    [0x91] = KEY(KEY_HANJA),
    [0x92] = KEY(KEY_KATAKANA),
    [0x93] = KEY(KEY_HIRAGANA),
    [0x94] = KEY(KEY_ZENKAKUHANKAKU),
    [0x95] = EMPTY,
    [0x96] = EMPTY,
    [0x97] = EMPTY,
    [0x98] = EMPTY,
    [0x99] = EMPTY,
    [0x9A] = EMPTY,
    [0x9B] = EMPTY,
    [0x9C] = EMPTY,
    [0x9D] = EMPTY,
    [0x9E] = EMPTY,
    [0x9F] = EMPTY,
    [0xA0] = EMPTY,
    [0xA1] = EMPTY,
    [0xA2] = EMPTY,
    [0xA3] = EMPTY,
    [0xA4] = EMPTY,
    [0xA5] = EMPTY,
    [0xA6] = EMPTY,
    [0xA7] = EMPTY,
    [0xA8] = EMPTY,
    [0xA9] = EMPTY,
    [0xAA] = EMPTY,
    [0xAB] = EMPTY,
    [0xAC] = EMPTY,
    [0xAD] = EMPTY,
    [0xAE] = EMPTY,
    [0xAF] = EMPTY,
    [0xB0] = EMPTY,
    [0xB1] = EMPTY,
    [0xB2] = EMPTY,
    [0xB3] = EMPTY,
    [0xB4] = EMPTY,
    [0xB5] = EMPTY,
    [0xB6] = EMPTY,
    [0xB7] = EMPTY,
    [0xB8] = EMPTY,
    [0xB9] = EMPTY,
    [0xBA] = EMPTY,
    [0xBB] = EMPTY,
    [0xBC] = EMPTY,
    [0xBD] = EMPTY,
    [0xBE] = EMPTY,
    [0xBF] = EMPTY,
    [0xC0] = EMPTY,
    [0xC1] = EMPTY,
    [0xC2] = EMPTY,
    [0xC3] = EMPTY,
    [0xC4] = EMPTY,
    [0xC5] = EMPTY,
    [0xC6] = EMPTY,
    [0xC7] = EMPTY,
    [0xC8] = EMPTY,
    [0xC9] = EMPTY,
    [0xCA] = EMPTY,
    [0xCB] = EMPTY,
    [0xCC] = EMPTY,
    [0xCD] = EMPTY,
    [0xCE] = EMPTY,
    [0xCF] = EMPTY,
    [0xD0] = EMPTY,
    [0xD1] = EMPTY,
    [0xD2] = EMPTY,
    [0xD3] = EMPTY,
    [0xD4] = EMPTY,
    [0xD5] = EMPTY,
    [0xD6] = EMPTY,
    [0xD7] = EMPTY,
    [0xD8] = EMPTY,
    [0xD9] = EMPTY,
    [0xDA] = EMPTY,
    [0xDB] = EMPTY,
    [0xDC] = EMPTY,
    [0xDD] = EMPTY,
    [0xDE] = EMPTY,
    [0xDF] = EMPTY,
    [0xE0] = KEY(KEY_LEFTCTRL),
    [0xE1] = KEY(KEY_LEFTSHIFT),
    [0xE2] = KEY(KEY_LEFTALT),
    [0xE3] = KEY(KEY_LEFTMETA),
    [0xE4] = KEY(KEY_RIGHTCTRL),
    [0xE5] = KEY(KEY_RIGHTSHIFT),
    [0xE6] = KEY(KEY_RIGHTALT),
    [0xE7] = KEY(KEY_RIGHTMETA),
    [0xE8] = KEY(KEY_PLAYPAUSE),
    [0xE9] = KEY(KEY_STOPCD),
    [0xEA] = KEY(KEY_PREVIOUSSONG),
    [0xEB] = KEY(KEY_NEXTSONG),
    [0xEC] = KEY(KEY_EJECTCD),
    [0xED] = KEY(KEY_VOLUMEUP),
    [0xEE] = KEY(KEY_VOLUMEDOWN),
    [0xEF] = KEY(KEY_MUTE),
    [0xF0] = KEY(KEY_WWW),
    [0xF1] = KEY(KEY_BACK),
    [0xF2] = KEY(KEY_FORWARD),
    [0xF3] = KEY(KEY_STOP),
    [0xF4] = KEY(KEY_FIND),
    [0xF5] = KEY(KEY_SCROLLUP),
    [0xF6] = KEY(KEY_SCROLLDOWN),
    [0xF7] = KEY(KEY_EDIT),
    [0xF8] = KEY(KEY_SLEEP),
    [0xF9] = KEY(KEY_COFFEE),
    [0xFA] = KEY(KEY_REFRESH),
    [0xFB] = KEY(KEY_CALC),
    [0xFC] = EMPTY,
    [0xFD] = EMPTY,
    [0xFE] = EMPTY,
    [0xFF] = EMPTY,
};

int hid_kbd_map(uint8_t mod, uint8_t code)
{
    const keymap_t *_key = &key_map[code];
    if (0 != code) {
        if (NULL != _key->name)
            printf("kbd[%d%d%d%d%d%d%d%d]: %s (%d)\n",
                (mod >> 7) & 1,
                (mod >> 6) & 1,
                (mod >> 5) & 1,
                (mod >> 4) & 1,
                (mod >> 3) & 1,
                (mod >> 2) & 1,
                (mod >> 1) & 1,
                (mod >> 0) & 1,
                _key->name, _key->value);
        else
            printf("kbd unknown: %02X\n", code);
    }
    return 0;
}

bool HIDServer::keyAvailable() {
    if (_queue) {
        return uxQueueMessagesWaiting(_queue) > 0;
    }
    return true;
}

bool HIDServer::getNextKey(HIDKey & key) {
    bool ret = false;

    if (_queue) {
        ret = xQueueReceive(_queue, &key, msToTicks(HID_SERVER_KEY_TIMEOUT));
    }

    return ret;
}

int HIDServer::checkKeys(HIDKey & key)
{
    int ret = -1;
    if (NULL != _hid_source) {
        int sz = _hid_source->get(_buf,sizeof(_buf));
        if (sz >= 2) {
            if (0xA1 == _buf[0]) {
                if ((0x01 == _buf[1]) && (10 == sz)) { // keyboard
                    // A1 01 00 00 0A 00 00 00 00 00

                    key.mod = _buf[2];
                    key.len = 0;
                    for (int i = 0; i < 6; i++) {
                        uint8_t code = (_buf[4+i]);
                        const keymap_t *_key = &key_map[code];

                        if (_key->value) {
                            key.keys[i] = _key->value;
                            key.len++;
                        } else {
                            key.keys[i] = 0;
                        }
                    }
                    ret = key.len ? 1 : 0;
                }
                else if ((0x02 == _buf[1]) && (6 == sz)) { // mouse
                    // A1 02 00 00 00 00
                    ret = -1;
                }
                else if ((0x03 == _buf[1]) && (11 == sz)) {// gamepad
                    //  0  1  2  3  4  5  6  7  8  9  10
                    // A1 03 0F 7F FF 7F 7F 00 00 00 00
                    // printf("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", _buf[0],_buf[1],_buf[2],_buf[3],_buf[4],_buf[5],_buf[6],_buf[7],_buf[8],_buf[9],_buf[10]);
                    uint16_t gp1 = (*(uint32_t*)&_buf[3]) & 0xffff;
                    uint16_t gp2 = (*(uint32_t*)&_buf[7]) >> 16;

                    uint8_t x = ((gp1 >> 0) & 0xff);
                    uint8_t y = ((gp1 >> 8) & 0xff);

                    key.mod = 0xff;
                    key.len = 3;
                    key.dpad.key = gp2;

                    key.dpad.x = 0;
                    key.dpad.y = 0;

                    if (x == 0)
                        key.dpad.x = -1;
                    else if (x == 0xff)
                        key.dpad.x = 1;

                    if (y == 0)
                        key.dpad.y = -1;
                    else if (y == 0xff)
                        key.dpad.y = 1;

                    ret = (key.dpad.key != 0) || (key.dpad.x != 0) || (key.dpad.y != 0);
                }
                else if ((0x07 == _buf[1]) && (12 == sz)) {  // joystick
                    // A1 07 XX1 YY1 XX1 YY1 88 00 00 00 00 00
                    ret = -1;
                }
            }
        }
    }
    return ret;
}
