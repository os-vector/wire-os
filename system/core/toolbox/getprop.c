/******************************************************************************
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Some portions are from the Android Open Source Project, but did not contain
 * copyright or license information. Not a Contribution.
 *
 * *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <cutils/properties.h>

int main(int argc, char *argv[])
{
    if (argc == 1) {
    //    dump_properties();
    } else {
        char value[PROPERTY_VALUE_MAX];
        char *default_value;
        if(argc > 2) {
            default_value = argv[2];
        } else {
            default_value = "";
        }

        property_get(argv[1], value, default_value);
        printf("%s\n", value);
    }
    return 0;
}
