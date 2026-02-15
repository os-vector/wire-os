/******************************************************************************
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Some portions are from the Android Open Source Project, but did not contain
 * copyright or license information. Not a Contribution.
 *
 * *****************************************************************************/

#include <stdio.h>
#include <cutils/properties.h>

int main(int argc, char *argv[])
{
    if(argc != 3) {
        fprintf(stderr,"usage: setprop <key> <value>\n");
        return 1;
    }

    if(property_set(argv[1], argv[2])){
        fprintf(stderr,"could not set property\n");
        return 1;
    }

    return 0;
}
