NSString *audio_unit_description_to_NSString(AudioComponentDescription desc)
{
    unichar c[12];
    c[0] = (desc.componentType >> 24) & 0xFF;
    c[1] = (desc.componentType >> 16) & 0xFF;
    c[2] = (desc.componentType >> 8) & 0xFF;
    c[3] = (desc.componentType >> 0) & 0xFF;
    c[4] = (desc.componentSubType >> 24) & 0xFF;
    c[5] = (desc.componentSubType >> 16) & 0xFF;
    c[6] = (desc.componentSubType >> 8) & 0xFF;
    c[7] = (desc.componentSubType >> 0) & 0xFF;
    c[8] = (desc.componentManufacturer >> 24) & 0xFF;
    c[9] = (desc.componentManufacturer >> 16) & 0xFF;
    c[10] = (desc.componentManufacturer >> 8) & 0xFF;
    c[11] = (desc.componentManufacturer >> 0) & 0xFF;

    NSString *string = [NSString stringWithCharacters:c length:12];
    return string;
}

AudioComponentDescription description_string_to_description(const char *code)
{
    OSType type = code[3] | (code[2] << 8) | (code[1] << 16) | (code[0] << 24);
    OSType subType = code[7] | (code[6] << 8) | (code[5] << 16) | (code[4] << 24);
    OSType manufacturer = code[11] | (code[10] << 8) | (code[9] << 16) | (code[8] << 24);

    AudioComponentDescription desc = {
        .componentType = type,
        .componentSubType = subType,
        .componentManufacturer = manufacturer,
    };

    return desc;
}
