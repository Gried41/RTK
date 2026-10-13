#include <stdint.h>
#include <string.h>

#include "app_hid_sdp.h"
#include "btdm_mem.h"

/****************************************************************************
 * HID Specific Attribute IDs
 ****************************************************************************/

#define AID_HID_DEVICE_RELEASE        0x0200
#define AID_HID_PARSER_VERSION        0x0201
#define AID_HID_DEVICE_SUBCLASS       0x0202
#define AID_HID_COUNTRY_CODE          0x0203
#define AID_HID_VIRTUAL_CABLE         0x0204
#define AID_HID_RECONNECT_INIT        0x0205
#define AID_HID_DESCRIPTOR_LIST       0x0206
#define AID_HID_LANG_ID_BASE_LIST     0x0207
#define AID_HID_SDP_DISABLE           0x0208
#define AID_HID_BATTERY_POWER         0x0209
#define AID_HID_REMOTE_WAKE           0x020A
#define AID_HID_PROFILE_VERSION       0x020B
#define AID_HID_SUPERV_TIMEOUT        0x020C
#define AID_HID_NORM_CONNECTABLE      0x020D
#define AID_HID_BOOT_DEVICE           0x020E

/****************************************************************************
 * Device ID Specific Attribute IDs
 ****************************************************************************/

#define AID_DEVID_SPEC_ID             0x0200        /* UINT16 */
#define AID_DEVID_VENDOR_ID           0x0201        /* UINT16 */
#define AID_DEVID_PRODUCT_ID          0x0202        /* UINT16 */
#define AID_DEVID_VERSION             0x0203        /* UINT16 */
#define AID_DEVID_PRIMARY_RECORD      0x0204        /* BOOL */
#define AID_DEVID_VENDOR_ID_SRC       0x0205        /* UINT16 */


/****************************************************************************
 *
 * ROMable data
 *
 ****************************************************************************/

#if HID_DEVICE == XA_ENABLED
/****************************************************************************
 * HID SDP Entries
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * SDP ServiceClassIDList
 */
static const U8 HidSrcClassId[] = {
    SDP_ATTRIB_HEADER_8BIT(3),                  /* Data Element Sequence, 3 bytes */
    SDP_UUID_16BIT(SC_HUMAN_INTERFACE_DEVICE),  /* Human Interface Device */
};

/*---------------------------------------------------------------------------
 * SDP Protocol Descriptor List object registered by HID.
 * 
 * Value of the protocol descriptor list for the HID Profile.
 * This structure is a ROM'able representation of the RAM structure.
 * This structure is copied into a RAM structure used to register the 
 * service.
 */
static const U8 HidProtoDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(13),  /* Data element sequence, 13 bytes */

    /* Each element of the list is a Protocol descriptor which is a
     * data element sequence. The first element is L2CAP which only
     * has a UUID element.
     */
    SDP_ATTRIB_HEADER_8BIT(6),    /* Data element sequence for L2CAP, 6 bytes */     
    SDP_UUID_16BIT(PROT_L2CAP),      /* L2CAP UUID */
    SDP_UINT_16BIT(BT_PSM_HID_CTRL), /* HID PSM */

    /* Next protocol descriptor in the list is HID.
     */
    SDP_ATTRIB_HEADER_8BIT(3),   /* Data element sequence for AVDTP, 2 bytes */
    SDP_UUID_16BIT(PROT_HIDP)    /* HID UUID */
};

/*---------------------------------------------------------------------------
 * SDP AdditionalProtocol Descriptor List object registered by HID.
 * 
 * Value of the additional protocol descriptor list for the HID.
 * This structure is a ROM'able representation of the RAM structure.
 * During HCRP_ServerRegisterSDP, this structure is copied into the RAM 
 * structure and used to register the client or server.
 */
static const U8 HcrpAddProtoDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(15),  /* Data element sequence, 15 bytes  */

    /* Each element of this list is a protocol descriptor list.  For
     * HID, there is only one list.
     */

    SDP_ATTRIB_HEADER_8BIT(13),  /* Data element sequence, 13 bytes */

    /* Each element of the list is a Protocol descriptor which is a
     * data element sequence. The first element is L2CAP which has
     * a UUID element and a PSM.  The second element is HID.
     */
    SDP_ATTRIB_HEADER_8BIT(6),   /* Data element sequence for L2CAP */     
    SDP_UUID_16BIT(PROT_L2CAP),  /* Uuid16 L2CAP                    */
    SDP_UINT_16BIT(BT_PSM_HID_INTR),  /* L2CAP PSM (varies)         */

    /* The next protocol descriptor is for HCRP. It contains one element
     * which is the UUID.
     */
    SDP_ATTRIB_HEADER_8BIT(3),   /* Data element sequence for HCRP Control */
    SDP_UUID_16BIT(PROT_HIDP)    /* Uuid16 HCRP Channel (varies)           */
};

/*---------------------------------------------------------------------------
 * SDP Language Base Attribute ID List.  
 *
 * Only defines the English language.  This must be modified if other language
 * support is required.
 */
static const U8 HidLangBaseAttrIdList[] = {
    SDP_ATTRIB_HEADER_8BIT(9),

    /* Elements of this list occur in triplets that describe the Language
     * Base Attribute ID.
     */
    SDP_UINT_16BIT(0x656e),      /* English "en" */
    SDP_UINT_16BIT(0x006a),      /* UTF-8 endoding */
    SDP_UINT_16BIT(0x0100)       /* Primary Language Base ID */

    /* Additional languages may be defined.  Each language is defined by a
     * "triplet" of 16-bit elements.
     * 
     * The first element of the triplet defines the language.  The language is
     * encoded according to ISO 639:1988 (E/F):  "Code for the representation
     * of names of languages."  
     *
     * The second element describes the character encoding.  Values for
     * character encoding can be found in IANA's database2, and have the values
     * that are referred to as MIBEnum values. The recommended character
     * encoding is UTF-8.
     *
     * The third element of each triplet contains an attribute ID that serves as
     * the base attribute ID for the natural language in the service record.
     *
     * The length of this attribute must be modified to include the length
     * of any additional triplets.
     */
};

/*---------------------------------------------------------------------------
 * HID Language ID Base List.
 *
 * Defines how Bluetooth strings are mapped to HID LANGID and string 
 * indices.  For a complete description, see section 7.11.7 of the
 * Bluetooth HID Profile specification.  The default value is a sample
 * that supports only United States English.
 */
static const U8 HidLangIdBaseList[] = {
    SDP_ATTRIB_HEADER_8BIT(8),   /* Data element sequence */

    /* Each element of the list is a data element sequence describing a HID
     * language ID base.  Only one element is included.
     */
    SDP_ATTRIB_HEADER_8BIT(6),   /* Data element sequence. */
    SDP_UINT_16BIT(0x0409),      /* Language = English (United States) */
    SDP_UINT_16BIT(0x0100)       /* Bluetooth String Offset */

    /* Additional languages may be defined.  Each language is defined by
     * a data element sequence with 2 elements.
     *
     * The first element defines the language and is encoded according to the
     * the "Universal Serial Bus Language Identifiers (LANGIDs)" specification.
     *
     * The second element defines the Bluetooth base attribute ID as is defined
     * in the Language Base Attribute ID List (see HidLangBaseAttrIdList above).
     *
     * Headers must be added along with the actual element (HID Language ID
     * base) data.  Also,  the length in the first header must be adjusted.
     */
};

/*---------------------------------------------------------------------------
 * SDP Public Browse Group.
 */
static const U8 HidBrowseGroup[] = {
    SDP_ATTRIB_HEADER_8BIT(3),               /* 3 bytes */
    SDP_UUID_16BIT(SC_PUBLIC_BROWSE_GROUP)   /* Public Browse Group */
};

/*---------------------------------------------------------------------------
 * SDP Profile Descriptor List
 */
static const U8 HidProfileDescList[] = {
    SDP_ATTRIB_HEADER_8BIT(8),   /* Data element sequence, 8 bytes */
    SDP_ATTRIB_HEADER_8BIT(6),   /* Data element sequence for ProfileDescriptor,
                                  * 6 bytes.
                                  */
    SDP_UUID_16BIT(SC_HUMAN_INTERFACE_DEVICE),
    SDP_UINT_16BIT(0x0100)
};

/*---------------------------------------------------------------------------
 * Device Release Number.
 */
static const U8 HidDeviceRelease[] = {
    SDP_UINT_16BIT(HID_DEVICE_RELEASE)
};

/*---------------------------------------------------------------------------
 * Parser Version.
 */
static const U8 HidParserVersion[] = {
    SDP_UINT_16BIT(HID_PARSER_VERSION)
};

/*---------------------------------------------------------------------------
 * Device Subclass.
 */
static const U8 HidDeviceSubclass[] = {
    SDP_UINT_8BIT(HID_DEVICE_SUBCLASS)
};

/*---------------------------------------------------------------------------
 * Country Code.
 */
static const U8 HidCountrycode[] = {
    SDP_UINT_8BIT(HID_COUNTRY_CODE)
};

/*---------------------------------------------------------------------------
 * Virtual Cable.
 */
static const U8 HidVirtualCable[] = {
    SDP_BOOL(HID_VIRTUAL_CABLE)
};

/*---------------------------------------------------------------------------
 * Initiate Reconnect.
 */
static const U8 HidReconnectInitiate[] = {
    SDP_BOOL(HID_RECONNECT_INITIATE)
};

/*---------------------------------------------------------------------------
 * Descriptor List.
 */
static const U8 HidDescriptorList[] = {
    SDP_ATTRIB_HEADER_8BIT(HID_DESCRIPTOR_LEN + 6), /* Data element sequence */

    /* Each element of the list is a HID descriptor which is a
     * data element sequence.
     */
    SDP_ATTRIB_HEADER_8BIT(HID_DESCRIPTOR_LEN + 4),
    SDP_UINT_8BIT(HID_DESCRIPTOR_TYPE),  /* Report Descriptor Type */

    /* Length of the HID descriptor */
    SDP_TEXT_8BIT(HID_DESCRIPTOR_LEN),

    /* The actual descriptor is defined in hid.h or in overide.h */
    HID_DESCRIPTOR

    /* Addition descriptors may be added, but the header must be added
     * along with the actual descriptor data.  Also, all lengths in the 
     * headers above must be adjusted.
     */
};

/*---------------------------------------------------------------------------
 * Profile Version
 */
static const U8 HidProfileVersion[] = {
    SDP_UINT_16BIT(0x0100)
};

/*---------------------------------------------------------------------------
 * Boot Device
 */
static const U8 HidBootDevice[] = {
    SDP_BOOL(HID_BOOT_DEVICE)
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  ServiceName
 *
 * This is the English string.  Other languguages can be defined.
 */
static const U8 HidServiceName[] = {
    SDP_TEXT_8BIT(15),          /* Null terminated text string */
    'X', 'T', 'N', 'D', 'A', 'c', 'c', 'e', 's', 's', ' ', 
    'H', 'I', 'D', '\0'
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  ServiceDescription
 *
 * This is the English string.  Other languguages can be defined.
 */
static const U8 HidServiceDescription[] = {
    SDP_TEXT_8BIT(18),          /* Null terminated text string */
    'X', 'T', 'N', 'D', 'A', 'c', 'c', 'e', 's', 's', ' ', 
    'D', 'e', 'v', 'i', 'c', 'e', '\0'
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  ProviderName
 *
 * This is the English string.  Other languguages can be defined.
 */
static const U8 HidProviderName[] = {
    SDP_TEXT_8BIT(17),          /* Null terminated text string */
    'E', 'x', 't', 'e', 'n', 'd', 'e', 'd', ' ', 'S', 'y', 
    's', 't', 'e', 'm', 's', '\0'
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  HidSdpDisable
 */
static const U8 HidSdpDisable[] = {
    SDP_BOOL(FALSE)
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  HidBatteryPower
 */
static const U8 HidBatteryPower[] = {
    SDP_BOOL(HID_BATTERY_POWER)
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  HidRemoteWake
 */
static const U8 HidRemoteWake[] = {
    SDP_BOOL(HID_REMOTE_WAKE)
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  HidSupervisionTimeout
 */
static const U8 HidSupervisionTimeout[] = {
    SDP_UINT_16BIT(HID_SUPERVISION_TIMEOUT)
};

/*---------------------------------------------------------------------------
 * * OPTIONAL *  HidNormallyConnectable
 */
static const U8 HidNormallyConnectable[] = {
    SDP_BOOL(HID_NORMALLY_CONNECTABLE)
};

/*---------------------------------------------------------------------------
 * HID attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * HID SDP record.
 */
static SdpAttribute HidSdpAttributes[] = {
    
    /* Mandatory SDP Attributes */

    /* HID class ID List attribute */
    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, HidSrcClassId), 
    /* HID protocol descriptor list attribute */
    SDP_ATTRIBUTE(AID_PROTOCOL_DESC_LIST, HidProtoDescList),
    /* Public Browse Group Service */
    SDP_ATTRIBUTE(AID_BROWSE_GROUP_LIST, HidBrowseGroup), 
    /* Language Base ID List */
    SDP_ATTRIBUTE(AID_LANG_BASE_ID_LIST, HidLangBaseAttrIdList),
    /* HID profile descriptor list attribute */
    SDP_ATTRIBUTE(AID_BT_PROFILE_DESC_LIST, HidProfileDescList),
    /* HID additional protocol descriptor list attribute */
    SDP_ATTRIBUTE(AID_ADDITIONAL_PROT_DESC_LISTS, HcrpAddProtoDescList),

    /* Optional Human readable attributes.  The strings provided are English.
     * Other languages can be added.  Each language should have a different
     * Language Base Attribute ID (defined in HidLangBaseAttrIdList).  This ID
     * is added to the universal attribute ID for Service Name, Service
     * Description, and Provider Name.
     */

    /* HID Service Name in English */
    SDP_ATTRIBUTE((AID_SERVICE_NAME + 0x0100), HidServiceName),
    /* HID Service Description in English */
    SDP_ATTRIBUTE((AID_SERVICE_DESCRIPTION + 0x0100), HidServiceDescription),
    /* HID Provider Name in English*/
    SDP_ATTRIBUTE((AID_PROVIDER_NAME + 0x0100), HidProviderName),

    /* Mandatory HID attributes */

    /* Device release number */
    SDP_ATTRIBUTE(AID_HID_DEVICE_RELEASE, HidDeviceRelease),
    /* HID parser version */
    SDP_ATTRIBUTE(AID_HID_PARSER_VERSION, HidParserVersion),
    /* Device subclass */
    SDP_ATTRIBUTE(AID_HID_DEVICE_SUBCLASS, HidDeviceSubclass),
    /* Country Code */
    SDP_ATTRIBUTE(AID_HID_COUNTRY_CODE, HidCountrycode),
    /* Virtual Cable */
    SDP_ATTRIBUTE(AID_HID_VIRTUAL_CABLE, HidVirtualCable),
    /* Device initiates reconnect */
    SDP_ATTRIBUTE(AID_HID_RECONNECT_INIT, HidReconnectInitiate),
    /* HID descriptor list */
    SDP_ATTRIBUTE(AID_HID_DESCRIPTOR_LIST, HidDescriptorList),
    /* Language ID Base List */
    SDP_ATTRIBUTE(AID_HID_LANG_ID_BASE_LIST, HidLangIdBaseList),

    /* Optional HID attibutes */

    /* SDP Disable/Enable */
    SDP_ATTRIBUTE(AID_HID_SDP_DISABLE, HidSdpDisable),
    /* Battery powered */
    SDP_ATTRIBUTE(AID_HID_BATTERY_POWER, HidBatteryPower),
    /* Device support of remote wakeup */
    SDP_ATTRIBUTE(AID_HID_REMOTE_WAKE, HidRemoteWake),

    /* Mandatory HID attribute */

    /* HID profile version*/
    SDP_ATTRIBUTE(AID_HID_PROFILE_VERSION, HidProfileVersion),

    /* Optional HID attributes */

    /* Recommended supervision timeout */
    SDP_ATTRIBUTE(AID_HID_SUPERV_TIMEOUT, HidSupervisionTimeout),
    /* Device connectability */
    SDP_ATTRIBUTE(AID_HID_NORM_CONNECTABLE, HidNormallyConnectable),

    /* Mandatory HID attribute */

    /* Support for boot protocol */
    SDP_ATTRIBUTE(AID_HID_BOOT_DEVICE, HidBootDevice),
};

/*---------------------------------------------------------------------------
 * ServiceClassIDList
 */
static const U8 DeviceIdSrcClassId[] = {
    SDP_ATTRIB_HEADER_8BIT(3),          /* Data Element Sequence, 3 bytes */
    SDP_UUID_16BIT(SC_PNP_INFO),        /* Plug and Play Information */
};

/*---------------------------------------------------------------------------
 * Specification Id
 */
static const U8 DeviceIdSpecId[] = {
    SDP_UINT_16BIT(HID_DEVID_SPEC_ID)
};

/*---------------------------------------------------------------------------
 * Vendor Id
 */
static const U8 DeviceIdVendorId[] = {
    SDP_UINT_16BIT(HID_DEVID_VENDOR_ID)
};

/*---------------------------------------------------------------------------
 * Product Id
 */
static const U8 DeviceIdProductId[] = {
    SDP_UINT_16BIT(HID_DEVID_PRODUCT_ID)
};

/*---------------------------------------------------------------------------
 * Version
 */
static const U8 DeviceIdVersion[] = {
    SDP_UINT_16BIT(HID_DEVICE_RELEASE)
};

/*---------------------------------------------------------------------------
 * Primary Record
 */
static const U8 DeviceIdPrimaryRec[] = {
    SDP_BOOL(TRUE)
};

/*---------------------------------------------------------------------------
 * Vendor ID Source
 */
static const U8 DeviceIdVendorIdSrc[] = {
    SDP_UINT_16BIT(HID_DEVID_VENDOR_ID_SRC)
};

/*---------------------------------------------------------------------------
 * Device ID attributes.
 *
 * This is a ROM template for the RAM structure used to register the
 * Device ID SDP record.
 */
static SdpAttribute DeviceIdSdpAttributes[] = {
    
    /* Mandatory SDP Attributes */

    /* PNP class ID List attribute */
    SDP_ATTRIBUTE(AID_SERVICE_CLASS_ID_LIST, DeviceIdSrcClassId), 
    /* Device ID Specification ID attribute */
    SDP_ATTRIBUTE(AID_DEVID_SPEC_ID, DeviceIdSpecId),
    /* Device ID Vendor ID attribute */
    SDP_ATTRIBUTE(AID_DEVID_VENDOR_ID, DeviceIdVendorId), 
    /* Device ID Product ID */
    SDP_ATTRIBUTE(AID_DEVID_PRODUCT_ID, DeviceIdProductId),
    /* Device ID Version */
    SDP_ATTRIBUTE(AID_DEVID_VERSION, DeviceIdVersion),
    /* Device ID Primary Record */
    SDP_ATTRIBUTE(AID_DEVID_PRIMARY_RECORD, DeviceIdPrimaryRec),
    /* Device ID Vendor ID Source attribute */
    SDP_ATTRIBUTE(AID_DEVID_VENDOR_ID_SRC, DeviceIdVendorIdSrc), 
};

#if HID_HOST == XA_ENABLED
/****************************************************************************
 * HOST SDP Queries
 ****************************************************************************/

/*-------------------------------------------------------------------------
 *
 * SDP query info
 */
static const U8 HidServiceSearchAttribReq[] = {
    /* First parameter is the search pattern in data element format. It
     * is a list of 3 UUIDs. 
     */
    SDP_ATTRIB_HEADER_8BIT(9),           /* Data Element Sequence, 9 bytes */ 

    SDP_UUID_16BIT(SC_HUMAN_INTERFACE_DEVICE),  /* UUID for HID service    */
    SDP_UUID_16BIT(PROT_L2CAP),          /* L2CAP UUID                     */
    SDP_UUID_16BIT(PROT_HIDP),           /* HID profile UUID               */

    /* The second parameter is the maximum number of bytes that can be
     * be received for the attribute list
     */
    0x00, 0x64,                /* Max number of bytes for attribute is 100 */

    /* The third parameter is the actual attribute ID list which is a data
     * element sequence. 
     */
    SDP_ATTRIB_HEADER_8BIT(5),         /* Data Element Sequence, 45 bytes */
    0x0A, 0x00, 0x00, 0xFF, 0xFF       /* Return all records              */
};
#endif
#endif


void HID_RegDevice(HidChannel *Channel, HidCallback Callback)
{
    sdp_AttTable_Info_t sdp_AttTable_Info;
    
    sdp_AttTable_Info.att_table_size = sizeof(HidSdpAttributes);
    sdp_AttTable_Info.deviceId_size  = sizeof(DeviceIdSdpAttributes);
    sdp_AttTable_Info.sdpatt_table   = (void *)HidSdpAttributes;
    sdp_AttTable_Info.sdpdeviceId_table = (void *)DeviceIdSdpAttributes;

    HID_RegisterDevice(Channel, Callback, &sdp_AttTable_Info);
}

void HID_RegHost(HidChannel *Channel, HidCallback Callback)
{
//    sdp_AttTable_Info_t sdp_AttTable_Info;
//    
//    sdp_AttTable_Info.att_table_size = sizeof(HidSdpAttributes);
//    sdp_AttTable_Info.deviceId_size  = sizeof(DeviceIdSdpAttributes);
//    sdp_AttTable_Info.sdpatt_table   = (void *)HidSdpAttributes;
//    sdp_AttTable_Info.sdpdeviceId_table = (void *)DeviceIdSdpAttributes;

    HID_RegisterHost(Channel, Callback);
}

BtStatus bt_hid_send_data(HidChannel *Channel,uint8_t *data, uint8_t data_len)
{
    
	HidInterrupt *Interrupt = (HidInterrupt*)btdm_malloc(sizeof(HidInterrupt));
            
    Interrupt->reportType = HID_REPORT_INPUT;
    Interrupt->dataLen = data_len;
    Interrupt->data = (uint8_t *)btdm_malloc(Interrupt->dataLen);
//    printf("--> %x ,%x\r\n",Interrupt,Interrupt->data);
    memcpy(Interrupt->data, data, data_len);
//    fputc('S',NULL);
    return HID_SendInterrupt(Channel, Interrupt);
}