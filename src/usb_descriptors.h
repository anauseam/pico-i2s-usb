#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

// Audio IN endpoint for capturing I2S data
#define EPNUM_AUDIO_IN 0x81

// UAC2 Entity IDs — referenced from both src/usb_descriptors.c (as descriptor
// _termid / _unitid / _srcid / _clkid / _assocTerm field values) and from
// src/usb_audio.c (in entityID matching inside tud_audio_*_req_entity_cb).
// Required by docs/internals/03-usb-stack.md.
#define UAC2_ENTITY_INPUT_TERMINAL 0x01
#define UAC2_ENTITY_FEATURE_UNIT 0x02
#define UAC2_ENTITY_OUTPUT_TERMINAL 0x03
#define UAC2_ENTITY_CLOCK_SOURCE 0x04

#endif // USB_DESCRIPTORS_H
