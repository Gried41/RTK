#ifndef __HID_SERVER_H
#define __HID_SERVER_H

#include "hid_api.h"

typedef unsigned short U16;
typedef unsigned char  U8;

/*---------------------------------------------------------------------------
 * L2capPsmValue type
 *
 *     Defines well-known protocol service values. These values identify
 *     protocol services that use L2CAP. When creating an L2capPsm structure
 *     for registration with L2CAP_RegisterPsm, the "psm" field must be
 *     set to one of these values.
 *
 *     Dynamically assigned PSM values are also permitted; however, they
 *     must be odd, within the range of 0x1001 to 0xFFFF, and have
 *     the 9th bit (0x0100) set to zero.
 */
 
/* Value for the HID Control Channel */
#define BT_PSM_HID_CTRL         0x0011

/* Value for the HID Interrupt Channel */
#define BT_PSM_HID_INTR         0x0013



/* Group: The following attributes are required to be present in all
 * service records on all Bluetooth devices.
 */

/* A 32-bit UINT that uniquely identifies the service record for a
 * particular SDP server.
 */
#define AID_SERVICE_RECORD_HANDLE               0x0000

/* A Data Element Sequence of UUIDs. Each UUID represents a service
 * class supported by the service record. At least one UUID must
 * be present.
 *
 * The SdpServiceClassUuid type represents these UUIDs.
 */
#define AID_SERVICE_CLASS_ID_LIST               0x0001

/* Group: The following attributes are "universal" to all service records,
 * meaning that the same attribute IDs are always used. However, attributes
 * may or may not be present within a service record.
 *
 * See the Bluetooth Core specification, Service Discovery Protocol (SDP)
 * chapter, section 5.1 for more detailed explanations of these attributes.
 */

#define AID_SERVICE_RECORD_STATE                0x0002
#define AID_SERVICE_ID                          0x0003
#define AID_PROTOCOL_DESC_LIST                  0x0004
#define AID_BROWSE_GROUP_LIST                   0x0005
#define AID_LANG_BASE_ID_LIST                   0x0006
#define AID_SERVICE_INFO_TIME_TO_LIVE           0x0007
#define AID_SERVICE_AVAILABILITY                0x0008
#define AID_BT_PROFILE_DESC_LIST                0x0009
#define AID_DOC_URL                             0x000a
#define AID_CLIENT_EXEC_URL                     0x000b
#define AID_ICON_URL                            0x000c
#define AID_ADDITIONAL_PROT_DESC_LISTS          0x000d

/* Group: The following "universal" attribute IDs must be added to
 * the appropriate value from the AID_LANG_BASE_ID_LIST attribute (usually 
 * 0x0100).
 */
#define AID_SERVICE_NAME                        0x0000
#define AID_SERVICE_DESCRIPTION                 0x0001
#define AID_PROVIDER_NAME                       0x0002

/* Personal Area Networking Profile */
#define AID_IP_SUBNET                           0x0200

/* Group: The following attribute applies only to a service record that
 * corresponds to a BrowseGroupDescriptor service.
 */

/* A UUID used to locate services that are part of the browse group. */
#define AID_GROUP_ID                            0x0200

/* Group: The following attributes apply only to the service record that
 * corresponds to the Service Discovery Server itself. Therefore, they
 * are valid only when the AID_SERVICE_CLASS_ID_LIST contains
 * a UUID of SC_SERVICE_DISCOVERY_SERVER.
 */
#define AID_VERSION_NUMBER_LIST                 0x0200
#define AID_SERVICE_DATABASE_STATE              0x0201

/* Group: The following attributes are for use by specific profiles as
 * defined in the profile specification.
 */
#define AID_SERVICE_VERSION                     0x0300

/* Cordless Telephony Profile */
#define AID_EXTERNAL_NETWORK                    0x0301

/* Synchronization Profile */
#define AID_SUPPORTED_DATA_STORES_LIST          0x0301

/* Fax Class 1 */
#define AID_FAX_CLASS_1_SUPPORT                 0x0302

/* GAP Profile */
#define AID_REMOTE_AUDIO_VOL_CONTROL            0x0302

/* Fax Class 2.0 */
#define AID_FAX_CLASS_20_SUPPORT                0x0303

/* Object Push Profile */
#define AID_SUPPORTED_FORMATS_LIST              0x0303

/* Fax Service Class 2 - Manufacturer specific */
#define AID_FAX_CLASS_2_SUPPORT                 0x0304
#define AID_AUDIO_FEEDBACK_SUPPORT              0x0305

/* Bluetooth as WAP requirements */
#define AID_NETWORK_ADDRESS                     0x0306
#define AID_WAP_GATEWAY                         0x0307
#define AID_HOME_PAGE_URL                       0x0308
#define AID_WAP_STACK_TYPE                      0x0309

/* Personal Area Networking Profile */
#define AID_SECURITY_DESC                       0x030A
#define AID_NET_ACCESS_TYPE                     0x030B
#define AID_MAX_NET_ACCESS_RATE                 0x030C
#define AID_IPV4_SUBNET                         0x030D
#define AID_IPV6_SUBNET                         0x030E

/* Imaging Profile */
#define AID_SUPPORTED_CAPABILITIES              0x0310
#define AID_SUPPORTED_FEATURES                  0x0311
#define AID_SUPPORTED_FUNCTIONS                 0x0312
#define AID_TOTAL_IMAGE_DATA_CAPACITY           0x0313

/* Phonebook Access Profile */
#define AID_SUPPORTED_REPOSITORIES              0x0314

/* Message Access Profile */
#define AID_MAS_INSTANCE_ID                     0x0315
#define AID_SUPPORTED_MESSAGE_TYPES             0x0316

/* Basic Printing Profile */
#define AID_SUPPORTED_DOC_FORMATS               0x0350
#define AID_SUPPORTED_CHAR_REPERTOIRES          0x0352
#define AID_SUPPORTED_XHTML_IMAGE_FORMATS       0x0354
#define AID_COLOR_SUPPORTED                     0x0356
#define AID_PRINTER_1284ID                      0x0358
#define AID_DUPLEX_SUPPORTED                    0x035E
#define AID_SUPPORTED_MEDIA_TYPES               0x0360
#define AID_MAX_MEDIA_WIDTH                     0x0362
#define AID_MAX_MEDIA_LENGTH                    0x0364

/* End of SdpAttributeId */


/*---------------------------------------------------------------------------
 * SdpServiceClassUuid type
 *
 *     Represents the UUID associated with a specific service and
 *     profile.
 *
 *     Any number of these UUIDs may be present in the
 *     AID_SERVICE_CLASS_ID_LIST attribute of a service record, and may
 *     appear in the AID_BT_PROFILE_DESC_LIST.
 */
typedef U16 SdpServiceClassUuid;

/* Service Discovery Server service. */
#define SC_SERVICE_DISCOVERY_SERVER             0x1000

/* Browse Group Descriptor service. */
#define SC_BROWSE_GROUP_DESC                    0x1001

/* Public Browse Group service. */
#define SC_PUBLIC_BROWSE_GROUP                  0x1002

/* Serial Port service and profile. */
#define SC_SERIAL_PORT                          0x1101

/* LAN Access over PPP service. */
#define SC_LAN_ACCESS_PPP                       0x1102

/* Dial-up networking service and profile. */
#define SC_DIALUP_NETWORKING                    0x1103

/* IrMC Sync service and Synchronization profile. */
#define SC_IRMC_SYNC                            0x1104

/* OBEX Object Push service and Object Push profile. */
#define SC_OBEX_OBJECT_PUSH                     0x1105

/* OBEX File Transfer service and File Transfer profile. */
#define SC_OBEX_FILE_TRANSFER                   0x1106

/* IrMC Sync service and Synchronization profile
 * (Sync Command Scenario).
 */
#define SC_IRMC_SYNC_COMMAND                    0x1107

/* Headset service and profile. */
#define SC_HEADSET                              0x1108

/* Cordless telephony service and profile. */
#define SC_CORDLESS_TELEPHONY                   0x1109

/* Audio Source */
#define SC_AUDIO_SOURCE                         0x110A

/* Audio Sink */
#define SC_AUDIO_SINK                           0x110B

/* Audio/Video Remote Control Target */
#define SC_AV_REMOTE_CONTROL_TARGET             0x110C

/* Advanced Audio Distribution Profile */
#define SC_AUDIO_DISTRIBUTION                   0x110D

/* Audio/Video Remote Control */
#define SC_AV_REMOTE_CONTROL                    0x110E

/* Video Conferencing Profile */
#define SC_VIDEO_CONFERENCING                   0x110F

/* Intercom service and profile. */
#define SC_INTERCOM                             0x1110

/* Fax service and profile. */
#define SC_FAX                                  0x1111

/* Headset Audio Gateway */
#define SC_HEADSET_AUDIO_GATEWAY                0x1112

/* WAP service */
#define SC_WAP                                  0x1113

/* WAP client service */
#define SC_WAP_CLIENT                           0x1114

/* Personal Area Networking Profile */
#define SC_PANU                                 0x1115

/* Personal Area Networking Profile */
#define SC_NAP                                  0x1116

/* Personal Area Networking Profile */
#define SC_GN                                   0x1117

/* Basic Printing Profile */
#define SC_DIRECT_PRINTING                      0x1118

/* Basic Printing Profile */
#define SC_REFERENCE_PRINTING                   0x1119
    
/* Imaging Profile */
#define SC_IMAGING                              0x111A

/* Imaging Profile */
#define SC_IMAGING_RESPONDER                    0x111B

/* Imaging Profile */
#define SC_IMAGING_AUTOMATIC_ARCHIVE            0x111C

/* Imaging Profile */
#define SC_IMAGING_REFERENCED_OBJECTS           0x111D

/* Handsfree Profile */
#define SC_HANDSFREE                            0x111E

/* Handsfree Audio Gateway */
#define SC_HANDSFREE_AUDIO_GATEWAY              0x111F

/* Basic Printing Profile */
#define SC_DIRECT_PRINTING_REF_OBJECTS          0x1120

/* Basic Printing Profile */
#define SC_REFLECTED_UI                         0x1121

/* Basic Printing Profile */
#define SC_BASIC_PRINTING                       0x1122

/* Basic Printing Profile */
#define SC_PRINTING_STATUS                      0x1123

/* Human Interface Device Profile */
#define SC_HUMAN_INTERFACE_DEVICE               0x1124

/* Hardcopy Cable Replacement Profile */
#define SC_HCR                                  0x1125

/* Hardcopy Cable Replacement Profile */
#define SC_HCR_PRINT                            0x1126

/* Hardcopy Cable Replacement Profile */
#define SC_HCR_SCAN                             0x1127

/* Common ISDN Access / CAPI Message Transport Protocol */
#define SC_ISDN                                 0x1128

/* Video Conferencing Gateway */
#define SC_VIDEO_CONFERENCING_GW                0x1129

/* Unrestricted Digital Information Mobile Termination */
#define SC_UDI_MT                               0x112A

/* Unrestricted Digital Information Terminal Adapter */
#define SC_UDI_TA                               0x112B

/* Audio Video service */
#define SC_AUDIO_VIDEO                          0x112C

/* SIM Access Profile */
#define SC_SIM_ACCESS                           0x112D

/* Phonebook Access Client */
#define SC_PBAP_CLIENT                          0x112E

/* Phonebook Access Server */
#define SC_PBAP_SERVER                          0x112F

/* Phonebook Access Profile Id */
#define SC_PBAP_PROFILE                         0x1130

/* Message Access Server */
#define SC_MAP_SERVER                           0x1132

/* Message Access Notification Server */
#define SC_MAP_NOTIFY_SERVER                    0x1133

/* Message Access Profile */
#define SC_MAP_PROFILE                          0x1134

/* Plug-n-Play service */
#define SC_PNP_INFO                             0x1200

/* Generic Networking service. */
#define SC_GENERIC_NETWORKING                   0x1201

/* Generic File Transfer service. */
#define SC_GENERIC_FILE_TRANSFER                0x1202

/* Generic Audio service. */
#define SC_GENERIC_AUDIO                        0x1203

/* Generic Telephony service. */
#define SC_GENERIC_TELEPHONY                    0x1204

/* UPnP L2CAP based profile. */
#define SC_UPNP_SERVICE                         0x1205

/* UPnP IP based profile. */
#define SC_UPNP_IP_SERVICE                      0x1206

/* UPnP IP based solution using PAN */
#define SC_ESDP_UPNP_IP_PAN                     0x1300

/* UPnP IP based solution using LAP */
#define SC_ESDP_UPNP_IP_LAP                     0x1301

/* UPnP L2CAP based solution */
#define SC_ESDP_UPNP_L2CAP                      0x1302

/* Video Source */
#define SC_VIDEO_SOURCE                         0x1303

/* Video Sink */
#define SC_VIDEO_SINK                           0x1304

/* Video Sink */
#define SC_VIDEO_DISTRIBUTION                   0x1305

/* End of SdpServiceClassUuid */



/* End of SdpServiceClassUuid */

/*---------------------------------------------------------------------------
 * SdpProtocolUuid type
 *
 *     Represents the UUID associated with a protocol.
 *
 *     Any number of these UUIDs may be present in the
 *     AID_SERVICE_CLASS_ID_LIST attribute of a service record, and may
 *     appear in the AID_BT_PROFILE_DESC_LIST.
 */
typedef U16 SdpProtocolUuid;

/* Service Discovery Protocol */
#define PROT_SDP                     0x0001

/* UDP Protocol */
#define PROT_UDP                     0x0002

/* RFCOMM Protocol */
#define PROT_RFCOMM                  0x0003

/* TCP Protocol */
#define PROT_TCP                     0x0004

/* TCS Binary Protocol */
#define PROT_TCS_BIN                 0x0005

/* TCS-AT Protocol */
#define PROT_TCS_AT                  0x0006

/* OBEX Protocol */
#define PROT_OBEX                    0x0008

/* IP Protocol */
#define PROT_IP                      0x0009

/* FTP Protocol */
#define PROT_FTP                     0x000A

/* HTTP Protocol */
#define PROT_HTTP                    0x000C

/* WSP Protocol */
#define PROT_WSP                     0x000E

/* BNEP Protocol */
#define PROT_BNEP                    0x000F

/* Universal Plug and Play */
#define PROT_UPNP                    0x0010

/* Human Interface Device Profile */
#define PROT_HIDP                    0x0011

/* Hardcopy Cable Replacement Control Channel */
#define PROT_HCR_CONTROL_CHANNEL     0x0012

/* Hardcopy Cable Replacement Data Channel */
#define PROT_HCR_DATA_CHANNEL        0x0014

/* Hardcopy Cable Replacement Notification*/
#define PROT_HCR_NOTIFICATION        0x0016

/* Audio/Video Control Transport Protocol */
#define PROT_AVCTP                   0x0017

/* Audio/Video Distribution Transport Protocol */
#define PROT_AVDTP                   0x0019

/* Audio/Video Control Transport Protocol Browsing Channel*/
#define PROT_AVCTP_BROWSING          0x001B

/* Unrestricted Digital Information Control Plane */
#define PROT_UDI_C                   0x001D

/* L2CAP Protocol */
#define PROT_L2CAP                   0x0100

/* End of SdpProtocolUuid */


/*---------------------------------------------------------------------------
 * SdpDataElemType type
 *
 *     Specifies the type of a Data Element.
 *
 *     Data Elements begin with a single byte that contains both type and
 *     size information. To read the type from this byte, use the
 *     SDP_GetElemType macro.
 *
 *     To create the first byte of a Data Element, bitwise OR the
 *     SdpDataElemType and SdpDataElemSize values into a single byte.
 */

typedef U8 SdpDataElemType;

#define DETD_NIL  0x00 /* Specifies nil, the null type.
                        * Requires a size of DESD_1BYTE, which for this type
                        * means an actual size of 0.
                        */
#define DETD_UINT 0x08 /* Specifies an unsigned integer. Must use size
                        * DESD_1BYTE, DESD_2BYTES, DESD_4BYTES, DESD_8BYTES,
                        * or DESD_16BYTES.
                        */
#define DETD_SINT 0x10 /* Specifies a signed integer. May use size
                        * DESD_1BYTE, DESD_2BYTES, DESD_4BYTES, DESD_8BYTES,
                        * or DESD_16BYTES.
                        */
#define DETD_UUID 0x18 /* Specifies a Universally Unique Identifier (UUID).
                        * Must use size DESD_2BYTES, DESD_4BYTES, or
                        * DESD_16BYTES.
                        */
#define DETD_TEXT 0x20 /* Specifies a text string. Must use sizes
                        * DESD_ADD_8BITS, DESD_ADD_16BITS, or DESD_ADD32_BITS.
                        */
#define DETD_BOOL 0x28 /* Specifies a boolean value. Must use size
                        * DESD_1BYTE.
                        */
#define DETD_SEQ  0x30 /* Specifies a data element sequence. The data contains
                        * a sequence of Data Elements. Must use sizes
                        * DESD_ADD_8BITS, DESD_ADD_16BITS, or DESD_ADD_32BITS.
                        */
#define DETD_ALT  0x38 /* Specifies a data element alternative. The data contains
                        * a sequence of Data Elements. This type is sometimes
                        * used to distinguish between two possible sequences.
                        * Must use size DESD_ADD_8BITS, DESD_ADD_16BITS,
                        * or DESD_ADD_32BITS.
                        */
#define DETD_URL  0x40 /* Specifies a Uniform Resource Locator (URL).
                        * Must use size DESD_ADD_8BITS, DESD_ADD_16BITS,
                        * or DESD_ADD_32BITS.
                        */

#define DETD_MASK 0xf8 /* AND this value with the first byte of a Data
                        * Element to return the element's type.
                        */

/* End of SdpDataElemType */



/*---------------------------------------------------------------------------
 * SdpDataElemSize type
 *
 *     Specifies the size of a Data Element.
 *
 *     Data Elements begin with a single byte that contains both type and
 *     size information. To read the size from this byte, use the
 *     SDP_GetElemSize macro.
 *
 *     To create the first byte of a Data Element, bitwise OR the
 *     SdpDataElemType and SdpDataElemSize values into a single byte.
 *     For example, a standard 16 bit unsigned integer with a value of 0x57
 *     could be encoded as follows:
 * 
 *     U8 val[3] = { DETD_UINT | DESD_2BYTES, 0x00, 0x57 };
 *
 *     The text string "hello" could be encoded as follows:
 *
 *     U8 str[7] = { DETD_TEXT | DESD_ADD_8BITS, 0x05, 'h','e','l','l','o' };
 */
typedef U8 SdpDataElemSize;

#define DESD_1BYTE      0x00 /* Specifies a 1-byte element. However, if
                              * type is DETD_NIL then the size is 0.
                              */
#define DESD_2BYTES     0x01 /* Specifies a 2-byte element. */
#define DESD_4BYTES     0x02 /* Specifies a 4-byte element. */
#define DESD_8BYTES     0x03 /* Specifies an 8-byte element. */
#define DESD_16BYTES    0x04 /* Specifies a 16-byte element. */
#define DESD_ADD_8BITS  0x05 /* The element's actual data size, in bytes,
                              * is contained in the next 8 bits.
                              */
#define DESD_ADD_16BITS 0x06 /* The element's actual data size, in bytes,
                              * is contained in the next 16 bits.
                              */
#define DESD_ADD_32BITS 0x07 /* The element's actual data size, in bytes,
                              * is contained in the next 32 bits.
                              */

#define DESD_MASK       0x07 /* AND this value with the first byte of a Data
                              * Element to return the element's size.
                              */

/* End of SdpDataElemSize */





/****************************************************************************
 *
 * Section: SDP Macros 
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * SDP_ATTRIBUTE()
 *
 *     Macro that formats an SdpAttribute structure using the supplied Attribute ID 
 *     and Attribute value. This macro is very useful when formatting the
 *     SdpAttribute structures used to form the attributes in an SDP Record.
 *
 * Parameters:
 *     attribId - SdpAttributeId value (see the AID_ values).
 *     attrib - Array containing the attribute value.
 */
#define SDP_ATTRIBUTE(attribId, attrib) \
          { attribId,           /* Attribute ID */          \
            sizeof(attrib),     /* Attribute Size */        \
            attrib,             /* Attribute Value */       \
            0x0000 }            /* Flag - For Internal Use */


/*---------------------------------------------------------------------------
 * SDP_ATTRIBUTE_ARM()
 *
 *     Macro that formats an SdpAttribute structure using the supplied Attribute ID 
 *     and Attribute size. This macro is very useful when formatting the
 *     SdpAttribute structures used to form the attributes in an SDP Record.
 *
 *     This macro is used for ARM compilers that cannot set the attribute value
 *     using a pointer to ROM. The pointer must be set later at run-time after
 *     the structure is copied to RAM.
 *
 * Parameters:
 *     attribId - SdpAttributeId value (see the AID_ values).
 *     attrib - Array containing the attribute value (used to set size only.)
 */
#define SDP_ATTRIBUTE_ARM(attribId, attrib) \
          { attribId,           /* Attribute ID */          \
            sizeof(attrib),     /* Attribute Size */        \
            0,                  /* Attribute Value - to be set later */ \
            0x0000 }            /* Flag - For Internal Use */


/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_8BIT()
 *
 *     Macro that forms a Data Element Sequence header using the supplied 8-bit
 *     size value.  Data Element Sequence headers are used in SDP Record Attributes
 *     and SDP Queries. Notice that this macro only forms the header portion
 *     of the Data Element Sequence. The actual Data Elements within this
 *     sequence will need to be formed using other SDP macros.
 *
 * Parameters:
 *     size - 8-bit size of the Data Element Sequence.
 */
#define SDP_ATTRIB_HEADER_8BIT(size) \
            DETD_SEQ + DESD_ADD_8BITS,      /* Type & size index 0x35 */ \
            size                            /* 8-bit size */

/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_16BIT()
 *
 *     Macro that forms a Data Element Sequence header using the supplied 16-bit
 *     size value.  Data Element Sequence headers are used in SDP Record Attributes
 *     and SDP Queries. Notice that this macro only forms the header portion
 *     of the Data Element Sequence. The actual Data Elements within this
 *     sequence will need to be formed using other SDP macros.
 *
 * Parameters:
 *     size - 16-bit size of the Data Element Sequence.
 */
#define SDP_ATTRIB_HEADER_16BIT(size) \
            DETD_SEQ + DESD_ADD_16BITS,      /* Type & size index 0x36 */ \
            (U8)(((size) & 0xff00) >> 8),    /* Bits[15:8] of size */     \
            (U8)((size) & 0x00ff)            /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_32BIT()
 *
 *     Macro that forms a Data Element Sequence header using the supplied 32-bit
 *     size value.  Data Element Sequence headers are used in SDP Record Attributes
 *     and SDP Queries. Notice that this macro only forms the header portion
 *     of the Data Element Sequence. The actual Data Elements within this
 *     sequence will need to be formed using other SDP macros.
 *
 * Parameters:
 *     size - 32-bit size of the Data Element Sequence.
 */
#define SDP_ATTRIB_HEADER_32BIT(size) \
            DETD_SEQ + DESD_ADD_32BITS,         /* Type & size index 0x37 */ \
            (U8)(((size) & 0xff000000) >> 24),  /* Bits[32:24] of size */    \
            (U8)(((size) & 0x00ff0000) >> 16),  /* Bits[23:16] of size */    \
            (U8)(((size) & 0x0000ff00) >> 8),   /* Bits[15:8] of size */     \
            (U8)((size) & 0x000000ff)           /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_ALT_8BIT()
 *
 *     Macro that forms a Data Element Sequence Alternative header using the 
 *     supplied 8-bit size value.  Data Element Sequence Alternative headers 
 *     are used in SDP Record Attributes. Notice that this macro only forms 
 *     the header portion of the Data Element Sequence Alternative. The actual 
 *     Data Element Sequences within this alternative will need to be formed 
 *     using other SDP macros.
 *
 * Parameters:
 *     size - 8-bit size of the Data Element Sequence Alternative.
 */
#define SDP_ATTRIB_HEADER_ALT_8BIT(size) \
            DETD_ALT + DESD_ADD_8BITS,      /* Type & size index 0x35 */ \
            size                            /* 8-bit size */

/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_ALT_16BIT()
 *
 *     Macro that forms a Data Element Sequence Alternative header using the 
 *     supplied 16-bit size value.  Data Element Sequence Alternative headers 
 *     are used in SDP Record Attributes. Notice that this macro only forms 
 *     the header portion of the Data Element Sequence Alternative. The actual 
 *     Data Element Sequences within this alternative will need to be formed 
 *     using other SDP macros.
 *
 * Parameters:
 *     size - 16-bit size of the Data Element Sequence Alternative.
 */
#define SDP_ATTRIB_HEADER_ALT_16BIT(size) \
            DETD_ALT + DESD_ADD_16BITS,      /* Type & size index 0x36 */ \
            (U8)(((size) & 0xff00) >> 8),    /* Bits[15:8] of size */     \
            (U8)((size) & 0x00ff)            /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_ATTRIB_HEADER_ALT_32BIT()
 *
 *     Macro that forms a Data Element Sequence Alternative header using the 
 *     supplied 32-bit size value.  Data Element Sequence Alternative headers 
 *     are used in SDP Record Attributes. Notice that this macro only forms 
 *     the header portion of the Data Element Sequence Alternative. The actual 
 *     Data Element Sequences within this alternative will need to be formed 
 *     using other SDP macros.
 *
 * Parameters:
 *     size - 32-bit size of the Data Element Sequence Alternative.
 */
#define SDP_ATTRIB_HEADER_ALT_32BIT(size) \
            DETD_ALT + DESD_ADD_32BITS,         /* Type & size index 0x37 */ \
            (U8)(((size) & 0xff000000) >> 24),  /* Bits[32:24] of size */    \
            (U8)(((size) & 0x00ff0000) >> 16),  /* Bits[23:16] of size */    \
            (U8)(((size) & 0x0000ff00) >> 8),   /* Bits[15:8] of size */     \
            (U8)((size) & 0x000000ff)           /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_UUID_16BIT()
 *
 *     Macro that forms a UUID Data Element from the supplied 16-bit UUID value. 
 *     UUID data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uuid - 16-bit UUID value (see the SC_ and PROT_ values).
 */
#define SDP_UUID_16BIT(uuid) \
            DETD_UUID + DESD_2BYTES,         /* Type & size index 0x19 */ \
            (U8)(((uuid) & 0xff00) >> 8),    /* Bits[15:8] of UUID */     \
            (U8)((uuid) & 0x00ff)            /* Bits[7:0] of UUID */

/*---------------------------------------------------------------------------
 * SDP_UUID_32BIT()
 *
 *     Macro that forms a UUID Data Element from the supplied 32-bit UUID value. 
 *     UUID data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uuid - 32-bit UUID value (see the SC_ and PROT_ values for 16-bit 
 *            values). 16-bit UUID values can be converted to 32-bit by 
 *            zero extending the 16-bit value.
 */
#define SDP_UUID_32BIT(uuid) \
            DETD_UUID + DESD_4BYTES,            /* Type & size index 0x1A */ \
            (U8)(((uuid) & 0xff000000) >> 24),  /* Bits[32:24] of UUID */    \
            (U8)(((uuid) & 0x00ff0000) >> 16),  /* Bits[23:16] of UUID */    \
            (U8)(((uuid) & 0x0000ff00) >> 8),   /* Bits[15:8] of UUID */     \
            (U8)((uuid) & 0x000000ff)           /* Bits[7:0] of UUID */

/*---------------------------------------------------------------------------
 * SDP_UUID_128BIT()
 *
 *     Macro that forms a UUID Data Element from the supplied 128-bit UUID value. 
 *     UUID data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uuid - 128-bit UUID value (see the SC_ and PROT_ values for 16-bit 
 *            values). 16-bit UUID values can be converted to 128-bit using 
 *            the following conversion: 128_bit_value = 16_bit_value * 2^96 +
 *            Bluetooth Base UUID.
 */
#define SDP_UUID_128BIT(uuid)                /* UUID must be a 16-byte array */ \
            DETD_UUID + DESD_16BYTES,        /* Type & size index 0x1C */ \
            uuid                             /* 128-bit UUID */

/*---------------------------------------------------------------------------
 * SDP_UINT_8BIT()
 *
 *     Macro that forms a UINT Data Element from the supplied 8-bit UINT value. 
 *     UINT data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uint - 8-bit UINT value.
 */
#define SDP_UINT_8BIT(uint) \
            DETD_UINT + DESD_1BYTE,          /* Type & size index 0x08 */ \
            (U8)(uint)                       /* 8-bit UINT */

/*---------------------------------------------------------------------------
 * SDP_UINT_16BIT()
 *
 *     Macro that forms a UINT Data Element from the supplied 16-bit UINT value. 
 *     UINT data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uint - 16-bit UINT value.
 */
#define SDP_UINT_16BIT(uint) \
            DETD_UINT + DESD_2BYTES,         /* Type & size index 0x09 */ \
            (U8)(((uint) & 0xff00) >> 8),    /* Bits[15:8] of UINT */     \
            (U8)((uint) & 0x00ff)            /* Bits[7:0] of UINT */

/*---------------------------------------------------------------------------
 * SDP_UINT_32BIT()
 *
 *     Macro that forms a UINT Data Element from the supplied 32-bit UINT value. 
 *     UINT data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uint - 32-bit UINT value.
 */
#define SDP_UINT_32BIT(uint) \
            DETD_UINT + DESD_4BYTES,            /* Type & size index 0x0A */ \
            (U8)(((uint) & 0xff000000) >> 24),  /* Bits[31:24] of UINT */    \
            (U8)(((uint) & 0x00ff0000) >> 16),  /* Bits[23:16] of UINT */    \
            (U8)(((uint) & 0x0000ff00) >> 8),   /* Bits[15:8] of UINT */     \
            (U8)((uint) & 0x000000ff)           /* Bits[7:0] of UINT */

/*---------------------------------------------------------------------------
 * SDP_UINT_64BIT()
 *
 *     Macro that forms a UINT Data Element from the supplied 64-bit UINT value. 
 *     UINT data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uint - 64-bit UINT value.
 */
#define SDP_UINT_64BIT(uint)                    /* UINT must be an 8-byte array */ \
            DETD_UINT + DESD_8BYTES,            /* Type & size index 0x0B */ \
            uint                                /* 64-bit UINT */

/*---------------------------------------------------------------------------
 * SDP_UINT_128BIT()
 *
 *     Macro that forms a UINT Data Element from the supplied 128-bit UINT value. 
 *     UINT data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     uint - 128-bit UINT value.
 */
#define SDP_UINT_128BIT(uint)                   /* UINT must be a 16-byte array */ \
            DETD_UINT + DESD_16BYTES,           /* Type & size index 0x0C */ \
            uint                                /* 128-bit UINT */

/*---------------------------------------------------------------------------
 * SDP_TEXT_8BIT()
 *
 *     Macro that forms a TEXT Data Element Header from the supplied 8-bit size 
 *     value. TEXT data elements are used in SDP Record Attributes and SDP Queries.
 *     Notice that this macro only forms the header portion of the TEXT Data 
 *     Element. The actual TEXT data within this data element will need to 
 *     be provided separately.
 *
 * Parameters:
 *     size - 8-bit size value.
 */
#define SDP_TEXT_8BIT(size) \
            DETD_TEXT + DESD_ADD_8BITS,      /* Type & size index 0x25 */ \
            (U8)(size)                       /* 8-bit size */

/*---------------------------------------------------------------------------
 * SDP_TEXT_16BIT()
 *
 *     Macro that forms a TEXT Data Element Header from the supplied 16-bit size 
 *     value. TEXT data elements are used in SDP Record Attributes and SDP Queries.
 *     Notice that this macro only forms the header portion of the TEXT Data 
 *     Element. The actual TEXT data within this data element will need to 
 *     be provided separately.
 *
 * Parameters:
 *     size - 16-bit size value.
 */
#define SDP_TEXT_16BIT(size) \
            DETD_TEXT + DESD_ADD_16BITS,      /* Type & size index 0x26 */ \
            (U8)(((size) & 0xff00) >> 8),     /* Bits[15:8] of size */     \
            (U8)((size) & 0x00ff)             /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_TEXT_32BIT()
 *
 *     Macro that forms a TEXT Data Element Header from the supplied 32-bit size 
 *     value. TEXT data elements are used in SDP Record Attributes and SDP Queries.
 *     Notice that this macro only forms the header portion of the TEXT Data 
 *     Element. The actual TEXT data within this data element will need to 
 *     be provided separately.
 *
 * Parameters:
 *     size - 32-bit size value.
 */
#define SDP_TEXT_32BIT(size) \
            DETD_TEXT + DESD_ADD_32BITS,        /* Type & size index 0x27 */ \
            (U8)(((size) & 0xff000000) >> 24),  /* Bits[32:24] of size */    \
            (U8)(((size) & 0x00ff0000) >> 16),  /* Bits[23:16] of size */    \
            (U8)(((size) & 0x0000ff00) >> 8),   /* Bits[15:8] of size */     \
            (U8)((size) & 0x000000ff)           /* Bits[7:0] of size */

/*---------------------------------------------------------------------------
 * SDP_BOOL()
 *
 *     Macro that forms a BOOL Data Element from the supplied 8-bit boolean value. 
 *     BOOL data elements are used in SDP Record Attributes and SDP Queries.
 *
 * Parameters:
 *     value - 8-bit boolean value.
 */
#define SDP_BOOL(value) \
            DETD_BOOL + DESD_1BYTE,          /* Type & size index 0x28 */ \
            (U8)(value)                      /* Boolean value */






/*---------------------------------------------------------------------------
 * HID_NUM_TX_PACKETS constant
 *     Defines the number of transmit packets available for sending control
 *     or interrupt data.
 */
#ifndef HID_NUM_TX_PACKETS
#define HID_NUM_TX_PACKETS 5
#endif

#if HID_NUM_TX_PACKETS > 255
#error HID_NUM_TX_PACKETS must not exceed 255
#endif

/*---------------------------------------------------------------------------
 * HID_NUM_SDP_ATTRIBUTES constant
 *     Defines the number of attributes in the Device's HID SDP database.  The
 *     default value is 24, which is the number of attributes in the default
 *     database entry.  If any SDP attributes are added or removed from
 *     hid_sdp.c, then this number must be modified appropriately.
 */
#ifndef HID_NUM_SDP_ATTRIBUTES
#define HID_NUM_SDP_ATTRIBUTES 24
#endif

/*---------------------------------------------------------------------------
 * DEVICE_ID_NUM_SDP_ATTRIBUTES constant
 *     Defines the number of attributes in the Device's DeviceId SDP database.
 *     The default value is 7, which is the number of attributes in the default
 *     database entry.  If any SDP attributes are added or removed from
 *     hid_sdp.c, then this number must be modified appropriately.
 */
#ifndef DEVICE_ID_NUM_SDP_ATTRIBUTES
#define DEVICE_ID_NUM_SDP_ATTRIBUTES 7
#endif

/*---------------------------------------------------------------------------
 * HID_DEVICE_RELEASE constant
 *     Vendor defined release version number for the device.  The default value
 *     is 0x0100 (1.0).  This information is stored in the SDP databases for the
 *     HID device and the DeviceId.
 */
#ifndef HID_DEVICE_RELEASE
#define HID_DEVICE_RELEASE 0x0100
#endif

/*---------------------------------------------------------------------------
 * HID_PARSER_VERSION constant
 *     Version number for the HID parser for which the device was designed.  
 *     The default value is 0x0111 (v1.11).  This information is stored in the
 *     SDP database of the HID device.
 */
#ifndef HID_PARSER_VERSION
#define HID_PARSER_VERSION 0x0111
#endif
      
/*---------------------------------------------------------------------------
 * HID_DEVICE_SUBCLASS constant
 *     Defines the type of device.  This is equivalent to the low order
 *     8 bytes of the Class of Device/Service Class field.  The default value
 *     is "Unspecified."  This information is stored in the SDP database of the 
 *     HID device.
 */
#ifndef HID_DEVICE_SUBCLASS
#define HID_DEVICE_SUBCLASS   ((U8)(COD_MINOR_PERIPH_UNCLASSIFIED))
#endif
      
/*---------------------------------------------------------------------------
 * HID_COUNTRY_CODE constant
 *     Country code.  The default value is 0x21 (USA).  This information is
 *     stored in the SDP database of the HID device.
 */
#ifndef HID_COUNTRY_CODE
#define HID_COUNTRY_CODE 0x21
#endif
      
/*---------------------------------------------------------------------------
 * HID_VIRTUAL_CABLE constant
 *     Indicates whether the device supports virtual connections.  Devices
 *     which set this value to TRUE indicate that they support a 1:1 bonding
 *     with the host and expect to automatically re-connect if the connection
 *     is dropped.  The default value is TRUE.  This information is stored in
 *     the SDP database of the HID device.
 */
#ifndef HID_VIRTUAL_CABLE
#define HID_VIRTUAL_CABLE TRUE
#endif
      
/*---------------------------------------------------------------------------
 * HID_RECONNECT_INITIATE constant
 *     Indicates that the device will attempt to re-connect to the host if
 *     the connection is dropped.  If set to TRUE, the device will connect
 *     to the host when the connection is dropped.  If set to FALSE, the
 *     device must be in a state to accept a connection from the host.
 *     Establishing a connection and setting the connectability modes is
 *     the responsibility of the application.  The default value is TRUE.  This
 *     information is stored in the SDP database of the HID device.
 */
#ifndef HID_RECONNECT_INITIATE
#define HID_RECONNECT_INITIATE TRUE
#endif
      
/*---------------------------------------------------------------------------
 * HID_DESCRIPTOR_TYPE constant
 *     Defines the type of descriptor defined by HID_DESCRIPTOR. The value
 *     can be either 0x22 (Report) or 0x23 (Physical Descriptor).
 *     A sample descriptor of type "Report" is provided.
 */
#ifndef HID_DESCRIPTOR_TYPE
#define HID_DESCRIPTOR_TYPE 0x22
#endif

/*---------------------------------------------------------------------------
 * HID_DESCRIPTOR_LEN constant
 *     Indicates the length of the descriptor defined in HID_DESCRIPTOR.  
 *     This length must match the number of elements defined in HID_DESCRIPTOR.
 *     A sample length and descriptor are provided.
 */
#ifndef HID_DESCRIPTOR_LEN
#define HID_DESCRIPTOR_LEN 46
#endif

/*---------------------------------------------------------------------------
 * HID_DESCRIPTOR constant
 *     Provides a HID descriptor.  See the explanation of this descriptor in
 *     section 7.11.6 of the Bluetooth HID Profile Specification.  The default
 *     value is a sample descriptor list for a typical mouse.  A comma must
 *     separate each 8-bit element, and no comma should be placed at the very end.
 *     This information is stored in the SDP database of the HID device.
 */
#ifndef HID_DESCRIPTOR
#define HID_DESCRIPTOR \
        0x05, 0x01,     \
        0x09, 0x02,     \
        0xA1, 0x01,     \
        0x09, 0x01,     \
        0xA1, 0x00,     \
        0x05, 0x09,     \
        0x19, 0x01,     \
        0x29, 0x03,     \
        0x15, 0x00,     \
        0x25, 0x01,     \
        0x95, 0x08,     \
        0x75, 0x01,     \
        0x81, 0x02,     \
        0x05, 0x01,     \
        0x09, 0x30,     \
        0x09, 0x31,     \
        0x09, 0x38,     \
        0x15, 0x81,     \
        0x25, 0x7F,     \
        0x75, 0x08,     \
        0x95, 0x03,     \
        0x81, 0x06,     \
        0xC0,           \
        0xC0,           
#endif        

/*---------------------------------------------------------------------------
 * HID_MAX_DESCRIPTOR_LEN constant
 *
 *     Defines maximum storage set aside on the host for each device's
 *     descriptor list. This value is only used when HID_HOST is enabled.
 */
#ifndef HID_MAX_DESCRIPTOR_LEN
#define HID_MAX_DESCRIPTOR_LEN 128
#endif

/*---------------------------------------------------------------------------
 * HID_BATTERY_POWER constant
 *     Indicates if the device is battery powered.  If set to TRUE, the device
 *     is battery powered.  If set to FALSE, the device has continuous power
 *     from a power supply.  The default value is FALSE.  This information is 
 *     stored in the SDP database of the HID device
 */
#ifndef HID_BATTERY_POWER
#define HID_BATTERY_POWER FALSE
#endif
      
/*---------------------------------------------------------------------------
 * HID_REMOTE_WAKE constant
 *     Indicates if the host can wake the device from suspend mode.  If
 *     this value is set to be TRUE, the host can wake the device from
 *     suspend mode by sending the appropriate control message.  If this
 *     value is set to FALSE, the host can exclude this device for the
 *     set of devices that it can wake up.  The default value is TRUE.  The
 *     ability to remotely wake is device (application) specific.  This
 *     information is stored in the SDP database of the HID device.
 */
#ifndef HID_REMOTE_WAKE
#define HID_REMOTE_WAKE TRUE
#endif
      
/*---------------------------------------------------------------------------
 * HID_SUPERVISION_TIMEOUT constant
 *     Defines the recommended supervision timeout for baseband connections.
 *     The default is the default value specified by the Bluetooth specification
 *     (0x7d00).  This information is stored in the SDP database of the HID 
 *     device.
 */
#ifndef HID_SUPERVISION_TIMEOUT
#define HID_SUPERVISION_TIMEOUT 0x7D00
#endif
      
/*---------------------------------------------------------------------------
 * HID_NORMALLY_CONNECTABLE constant
 *     Defines whether the device is normally in page scan mode.  If set to
 *     TRUE, the device is available to receive connections when there is
 *     no active connection.  The default value is TRUE.  Placing the device
 *     in the connectable mode is a function of the application.  This 
 *     information is stored in the SDP database of the HID device.
 */
#ifndef HID_NORMALLY_CONNECTABLE
#define HID_NORMALLY_CONNECTABLE TRUE
#endif
      
/*---------------------------------------------------------------------------
 * HID_BOOT_DEVICE constant
 *     Indicates whether the device supports the boot protocol.  If set to
 *     TRUE, the device supports the HID_TRANS_SET_PROTOCOL and 
 *     HID_TRANS_GET_PROTOCOL transaction.  The default value is TRUE.  Support
 *     for the boot protocol is application specific.  This information is
 *     stored in the SDP database of the HID device.
 */
#ifndef HID_BOOT_DEVICE
#define HID_BOOT_DEVICE TRUE
#endif
      
/*---------------------------------------------------------------------------
 * HID_DEVID_SPEC_ID constant
 *     Version number of the Bluetooth Device ID Profile supported by the 
 *     device.  The default value is 0x0103 (1.3).  This information is stored 
 *     in the SDP database for the DeviceId.
 */
#ifndef HID_DEVID_SPEC_ID
#define HID_DEVID_SPEC_ID 0x0103
#endif

/*---------------------------------------------------------------------------
 * HID_DEVID_VENDOR_ID constant
 *     Unique identifier for the vendor of the device. This value is used in 
 *     conjunction with the HID_DEVID_VENDOR_ID_SRC, which identifies the 
 *     organization that assigned the Vendor ID value. The example value 
 *     shown is 0x23A1, but should be changed for each vendor.  This 
 *     information is stored in the SDP database for the DeviceId.
 */
#ifndef HID_DEVID_VENDOR_ID
#define HID_DEVID_VENDOR_ID 0x23A1
#endif

/*---------------------------------------------------------------------------
 * HID_DEVID_VENDOR_ID_SRC constant
 *     Defines which organization assigned the VendorID value for the device. 
 *     This value is used in conjunction with the HID_DEVID_VENDOR_ID. The
 *     default value is 0x0001 which identifies the Bluetooth SIG as the
 *     organization assigning the Vendor ID value. This information is stored 
 *     in the SDP database for the DeviceId.
 */
#ifndef HID_DEVID_VENDOR_ID_SRC
#define HID_DEVID_VENDOR_ID_SRC 0x0001
#endif

/*---------------------------------------------------------------------------
 * HID_DEVID_PRODUCT_ID constant
 *     Unique identifier for each product used by the vendor. The example 
 *     value shown is 0x1234, but should be changed for each product issued 
 *     by a vendor.This information is stored in the SDP database for the 
 *     DeviceId.
 */
#ifndef HID_DEVID_PRODUCT_ID
#define HID_DEVID_PRODUCT_ID 0x1234
#endif

void HID_RegDevice(HidChannel *Channel, HidCallback Callback);
void HID_RegHost(HidChannel *Channel, HidCallback Callback);
#endif


