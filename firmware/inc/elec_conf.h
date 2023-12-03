/*
 * elec_conf.h -- The electrode configurations.
 *
 *  Created on: 2 Dec 2023
 *      Author: mark
 *   Copyright  2023 Neostim
 */

#ifndef INC_ELEC_CONF_H_
#define INC_ELEC_CONF_H_

// The 25 possible configurations of four electrodes.
typedef enum {
    EC_NONE,
    EC_A_B,
    EC_A_C,
    EC_B_C,
    EC_AB_C,
    EC_B_AC,
    EC_A_BC,
    EC_A_D,
    EC_B_D,
    EC_AB_D,
    EC_C_D,
    EC_AC_D,
    EC_BC_D,
    EC_ABC_D,
    EC_B_AD,
    EC_C_AD,
    EC_BC_AD,
    EC_A_BD,
    EC_C_BD,
    EC_AC_BD,
    EC_C_ABD,
    EC_A_CD,
    EC_B_CD,
    EC_AB_CD,
    EC_B_ACD,
    EC_A_BCD,
    EC_NR_OF_CONFIGS
} ElectrodeConfigNr;

#endif
